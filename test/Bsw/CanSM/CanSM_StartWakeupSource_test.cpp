/**
 * \file    CanSM_StartWakeupSource_test.cpp
 * \brief   CanSM_StartWakeupSource() の単体テスト
 * \details [SWS_CanSM_00609]/[SWS_CanSM_00616]/[SWS_CanSM_00607]。NO_COM のときだけ、起床の検証を始める（コントローラを
 *          Listen-Only へ移し、WAKEUP_VALIDATING へ進む）。それ以外の状態では、何もしない（要求は受理する）。
 *          CanIf は wrap で、実体を呼ばずに成功・失敗を注入する。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Can.h"
#include "Can_Hw.h"
#include "CanIf.h"
#include "CanSM.h"
#include "CanSM_Cfg.h"
#include "ComM.h"
#include "Fake_Can_Hw.h"
#include "Fake_Millis.h"
#include "Fake_Det_Hw.h"
#include "Wrap_Dem.h"
#include "Wrap_CanIf.h"
}

/* ======================================================================
 * Definitions
 * ====================================================================== */

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * Global Variables
 * ====================================================================== */

namespace
{

/* CanIf を wrap で成功させる（実体の Can を呼ばない）ため、CanSM が待っているコントローラのモード変更の完了通知を、
 * CanSM へ直接流す。STOPPED → STARTED → SLEEP の順に流すと、FULL_COM へ入る手順（STOPPED → STARTED）も、
 * NO_COM へ入る手順（STOPPED → SLEEP）も、完了する（待っていないモードの通知は、無視される）。 */
void SettleCanSmModeRequests()
{
    CanSM_ControllerModeIndication(0U, CAN_CS_STOPPED);
    CanSM_ControllerModeIndication(0U, CAN_CS_STARTED);
    CanSM_ControllerModeIndication(0U, CAN_CS_SLEEP);
}


/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* CanSM_StartWakeupSource() の単体テスト用フィクスチャ。
 * SetUp(): CanSM_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): CanSM_DeInit() で未初期化へ戻す。 */
class CanSM_StartWakeupSource_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        WrapCanIf_Reset();             // 他のテストファイルの故障注入が漏れ伝わらないようリセット
        FakeMillis_Reset();
        CanSM_Init(NULL);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        WrapCanIf_Reset();   // 故障注入を残さない
        CanSM_DeInit();
    }

    static const NetworkHandleType kInvalidNetwork = CANSM_CHANNEL_COUNT;
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(CanSM_StartWakeupSource_Test, CanSM_StartWakeupSource_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanSM_DeInit();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanSM_StartWakeupSource(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANSM_E_UNINIT);
}

TEST_F(CanSM_StartWakeupSource_Test, CanSM_StartWakeupSource_NG_InvalidNetwork)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanSM_StartWakeupSource(kInvalidNetwork);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANSM_E_INVALID_NETWORK_HANDLE);
}

// NO_COM のとき、Listen-Only（STOPPED）への移行を要求して、検証中へ進む。検証がタイムアウトすれば、再スリープを要求する
TEST_F(CanSM_StartWakeupSource_Test, CanSM_StartWakeupSource_OK_StartsValidationInNoCom)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanIf_SetControllerMode = 0U;   // コントローラの状態変更は、常に成功する
    ForcedReturn_CanIf_SetControllerMode      = E_OK;
    FailFromCallCount_CanIf_GetControllerMode = 0U;   // 現在のモードは、不明（CanIf は未初期化のため、実体を呼ばない）
    ForcedReturn_CanIf_GetControllerMode      = E_OK;
    const uint32 setModeBefore = CallCount_CanIf_SetControllerMode;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanSM_StartWakeupSource(0U);
    const uint32 afterStart = CallCount_CanIf_SetControllerMode;
    FakeMillis_Value += CANSM_WAKEUP_VALIDATION_MS + 1UL;
    CanSM_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(afterStart, setModeBefore + 1U);                               // STOPPED（Listen-Only）の要求
    EXPECT_EQ(CallCount_CanIf_SetControllerMode, afterStart + 1U);           // 検証がタイムアウトして、再スリープの要求
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

// NO_COM ではないとき（FULL_COM）は、要求だけ受理して、何もしない
TEST_F(CanSM_StartWakeupSource_Test, CanSM_StartWakeupSource_OK_IgnoredOutsideNoCom)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanIf_SetControllerMode = 0U;
    ForcedReturn_CanIf_SetControllerMode      = E_OK;
    FailFromCallCount_CanIf_SetPduMode = 0U;
    ForcedReturn_CanIf_SetPduMode      = E_OK;
    ASSERT_EQ(CanSM_RequestComMode(0U, COMM_FULL_COMMUNICATION), E_OK);
    SettleCanSmModeRequests();
    const uint32 setModeBefore = CallCount_CanIf_SetControllerMode;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanSM_StartWakeupSource(0U);
    ComM_ModeType mode = COMM_NO_COMMUNICATION;
    (void)CanSM_GetCurrentComMode(0U, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(mode, COMM_FULL_COMMUNICATION);                          // 状態は変わらない
    EXPECT_EQ(CallCount_CanIf_SetControllerMode, setModeBefore);       // コントローラへも何も要求しない
}

