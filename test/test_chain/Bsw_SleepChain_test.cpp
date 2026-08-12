/**
 * \file    Bsw_SleepChain_test.cpp
 * \brief   README.md「CAN コントローラの実スリープ」コールチェーンの
 *          単体テスト（GoogleTest / PlatformIO `[env:native_chain]`）。
 *
 * \details 本テストは README の以下のコールチェーンをそのまま実行して確認する：
 *
 *              CanSM_RequestComMode(NO_COM)   ← ComM から呼ばれる（本テストでは
 *                                                 ComM 自体はフェイクに差し替え、
 *                                                 直接呼び出す）
 *                → CanSM_State: FULL_COM → NO_COM_PENDING_SLEEP
 *                  （ここではまだ Can_SetControllerMode(CAN_T_SLEEP) を呼ばない。
 *                   Nm が Bus-Sleep Mode へ到達するまで HW は稼働継続する）
 *                ┊  (Nm の協調スリープ待ち。本テストでは Nm 自体を対象にせず、
 *                ┊   「Nm が Bus-Sleep Mode へ到達した」ことを CanSM_NmBusSleepMode()
 *                ┊   の直接呼び出しで模擬する)
 *              CanSM_NmBusSleepMode()
 *                → Can_SetControllerMode(CAN_T_SLEEP) → CanSM_State: NO_COM
 *
 *          「┊」（Nm の協調スリープという非同期の切れ目）を境に、Bsw_TxChain_test.cpp
 *          と同じ発想でテストを分けている。Nm 自身の状態機械（Ready Sleep →
 *          Prepare Bus-Sleep → Bus-Sleep Mode、Nm.c 参照）はここでは対象外。
 *
 *          ComM（CanSM_RequestComMode の呼び出し元、CanSM が呼び返す
 *          ComM_BusSMIndication の宛先）と Dem（Bus-Off 通信路の PASSED/FAILED
 *          報告先）は境界としてフェイクに差し替える（Bsw_ComM_fake.h /
 *          Bsw_Dem_fake.h 冒頭コメント参照）。CanIf は本ファイルでは使わない
 *          （ウェイクアップ側の Bsw_WakeupChain_test.cpp 参照）が、CanSM.h が
 *          include する ComM.h の型定義だけを使う。
 *
 *          Bsw_TxChain_test.cpp/Bsw_RxChain_test.cpp（README「Tx/Rx処理」の
 *          コールチェーン）と同じ `[env:native_chain]` 上で CanSM.c の実体を
 *          共有する（CanIf.c が CanSM_RxIndication() 等をハードコードで
 *          呼ぶため、どのみち CanSM は実体かフェイクかのいずれかをそのバイナリ
 *          全体で選ぶ必要がある。詳細は Bsw_TxChain_test.cpp 冒頭コメント参照）。
 */
#include <gtest/gtest.h>

extern "C" {
#include "CanSM.h"
#include "Can.h"
#include "Can_Hw.h"
#include "Hal_Can_Hw_fake.h"
#include "Bsw_ComM_fake.h"
#include "Bsw_Dem_fake.h"
#include "Hal_Det_Hw_fake.h"
}

namespace
{

class Bsw_SleepChain_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeCanHw_Reset();
        FakeComM_Reset();
        FakeDem_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制

        canConfig.filter.filterId = 0x0220U;
        canConfig.filter.mask     = 0x1FFFU;
        canConfig.csPin           = 10U;
        canConfig.intPin          = 2U;
        canConfig.baudrate        = 500000U;
        canConfig.crystalFreq     = CAN_CRYSTAL_16MHZ;

