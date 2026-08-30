/**
 * \file    Bsw_WakeupChain_test.cpp
 * \brief   README.md「ウェイクアップ検出とウェイクアップ検証」コールチェーンの
 *          単体テスト（GoogleTest / PlatformIO `[env:native_chain]`）。
 *
 * \details 本テストは README の以下のコールチェーンをそのまま実行して確認する：
 *
 *              Can_MainFunction_Wakeup()（SLEEP 中、INT ピンのウェイクアップ
 *                                          要因を検出。本テストでは
 *                                          Can_Hw_IsWakeupPending() のフォール
 *                                          バックポーリング経路を使う）
 *                → CanIf_ControllerWakeup()
 *                  → CanSM_ControllerWakeup()
 *                    → Can_SetControllerMode(CAN_T_WAKEUP)   ← SLEEP→STOPPED
 *                                                                (Listen-Only)
 *                      CanSM_State: NO_COM → WAKEUP_VALIDATING
 *                      （ComM/EcuM へはまだ通知しない）
 *
 *          ここから 2 通りの分岐がある（それぞれ独立したテストとして検証する）：
 *
 *            2a. 検証成功: Can_MainFunction_Read() が受信フレームをドレイン
 *                → CanIf_RxIndication() → CanSM_RxIndication()
 *                  → Can_SetControllerMode(CAN_T_START) → CanSM_State: FULL_COM
 *                    → ComM_BusSM_ModeIndication(FULL_COM)
 *
 *            2b. 検証失敗（タイムアウト）: CanSM_MainFunction() が
 *                CANSM_WAKEUP_VALIDATION_MS 超過を検出
 *                → Can_SetControllerMode(CAN_T_SLEEP) → CanSM_State: NO_COM
 *                  （ComM/EcuM への通知なし）
 *
 *          ComM（CanSM が呼び返す通知の宛先）は 2026-08 の協調スリープ移管
 *          （Bsw_SleepCoordination_test.cpp 参照）で本 env に実体として
 *          統合された。本テストは ComM_RequestComMode() を経由せず CanSM の
 *          API を直接叩くため、ComM の内部状態（ComM_Init() 直後の既定値
 *          NO_COM）がそのまま「ボランタリスリープ済み」の前提と一致する。
 *          ComM が呼ぶ EcuM_RequestRUN()/BswM_ComM_CurrentMode() は境界として
 *          フェイクに差し替える（Bsw_EcuM_fake.h/Bsw_BswM_fake.h 参照）。
 *          Dem（Bus-Off 通信路の PASSED/FAILED 報告先）も同様にフェイクに
 *          差し替える（Bsw_Dem_fake.h 参照）。PduR はこのチェーンに登場
 *          しないため（CanSM_RxIndication() は CanIf_RxIndication() の PDU
 *          振り分けより前に無条件で呼ばれる、CanIf.c 参照）、CanIf の RxPdu
 *          設定は 0 件のまま使う。
 *
 *          Bsw_TxChain_test.cpp/Bsw_RxChain_test.cpp（README「Tx/Rx処理」の
 *          コールチェーン）と同じ `[env:native_chain]` 上で CanSM.c の実体を
 *          共有する（詳細は Bsw_TxChain_test.cpp 冒頭コメント参照）。
 */
#include <gtest/gtest.h>

extern "C" {
#include "CanSM.h"
#include "CanIf.h"
#include "Can.h"
#include "Can_Hw.h"
#include "ComM.h"
#include "Nm.h"
#include "Hal_Can_Hw_fake.h"
#include "Hal_Millis_fake.h"
#include "Bsw_EcuM_fake.h"
#include "Bsw_BswM_fake.h"
#include "Bsw_Dem_fake.h"
#include "Hal_Det_Hw_fake.h"
}