// 完了通知が来なければ、CANSM_MODEREQ_REPEAT_TIME_MS ごとに、STOPPED の要求を繰り返す。通知が来たら、繰り返さない
// （T_CC_STOPPED_TIMEOUT / T_CC_STOPPED_INDICATED、[SWS_CanSM_00629]/[00630]）
TEST_F(CanSM_StartWakeupSource_Test, CanSM_StartWakeupSource_OK_RepeatsStoppedRequestUntilIndication)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanIf_SetControllerMode = 0U;
    ForcedReturn_CanIf_SetControllerMode      = E_OK;
    FailFromCallCount_CanIf_GetControllerMode = 0U;
    ForcedReturn_CanIf_GetControllerMode      = E_OK;
    ASSERT_EQ(CanSM_StartWakeupSource(0U), E_OK);
    const uint32 afterStart = CallCount_CanIf_SetControllerMode;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanSM_MainFunction();   // まだ、繰り返しの時間が経っていない
    const uint32 beforeRepeat = CallCount_CanIf_SetControllerMode;
    FakeMillis_Value += CANSM_MODEREQ_REPEAT_TIME_MS + 1UL;
    CanSM_MainFunction();   // 通知が来ないので、繰り返す
    const uint32 afterRepeat = CallCount_CanIf_SetControllerMode;
    CanSM_ControllerModeIndication(0U, CAN_CS_STOPPED);   // 通知が来た
    FakeMillis_Value += CANSM_MODEREQ_REPEAT_TIME_MS + 1UL;
    CanSM_MainFunction();   // もう繰り返さない

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(beforeRepeat, afterStart);
    EXPECT_EQ(afterRepeat, afterStart + 1U);
    EXPECT_EQ(CallCount_CanIf_SetControllerMode, afterRepeat);
}

// 検証の最初の通知（STOPPED）を待っている間に、FULL_COM が要求されたら、FULL_COM へ向かう手順（STOPPED → STARTED）へ進む
TEST_F(CanSM_StartWakeupSource_Test, CanSM_StartWakeupSource_OK_FullComRequestWhileWaitingForStoppedGoesToFullCom)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanIf_SetControllerMode = 0U;
    ForcedReturn_CanIf_SetControllerMode      = E_OK;
    FailFromCallCount_CanIf_GetControllerMode = 0U;
    ForcedReturn_CanIf_GetControllerMode      = E_OK;
    FailFromCallCount_CanIf_SetPduMode = 0U;
    ForcedReturn_CanIf_SetPduMode      = E_OK;
    ASSERT_EQ(CanSM_StartWakeupSource(0U), E_OK);   // STOPPED の通知を待つ

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ASSERT_EQ(CanSM_RequestComMode(0U, COMM_FULL_COMMUNICATION), E_OK);
    CanSM_ControllerModeIndication(0U, CAN_CS_STOPPED);
    CanSM_ControllerModeIndication(0U, CAN_CS_STARTED);
    ComM_ModeType mode = COMM_NO_COMMUNICATION;
    ASSERT_EQ(CanSM_GetCurrentComMode(0U, &mode), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(mode, COMM_FULL_COMMUNICATION);
}

// 検証を止めて再スリープする最中に、FULL_COM が要求されたら、NO_COM へ入り終えたあとで、FULL_COM へ向かう手順を始める
TEST_F(CanSM_StartWakeupSource_Test, CanSM_StartWakeupSource_OK_FullComRequestWhileSleepingAgainStartsAfterNoCom)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanIf_SetControllerMode = 0U;
    ForcedReturn_CanIf_SetControllerMode      = E_OK;
    FailFromCallCount_CanIf_GetControllerMode = 0U;
    ForcedReturn_CanIf_GetControllerMode      = E_OK;
    ASSERT_EQ(CanSM_StartWakeupSource(0U), E_OK);
    CanSM_ControllerModeIndication(0U, CAN_CS_STOPPED);   // Listen-Only になった
    ASSERT_EQ(CanSM_StopWakeupSource(0U), E_OK);          // 検証を止めて、再スリープへ（現在のモードは不明のため、STOPPED から）
    const uint32 afterStop = CallCount_CanIf_SetControllerMode;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ASSERT_EQ(CanSM_RequestComMode(0U, COMM_FULL_COMMUNICATION), E_OK);   // 保留される
    const uint32 afterRequest = CallCount_CanIf_SetControllerMode;
    CanSM_ControllerModeIndication(0U, CAN_CS_STOPPED);   // STOPPED の通知で、SLEEP を要求
    CanSM_ControllerModeIndication(0U, CAN_CS_SLEEP);     // NO_COM へ入り終えて、保留した FULL_COM の手順を始める
    const uint32 afterSleep = CallCount_CanIf_SetControllerMode;

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(afterRequest, afterStop);          // 再スリープの最中は、FULL_COM の要求を、すぐには処理しない
    EXPECT_EQ(afterSleep, afterStop + 2U);       // SLEEP の要求 + 保留した FULL_COM の手順の最初の要求
}

// 要求が失敗しても、検証中のまま、繰り返す（検証のタイムアウトが、最後に再スリープへ戻す）
TEST_F(CanSM_StartWakeupSource_Test, CanSM_StartWakeupSource_OK_KeepsRepeatingWhenControllerCannotStop)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanIf_SetControllerMode = 0U;   // コントローラの状態変更は、常に失敗する
    ForcedReturn_CanIf_SetControllerMode      = E_NOT_OK;
    FailFromCallCount_CanIf_GetControllerMode = 0U;
    ForcedReturn_CanIf_GetControllerMode      = E_OK;
    const uint32 setModeBefore = CallCount_CanIf_SetControllerMode;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanSM_StartWakeupSource(0U);
    const uint32 afterStart = CallCount_CanIf_SetControllerMode;
    FakeMillis_Value += CANSM_MODEREQ_REPEAT_TIME_MS + 1UL;
    CanSM_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);                                              // 要求自体は受理する（[SWS_CanSM_00616]）
    EXPECT_EQ(afterStart, setModeBefore + 1U);                         // 停止を要求したが失敗した
    EXPECT_EQ(CallCount_CanIf_SetControllerMode, afterStart + 1U);     // 検証中のまま、通知が来ないので、繰り返す
}

}  // namespace