        Can_Init(&canConfig);
        CanSM_Init();
        // Can_Init() 自体が Can_Hw_SetMode(LISTEN_ONLY) を1回呼ぶため、
        // 各テストの Act 区間の SetMode 呼び出し回数を 0 から数えられるよう
        // ここでリセットする。
        FakeCanHw_SetModeCount = 0U;

        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;  // DeInit() のログを抑制
        CanSM_DeInit();
    }

    /** FULL_COM 状態を Arrange するヘルパー。ComM/Dem/Can_Hw スパイの記録は
     *  呼び出し後にリセットし、各テストの Act 区間だけを対象にする。 */
    void ArrangeFullCom()
    {
        ASSERT_EQ(CanSM_RequestComMode(0U, COMM_FULL_COMMUNICATION), E_OK);
        ASSERT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);
        FakeComM_Reset();
        FakeDem_Reset();
        FakeCanHw_SetModeCount = 0U;
    }

    Can_ConfigType canConfig;
};

// ------------------------------------------------------------
// セグメント①: CanSM_RequestComMode(NO_COM) ─ Nm 協調スリープ待ちで切れるまで
// ------------------------------------------------------------
TEST_F(Bsw_SleepChain_Test, RequestComMode_NoCom_OK_TransitionsToPendingSleepWithoutPhysicalSleep)
{
    /* 準備 (Arrange): FULL_COM から開始する（CanSM_RequestComMode(NO_COM)が
     * NO_COM_PENDING_SLEEP へ遷移するのは FULL_COM 発の場合のみ） */
    ArrangeFullCom();

    /* 実行 (Act) */
    Std_ReturnType ret = CanSM_RequestComMode(0U, COMM_NO_COMMUNICATION);

    /* 評価 (Assert) */
    EXPECT_EQ(ret, E_OK);
    // 物理スリープはまだ行わない（Nm が Bus-Sleep Mode へ到達するまで HW 稼働継続）
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);
    EXPECT_EQ(FakeCanHw_SetModeCount, 0U);
    // ComM へは NO_COMMUNICATION を通知済み
    EXPECT_EQ(FakeComM_BusSMIndicationCount, 1U);
    EXPECT_EQ(FakeComM_LastNetwork, 0U);
    EXPECT_EQ(FakeComM_LastMode, static_cast<ComM_ModeType>(COMM_NO_COMMUNICATION));
}

// ------------------------------------------------------------
// セグメント②: CanSM_NmBusSleepMode() ─ Nm 到達通知から実スリープまで
// ------------------------------------------------------------
TEST_F(Bsw_SleepChain_Test, NmBusSleepMode_OK_PutsControllerToPhysicalSleep)
{
    /* 準備 (Arrange): セグメント①の終端状態（NO_COM_PENDING_SLEEP）を用意する */
    ArrangeFullCom();
    ASSERT_EQ(CanSM_RequestComMode(0U, COMM_NO_COMMUNICATION), E_OK);
    ASSERT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);
    FakeComM_Reset();
    FakeDem_Reset();

    /* 実行 (Act): Nm が Bus-Sleep Mode へ到達したことを模擬する */
    CanSM_NmBusSleepMode();

    /* 評価 (Assert) */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_SLEEP);
    EXPECT_EQ(FakeCanHw_LastMode, CAN_HW_MODE_SLEEP);
    // CanSM_NmBusSleepMode() 自体は ComM/Dem のいずれも呼ばない
    EXPECT_EQ(FakeComM_BusSMIndicationCount, 0U);
    EXPECT_EQ(FakeDem_ReportErrorStatusCount, 0U);
}

TEST_F(Bsw_SleepChain_Test, NmBusSleepMode_NG_IgnoredWhenNotPendingSleep)
{
    /* 準備 (Arrange): NO_COM_PENDING_SLEEP を経ていない（起動直後の NO_COM のまま） */

    /* 実行 (Act) */
    CanSM_NmBusSleepMode();

    /* 評価 (Assert): 物理スリープへは遷移しない（Can_SetControllerMode すら呼ばれない） */
    EXPECT_EQ(FakeCanHw_SetModeCount, 0U);
    EXPECT_EQ(FakeComM_BusSMIndicationCount, 0U);
}

}  // namespace