namespace
{

const CanIf_ConfigType kTestCanIfConfig = {
    /* TxPduConfig */ NULL,
    /* TxPduCount */  0U,
    /* RxPduConfig */ NULL,
    /* RxPduCount */  0U
};

class Bsw_WakeupChain_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeCanHw_Reset();
        FakeEcuM_Reset();
        FakeBswM_Reset();
        FakeDem_Reset();
        FakeMillis_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制

        canConfig.filter.filterId = 0x0220U;
        canConfig.filter.mask     = 0x1FFFU;
        canConfig.csPin           = 10U;
        canConfig.intPin          = 2U;
        canConfig.baudrate        = 500000U;
        canConfig.crystalFreq     = CAN_CRYSTAL_16MHZ;

        Can_Init(&canConfig);
        CanIf_Init(&kTestCanIfConfig);
        CanSM_Init(NULL);
        ComM_Init(NULL);
        Nm_Init(NULL);

        // ボランタリスリープ済みの状態を Arrange する（README のとおり、
        // ウェイクアップ検証は CANSM_STATE_NO_COM からの起床のみを受け付ける）。
        // ComM_Init() 直後の既定値は NO_COM のため、ComM 側は別途要求しなくても
        // この前提と一致する。
        ASSERT_EQ(Can_SetControllerMode(0U, CAN_T_SLEEP), CAN_OK);
        ASSERT_EQ(Can_Test_GetControllerState(), CAN_CS_SLEEP);
        FakeCanHw_SetModeCount = 0U;

        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;  // DeInit() のログを抑制
        Nm_DeInit();
        ComM_DeInit();
        CanSM_DeInit();
        CanIf_DeInit();
    }

    /* 現在 ComM が把握しているチャネルモードを返すヘルパー
     * （FakeComM_BusSM_ModeIndicationCount 相当の「まだ通知されていないか」判定に使う）。 */
    static ComM_ModeType CurrentComMode()
    {
        ComM_ModeType mode = 0xFFU;
        EXPECT_EQ(ComM_GetCurrentComMode(COMM_USER_0, &mode), E_OK);
        return mode;
    }

    /* 検証開始状態（CANSM_STATE_WAKEUP_VALIDATING）を Arrange するヘルパー。 */
    void ArrangeValidating()
    {
        FakeCanHw_IsWakeupPendingReturn = CAN_HW_OK;
        Can_MainFunction_Wakeup();
        ASSERT_EQ(Can_Test_GetControllerState(), CAN_CS_STOPPED);
        ASSERT_EQ(CurrentComMode(), static_cast<ComM_ModeType>(COMM_NO_COMMUNICATION));
        ASSERT_EQ(FakeEcuM_RequestRUNCount, 0U);
        FakeCanHw_SetModeCount = 0U;
        FakeEcuM_Reset();
        FakeBswM_Reset();
        FakeDem_Reset();
    }

    Can_ConfigType canConfig;
};

// ------------------------------------------------------------
// ウェイクアップ検出 → 検証開始（ComM/EcuM へはまだ通知しない）
// ------------------------------------------------------------
TEST_F(Bsw_WakeupChain_Test, CanMainFunctionWakeup_OK_StartsValidationWithoutNotifyingComM)
{
    /* 準備 (Arrange): SetUp() で SLEEP 済み */
    FakeCanHw_IsWakeupPendingReturn = CAN_HW_OK;

    /* 実行 (Act) */
    Can_MainFunction_Wakeup();

    /* 評価 (Assert): SLEEP → STOPPED (Listen-Only) のみ。FULL_COM へはまだ確定しない */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STOPPED);
    EXPECT_EQ(FakeCanHw_LastMode, CAN_HW_MODE_LISTEN_ONLY);
    EXPECT_EQ(CurrentComMode(), static_cast<ComM_ModeType>(COMM_NO_COMMUNICATION));
    EXPECT_EQ(FakeEcuM_RequestRUNCount, 0U);
}

