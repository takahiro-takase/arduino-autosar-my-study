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
 *                → Can_SetControllerMode(CAN_T_SLEEP) → CanSM_State: NO_COM
 *
 *          2026-08 の設計変更（Nm↔CanSM↔ComM 協調スリープ通知を ComM 経由へ
 *          移管）により、CanSM が NO_COM を受け取るのは「ComM が Nm の協調
 *          スリープ完了（Bus-Sleep Mode 到達）を確認した後」の 1 回だけになった。
 *          そのため CanSM 自身はもう「解放要求はされたがまだ寝てはいけない」
 *          という中間状態を持たず、FULL_COM から物理スリープまで一気に遷移する
 *          （旧テストにあった CanSM_NmBusSleepMode() 経由の 2 段階シーケンスは
 *          廃止された）。ComM 側の新しい協調スリープロジック
 *          （Nm_NetworkRelease() の遅延送信、再要求キャンセル、
 *          ComM_Nm_BusSleepMode()、Bus-Off 中の解放ペンディング処理）は
 *          `[env:native_sleep_chain]` の Bsw_SleepCoordination_test.cpp が
 *          ComM.c/Nm.c/CanSM.c 実体を使って検証する（そちらを参照）。
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
        CanSM_Init(NULL);
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

TEST_F(Bsw_SleepChain_Test, RequestComMode_NoCom_OK_PutsControllerToPhysicalSleepImmediately)
{
    /* 準備 (Arrange): FULL_COM から開始する */
    ArrangeFullCom();

    /* 実行 (Act) */
    Std_ReturnType ret = CanSM_RequestComMode(0U, COMM_NO_COMMUNICATION);

    /* 評価 (Assert): 中間状態を経ず、この1回の呼び出しで物理スリープまで完了する
     * （ComM は Nm の協調スリープ完了を確認済みでここへ辿り着く前提のため）。 */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_SLEEP);
    EXPECT_EQ(FakeCanHw_LastMode, CAN_HW_MODE_SLEEP);
    EXPECT_EQ(FakeComM_BusSMIndicationCount, 1U);
    EXPECT_EQ(FakeComM_LastNetwork, 0U);
    EXPECT_EQ(FakeComM_LastMode, static_cast<ComM_ModeType>(COMM_NO_COMMUNICATION));
}

}  // namespace