TEST_F(Bsw_WakeupChain_Test, CanMainFunctionWakeup_NG_NoWakeupPending_StaysAsleep)
{
    /* 準備 (Arrange): ウェイクアップ要因なし（既定 CAN_HW_FAIL のまま） */

    /* 実行 (Act) */
    Can_MainFunction_Wakeup();

    /* 評価 (Assert) */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_SLEEP);
    EXPECT_EQ(FakeCanHw_SetModeCount, 0U);
}

// ------------------------------------------------------------
// 検証成功: 受信フレームが CanSM_RxIndication() まで届き FULL_COM へ確定する
// ------------------------------------------------------------
TEST_F(Bsw_WakeupChain_Test, CanMainFunctionRead_OK_ValidatesWakeupAndNotifiesComMFullCom)
{
    /* 準備 (Arrange): セグメント①の終端状態（WAKEUP_VALIDATING）を用意する */
    ArrangeValidating();
    FakeCanHw_RxPendingCount = 1U;
    FakeCanHw_RxId  = 0x100U;
    FakeCanHw_RxDlc = 2U;
    FakeCanHw_RxData[0] = 0x12U;
    FakeCanHw_RxData[1] = 0x34U;

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert) */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);
    EXPECT_EQ(FakeCanHw_LastMode, CAN_HW_MODE_NORMAL);
    EXPECT_EQ(CurrentComMode(), static_cast<ComM_ModeType>(COMM_FULL_COMMUNICATION));
    EXPECT_EQ(FakeEcuM_RequestRUNCount, 1U);
    EXPECT_EQ(FakeBswM_ComM_CurrentModeCount, 1U);
    EXPECT_EQ(FakeBswM_LastMode, static_cast<ComM_ModeType>(COMM_FULL_COMMUNICATION));
    // Bus-Off の TF クリア相当（PASSED）を Dem へ報告する
    EXPECT_EQ(FakeDem_ReportErrorStatusCount, 1U);
    EXPECT_EQ(FakeDem_LastEventStatus, DEM_EVENT_STATUS_PASSED);
}

// ------------------------------------------------------------
// 検証失敗（タイムアウト）: ComM/EcuM への通知なしに再スリープする
// ------------------------------------------------------------
TEST_F(Bsw_WakeupChain_Test, CanSMMainFunction_NG_ValidationTimeout_ReturnsToSleepSilently)
{
    /* 準備 (Arrange): セグメント①の終端状態（WAKEUP_VALIDATING）から、
     * 検証タイマ（CANSM_WAKEUP_VALIDATION_MS=2000ms）を超過させる */
    ArrangeValidating();
    FakeMillis_Value = 2001UL;

    /* 実行 (Act) */
    CanSM_MainFunction();

    /* 評価 (Assert): ノイズによる誤ウェイクアップとみなし、静かに再スリープする */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_SLEEP);
    EXPECT_EQ(FakeCanHw_LastMode, CAN_HW_MODE_SLEEP);
    EXPECT_EQ(CurrentComMode(), static_cast<ComM_ModeType>(COMM_NO_COMMUNICATION));
    EXPECT_EQ(FakeEcuM_RequestRUNCount, 0U);
}

TEST_F(Bsw_WakeupChain_Test, CanSMMainFunction_NG_ValidationNotYetTimedOut_StaysValidating)
{
    /* 準備 (Arrange): タイマ超過前 */
    ArrangeValidating();
    FakeMillis_Value = 1999UL;

    /* 実行 (Act) */
    CanSM_MainFunction();

    /* 評価 (Assert): まだ検証継続中（Listen-Only のまま） */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STOPPED);
    EXPECT_EQ(FakeCanHw_SetModeCount, 0U);
    EXPECT_EQ(CurrentComMode(), static_cast<ComM_ModeType>(COMM_NO_COMMUNICATION));
    EXPECT_EQ(FakeEcuM_RequestRUNCount, 0U);
}

}  // namespace
