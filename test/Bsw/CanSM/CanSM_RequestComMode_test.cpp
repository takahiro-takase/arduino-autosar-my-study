/**
 * \file    CanSM_RequestComMode_test.cpp
 * \brief   CanSM_RequestComMode() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 CanSM_test.cpp）。
 *
 *          分割前の共通の説明は、旧 CanSM_test.cpp の冒頭コメント（git の履歴）を参照。
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
#include "CanNm.h"
#include "Nm.h"
#include "Fake_Can_Hw.h"
#include "Fake_Millis.h"
#include "Fake_Det_Hw.h"
#include "Wrap_Dem.h"
#include "Fake_Bsw_EcuM.h"
#include "Wrap_BswM.h"
#include "Wrap_CanIf.h"
#include "Wrap_Can.h"
#include "Wrap_ComM.h"
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

/**
 * \details 上記セクションは Bus-Off 回復のリトライ/バックオフ状態機械のみを
 *          検証しており、`Det_ReportError()` の報告内容はどこからも
 *          検証されていなかった。本セクションは CanSM.c の全
 *          `Det_ReportError()` 呼び出し箇所（NG ケースのみ）をまとめ、
 *          報告される ErrorId が仕様どおり正しい値になっていることを
 *          `Fake_Det_Hw.h`（`FakeDetHw_LastErrorId`）で検証する。
 *
 *          CanSM には `CanSM_DeInit()` が存在するため、Mcu/PduR/CanTp と
 *          異なり「未初期化状態」の検証にテスト専用のリセット関数は不要
 *          （各テストが明示的に `CanSM_DeInit()` を呼んでから検証する）。
 *
 *          検証対象はいずれも Can/CanIf への実処理カスケードへ到達する前に
 *          reject される分岐のみのため（CanSM.c 参照）、Can_Init()/CanIf_Init()
 *          は不要（他モジュールのテストの残留 static 状態と競合しない）。
 */

/* CanSM（Bus-Off 回復以外の API）の単体テスト用フィクスチャ。
 * SetUp(): CanSM_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): CanSM_DeInit() で未初期化へ戻す。 */
class CanSM_RequestComMode_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        WrapCanIf_Reset();             // 他のテストファイルの故障注入が漏れ伝わらないようリセット
        CanSM_Init(NULL);
        FakeDetHw_Reset();              // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        WrapCanIf_Reset();   // 故障注入を残さない
        CanSM_DeInit();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(CanSM_RequestComMode_Test, CanSM_RequestComMode_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanSM_DeInit();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CanSM_RequestComMode(0U, COMM_FULL_COMMUNICATION);
    SettleCanSmModeRequests();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANSM_E_UNINIT);
}

TEST_F(CanSM_RequestComMode_Test, CanSM_RequestComMode_NG_InvalidNetworkHandle)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CanSM_RequestComMode(CANSM_CHANNEL_COUNT, COMM_FULL_COMMUNICATION);
    SettleCanSmModeRequests();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANSM_E_INVALID_NETWORK_HANDLE);
}

TEST_F(CanSM_RequestComMode_Test, CanSM_RequestComMode_NG_UnknownModeIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanSM_RequestComMode(0U, static_cast<ComM_ModeType>(0xFFU));
    SettleCanSmModeRequests();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
}

TEST_F(CanSM_RequestComMode_Test, CanSM_RequestComMode_OK_FullComRetriesAndGivesUpWhenControllerCannotStart)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanIf_SetControllerMode = 0U;   // コントローラの状態変更は、常に失敗する
    ForcedReturn_CanIf_SetControllerMode      = E_NOT_OK;
    FakeMillis_Reset();
    WrapDem_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanSM_RequestComMode(0U, COMM_FULL_COMMUNICATION);   // 受理する（手順を始める）
    const uint32 callsAtRequest = CallCount_CanIf_SetControllerMode;
    for (unsigned i = 0U; i < (2U * (CANSM_MODEREQ_REPEAT_MAX + 1U)) + 2U; i++)
    {
        FakeMillis_Value += CANSM_MODEREQ_REPEAT_TIME_MS + 1UL;
        CanSM_MainFunction();   // 完了通知が来ないので、要求を繰り返す。上限を超えたら、NO_COM へ戻り、そこでも、あきらめる
    }
    const uint32 callsAfterGivingUp = CallCount_CanIf_SetControllerMode;
    for (unsigned i = 0U; i < 5U; i++)
    {
        FakeMillis_Value += CANSM_MODEREQ_REPEAT_TIME_MS + 1UL;
        CanSM_MainFunction();   // あきらめて NO_COM へ入り終えたあとは、もう要求しない
    }
    ComM_ModeType mode = COMM_FULL_COMMUNICATION;
    const Std_ReturnType retMode = CanSM_GetCurrentComMode(0U, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);                                             // 要求は受理する。結果は、完了通知が決める
    EXPECT_GT(callsAfterGivingUp, callsAtRequest);                    // 要求を繰り返した
    EXPECT_EQ(CallCount_CanIf_SetControllerMode, callsAfterGivingUp); // 上限を超えたら、それ以上は繰り返さない（T_REPEAT_MAX）
    // T_REPEAT_MAX のたびに、タイムアウトの故障を PREFAILED で報告する（PRE_FULL_COM と PRE_NO_COM の 2 回分）
    EXPECT_EQ(LastStatusOfEvent_Dem_SetEventStatus[DemConf_DemEventParameter_CANSM_MODE_REQUEST_TIMEOUT], DEM_EVENT_STATUS_PREFAILED);
    EXPECT_GE(CallCount_Dem_SetEventStatus, 2U);
    ASSERT_EQ(retMode, E_OK);
    EXPECT_EQ(mode, COMM_NO_COMMUNICATION);                           // FULL_COM へは達しない
}

TEST_F(CanSM_RequestComMode_Test, CanSM_RequestComMode_OK_FullComWaitsForControllerModeIndications)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanIf_SetControllerMode = 0U;   // コントローラの状態変更は、常に成功する（完了通知は、来ない）
    ForcedReturn_CanIf_SetControllerMode      = E_OK;
    FailFromCallCount_CanIf_SetPduMode = 0U;
    ForcedReturn_CanIf_SetPduMode      = E_OK;
    WrapComM_Reset();
    WrapDem_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ASSERT_EQ(CanSM_RequestComMode(0U, COMM_FULL_COMMUNICATION), E_OK);
    ComM_ModeType modeBefore = COMM_FULL_COMMUNICATION;
    ASSERT_EQ(CanSM_GetCurrentComMode(0U, &modeBefore), E_OK);
    const uint32 comMBefore = CallCount_ComM_BusSM_ModeIndication;
    CanSM_ControllerModeIndication(0U, CAN_CS_STOPPED);   // 1 つ目（STOPPED）の完了
    const uint32 comMAfterStopped = CallCount_ComM_BusSM_ModeIndication;
    CanSM_ControllerModeIndication(0U, CAN_CS_STARTED);   // 2 つ目（STARTED）の完了
    ComM_ModeType modeAfter = COMM_NO_COMMUNICATION;
    ASSERT_EQ(CanSM_GetCurrentComMode(0U, &modeAfter), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(modeBefore, COMM_NO_COMMUNICATION);                       // 完了通知が来るまで、FULL_COM ではない
    EXPECT_EQ(comMBefore, 0U);                                          // ComM へも、まだ通知しない
    EXPECT_EQ(comMAfterStopped, 0U);                                    // STOPPED の完了だけでは、まだ通知しない
    EXPECT_EQ(modeAfter, COMM_FULL_COMMUNICATION);                      // STARTED の完了で、FULL_COM へ入り終える
    EXPECT_EQ(CallCount_ComM_BusSM_ModeIndication, comMBefore + 1U);    // ComM へは、そこで 1 回だけ通知する
    // 最後の完了通知（STARTED）で FULL_COM へ入り終えたときに、1 回だけ、タイムアウトの故障を PREPASSED で戻す（[SWS_CanSM_00664]）。
    // 途中の STOPPED の通知では、報告しない
    EXPECT_EQ(LastStatusOfEvent_Dem_SetEventStatus[DemConf_DemEventParameter_CANSM_MODE_REQUEST_TIMEOUT], DEM_EVENT_STATUS_PREPASSED);
    EXPECT_EQ(CallCount_Dem_SetEventStatus, 1U);
}

// T_REPEAT_MAX で出した PREFAILED は、そのあとの NO_COM へ戻る手順の完了通知で、打ち消されない
TEST_F(CanSM_RequestComMode_Test, CanSM_RequestComMode_OK_PreFailedIsNotCancelledByNoComSequenceIndications)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanIf_SetControllerMode = 0U;   // 要求は成功する（完了通知だけ、来ない）
    ForcedReturn_CanIf_SetControllerMode      = E_OK;
    FailFromCallCount_CanIf_SetPduMode = 0U;
    ForcedReturn_CanIf_SetPduMode      = E_OK;
    FakeMillis_Reset();
    WrapDem_Reset();
    ASSERT_EQ(CanSM_RequestComMode(0U, COMM_FULL_COMMUNICATION), E_OK);
    CanSM_ControllerModeIndication(0U, CAN_CS_STOPPED);   // STARTED の完了通知は、来ないまま
    for (unsigned i = 0U; i < CANSM_MODEREQ_REPEAT_MAX + 1U; i++)
    {
        FakeMillis_Value += CANSM_MODEREQ_REPEAT_TIME_MS + 1UL;
        CanSM_MainFunction();   // T_REPEAT_MAX へ達して、PRE_NO_COM へ戻る
    }
    const uint32 demAtGiveUp = CallCount_Dem_SetEventStatus;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanSM_ControllerModeIndication(0U, CAN_CS_STOPPED);   // NO_COM へ戻る手順の完了通知（STOPPED → SLEEP）
    CanSM_ControllerModeIndication(0U, CAN_CS_SLEEP);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(demAtGiveUp, 1U);                                     // PREFAILED が 1 回
    EXPECT_EQ(CallCount_Dem_SetEventStatus, demAtGiveUp);           // NO_COM へ戻る手順では、PREPASSED を出さない
    EXPECT_EQ(LastStatusOfEvent_Dem_SetEventStatus[DemConf_DemEventParameter_CANSM_MODE_REQUEST_TIMEOUT], DEM_EVENT_STATUS_PREFAILED);
}

TEST_F(CanSM_RequestComMode_Test, CanSM_RequestComMode_OK_NoComRequestedDuringPreFullComTurnsBack)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanIf_SetControllerMode = 0U;
    ForcedReturn_CanIf_SetControllerMode      = E_OK;
    FailFromCallCount_CanIf_SetPduMode = 0U;
    ForcedReturn_CanIf_SetPduMode      = E_OK;
    ASSERT_EQ(CanSM_RequestComMode(0U, COMM_FULL_COMMUNICATION), E_OK);   // FULL_COM へ向かう途中（STOPPED の完了待ち）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ASSERT_EQ(CanSM_RequestComMode(0U, COMM_NO_COMMUNICATION), E_OK);     // NO_COM へ向きを変える
    SettleCanSmModeRequests();
    ComM_ModeType mode = COMM_FULL_COMMUNICATION;
    ASSERT_EQ(CanSM_GetCurrentComMode(0U, &mode), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(mode, COMM_NO_COMMUNICATION);
}

TEST_F(CanSM_RequestComMode_Test, CanSM_RequestComMode_OK_FullComRequestedDuringPreNoComStartsAfterNoCom)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanIf_SetControllerMode = 0U;
    ForcedReturn_CanIf_SetControllerMode      = E_OK;
    FailFromCallCount_CanIf_SetPduMode = 0U;
    ForcedReturn_CanIf_SetPduMode      = E_OK;
    ASSERT_EQ(CanSM_RequestComMode(0U, COMM_FULL_COMMUNICATION), E_OK);
    SettleCanSmModeRequests();                                           // FULL_COM
    ASSERT_EQ(CanSM_RequestComMode(0U, COMM_NO_COMMUNICATION), E_OK);   // NO_COM へ向かう途中（STOPPED の完了待ち）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ASSERT_EQ(CanSM_RequestComMode(0U, COMM_FULL_COMMUNICATION), E_OK); // 受理して、保留する
    ComM_ModeType modeWhilePending = COMM_FULL_COMMUNICATION;
    ASSERT_EQ(CanSM_GetCurrentComMode(0U, &modeWhilePending), E_OK);
    SettleCanSmModeRequests();   // NO_COM へ入り終える
    SettleCanSmModeRequests();   // 続けて、保留していた FULL_COM へ向かい、入り終える
    ComM_ModeType modeAfter = COMM_NO_COMMUNICATION;
    ASSERT_EQ(CanSM_GetCurrentComMode(0U, &modeAfter), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(modeWhilePending, COMM_FULL_COMMUNICATION);   // NO_COM へ入り終えるまでは、直前のモード（[SWS_CanSM_00266]）
    EXPECT_EQ(modeAfter, COMM_FULL_COMMUNICATION);
}

TEST_F(CanSM_RequestComMode_Test, CanSM_RequestComMode_NG_SilentComFailsWhenTxCannotBeSuppressed)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanIf_SetControllerMode = 0U;   // コントローラの状態変更は、常に成功する
    ForcedReturn_CanIf_SetControllerMode      = E_OK;
    FailFromCallCount_CanIf_SetPduMode = 0U;   // PDU モードの変更は、常に成功する
    ForcedReturn_CanIf_SetPduMode      = E_OK;
    ASSERT_EQ(CanSM_RequestComMode(0U, COMM_FULL_COMMUNICATION), E_OK);   // FULL_COM にする
    SettleCanSmModeRequests();
    FailFromCallCount_CanIf_SetPduMode = 0U;   // PDU モードの変更は、常に失敗する
    ForcedReturn_CanIf_SetPduMode      = E_NOT_OK;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanSM_RequestComMode(0U, COMM_SILENT_COMMUNICATION);
    SettleCanSmModeRequests();
    ComM_ModeType mode = COMM_NO_COMMUNICATION;
    const Std_ReturnType retMode = CanSM_GetCurrentComMode(0U, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    ASSERT_EQ(retMode, E_OK);
    EXPECT_EQ(mode, COMM_FULL_COMMUNICATION);   // 送信を止められないので、FULL_COM のまま
}

TEST_F(CanSM_RequestComMode_Test, CanSM_RequestComMode_OK_FullComProceedsEvenWhenPduModeCannotBeSetOnline)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanIf_SetControllerMode = 0U;   // コントローラの状態変更は、常に成功する
    ForcedReturn_CanIf_SetControllerMode      = E_OK;
    FailFromCallCount_CanIf_SetPduMode = 0U;   // PDU モードの変更は、常に失敗する
    ForcedReturn_CanIf_SetPduMode      = E_NOT_OK;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanSM_RequestComMode(0U, COMM_FULL_COMMUNICATION);
    SettleCanSmModeRequests();
    ComM_ModeType mode = COMM_NO_COMMUNICATION;
    const Std_ReturnType retMode = CanSM_GetCurrentComMode(0U, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);   // PDU モードの設定失敗は、ベストエフォートとして先へ進む
    ASSERT_EQ(retMode, E_OK);
    EXPECT_EQ(mode, COMM_FULL_COMMUNICATION);
}

TEST_F(CanSM_RequestComMode_Test, CanSM_RequestComMode_OK_FullThenSilentThenNoComAreReflectedInCurrentMode)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanIf_SetControllerMode = 0U;   // コントローラの状態変更は、常に成功する
    ForcedReturn_CanIf_SetControllerMode      = E_OK;
    FailFromCallCount_CanIf_SetPduMode = 0U;   // PDU モードの変更は、常に成功する
    ForcedReturn_CanIf_SetPduMode      = E_OK;
    ComM_ModeType full = COMM_NO_COMMUNICATION;
    ComM_ModeType silent = COMM_NO_COMMUNICATION;
    ComM_ModeType none = COMM_FULL_COMMUNICATION;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ASSERT_EQ(CanSM_RequestComMode(0U, COMM_FULL_COMMUNICATION), E_OK);
    SettleCanSmModeRequests();
    ASSERT_EQ(CanSM_GetCurrentComMode(0U, &full), E_OK);
    ASSERT_EQ(CanSM_RequestComMode(0U, COMM_SILENT_COMMUNICATION), E_OK);
    SettleCanSmModeRequests();
    ASSERT_EQ(CanSM_GetCurrentComMode(0U, &silent), E_OK);
    ASSERT_EQ(CanSM_RequestComMode(0U, COMM_NO_COMMUNICATION), E_OK);
    SettleCanSmModeRequests();
    ASSERT_EQ(CanSM_GetCurrentComMode(0U, &none), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(full, COMM_FULL_COMMUNICATION);
    EXPECT_EQ(silent, COMM_SILENT_COMMUNICATION);
    EXPECT_EQ(none, COMM_NO_COMMUNICATION);
}

TEST_F(CanSM_RequestComMode_Test, CanSM_RequestComMode_OK_NoComDuringBusOffIsAcceptedAndCancelsRecovery)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanIf_SetControllerMode = 0U;   // コントローラの状態変更は、常に成功する
    ForcedReturn_CanIf_SetControllerMode      = E_OK;
    FailFromCallCount_CanIf_SetPduMode = 0U;          // PDU モードの変更は、常に成功する
    ForcedReturn_CanIf_SetPduMode      = E_OK;
    FakeMillis_Reset();
    ASSERT_EQ(CanSM_RequestComMode(0U, COMM_FULL_COMMUNICATION), E_OK);
    SettleCanSmModeRequests();
    CanSM_ControllerBusOff(0U);   // Bus-Off の回復中になる
    const uint32 controllerCallsBefore = CallCount_CanIf_SetControllerMode;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanSM_RequestComMode(0U, COMM_NO_COMMUNICATION);
    SettleCanSmModeRequests();
    FakeMillis_Value += static_cast<unsigned long>(CANSM_BUSOFF_RECOVERY_L1_MS) + 1UL;
    CanSM_MainFunction();   // 回復のタイマが満了しても、回復しない

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ComM_ModeType mode = COMM_FULL_COMMUNICATION;
    ASSERT_EQ(CanSM_GetCurrentComMode(0U, &mode), E_OK);
    EXPECT_EQ(ret, E_OK);                                        // 受理する（[SWS_CanSM_00369]）。以前は E_NOT_OK で捨てていた
    EXPECT_EQ(mode, COMM_NO_COMMUNICATION);                      // すぐに、NO_COM へ移る
    EXPECT_EQ(CallCount_CanIf_SetControllerMode, controllerCallsBefore + 2U);   // 停止とスリープの要求の 2 回だけ。回復の再起動はしない
}

TEST_F(CanSM_RequestComMode_Test, CanSM_RequestComMode_OK_FullComDuringBusOffReturnsToFullComAfterRecovery)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanIf_SetControllerMode = 0U;   // コントローラの状態変更は、常に成功する
    ForcedReturn_CanIf_SetControllerMode      = E_OK;
    FailFromCallCount_CanIf_SetPduMode = 0U;          // PDU モードの変更は、常に成功する
    ForcedReturn_CanIf_SetPduMode      = E_OK;
    FakeMillis_Reset();
    ASSERT_EQ(CanSM_RequestComMode(0U, COMM_FULL_COMMUNICATION), E_OK);
    SettleCanSmModeRequests();
    ASSERT_EQ(CanSM_RequestComMode(0U, COMM_SILENT_COMMUNICATION), E_OK);   // Bus-Off の前は SILENT_COM
    SettleCanSmModeRequests();
    CanSM_ControllerBusOff(0U);   // Bus-Off の回復中になる
    const uint32 pduModeCallsBefore = CallCount_CanIf_SetPduMode;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanSM_RequestComMode(0U, COMM_FULL_COMMUNICATION);   // 回復の最中に、FULL_COM を要求する
    SettleCanSmModeRequests();
    FakeMillis_Value += static_cast<unsigned long>(CANSM_BUSOFF_RECOVERY_L1_MS) + 1UL;
    CanSM_MainFunction();   // 回復する

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ComM_ModeType mode = COMM_NO_COMMUNICATION;
    ASSERT_EQ(CanSM_GetCurrentComMode(0U, &mode), E_OK);
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(mode, COMM_FULL_COMMUNICATION);                    // 要求されたモード（FULL_COM）へ戻る（[SWS_CanSM_00182]）
    EXPECT_EQ(CallCount_CanIf_SetPduMode, pduModeCallsBefore + 1U);   // PDU を ONLINE にする（1 回）
}

TEST_F(CanSM_RequestComMode_Test, CanSM_RequestComMode_OK_SilentComDuringBusOffReturnsToSilentComAfterRecovery)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanIf_SetControllerMode = 0U;   // コントローラの状態変更は、常に成功する
    ForcedReturn_CanIf_SetControllerMode      = E_OK;
    FailFromCallCount_CanIf_SetPduMode = 0U;          // PDU モードの変更は、常に成功する
    ForcedReturn_CanIf_SetPduMode      = E_OK;
    FakeMillis_Reset();
    ASSERT_EQ(CanSM_RequestComMode(0U, COMM_FULL_COMMUNICATION), E_OK);
    SettleCanSmModeRequests();
    CanSM_ControllerBusOff(0U);   // Bus-Off の回復中になる
    const uint32 pduModeCallsBefore = CallCount_CanIf_SetPduMode;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanSM_RequestComMode(0U, COMM_SILENT_COMMUNICATION);   // 回復の最中に、SILENT_COM を要求する
    SettleCanSmModeRequests();
    const uint32 pduModeCallsAfterRequest = CallCount_CanIf_SetPduMode;               // TX を止める（1 回）
    FakeMillis_Value += static_cast<unsigned long>(CANSM_BUSOFF_RECOVERY_L1_MS) + 1UL;
    CanSM_MainFunction();   // 回復する

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ComM_ModeType mode = COMM_NO_COMMUNICATION;
    ASSERT_EQ(CanSM_GetCurrentComMode(0U, &mode), E_OK);
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(pduModeCallsAfterRequest, pduModeCallsBefore + 1U);   // 要求の時点で、TX を止める
    EXPECT_EQ(mode, COMM_SILENT_COMMUNICATION);                     // 要求されたモード（SILENT_COM）へ戻る
    EXPECT_EQ(CallCount_CanIf_SetPduMode, pduModeCallsAfterRequest);   // 回復では、PDU モードを触らない
}

TEST_F(CanSM_RequestComMode_Test, CanSM_RequestComMode_NG_SilentComDuringBusOffIsDeniedWhenTxCannotBeSuppressed)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanIf_SetControllerMode = 0U;   // コントローラの状態変更は、常に成功する
    ForcedReturn_CanIf_SetControllerMode      = E_OK;
    FailFromCallCount_CanIf_SetPduMode = 0U;          // PDU モードの変更は、常に成功する
    ForcedReturn_CanIf_SetPduMode      = E_OK;
    FakeMillis_Reset();
    ASSERT_EQ(CanSM_RequestComMode(0U, COMM_FULL_COMMUNICATION), E_OK);
    SettleCanSmModeRequests();
    CanSM_ControllerBusOff(0U);   // Bus-Off の回復中になる
    FailFromCallCount_CanIf_SetPduMode = 1U;   // これ以降の PDU モードの変更は、失敗する
    ForcedReturn_CanIf_SetPduMode      = E_NOT_OK;
    CallCount_CanIf_SetPduMode = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanSM_RequestComMode(0U, COMM_SILENT_COMMUNICATION);
    SettleCanSmModeRequests();
    ForcedReturn_CanIf_SetPduMode = E_OK;   // 回復の時の PDU モードの変更は、成功させる
    FakeMillis_Value += static_cast<unsigned long>(CANSM_BUSOFF_RECOVERY_L1_MS) + 1UL;
    CanSM_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ComM_ModeType mode = COMM_NO_COMMUNICATION;
    ASSERT_EQ(CanSM_GetCurrentComMode(0U, &mode), E_OK);
    EXPECT_EQ(ret, E_NOT_OK);                  // TX を止められないので、拒否する
    EXPECT_EQ(mode, COMM_FULL_COMMUNICATION);  // 拒否したので、Bus-Off の前の FULL_COM へ戻る
}

// PRE_NO_COM の最中は、NO_COM へ入り終えるまで、直前のモード（FULL_COM）を返す。入り終えたら NO_COM
TEST_F(CanSM_RequestComMode_Test, CanSM_RequestComMode_OK_CurrentModeStaysUntilNoComIsReached)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanIf_SetControllerMode = 0U;
    ForcedReturn_CanIf_SetControllerMode      = E_OK;
    FailFromCallCount_CanIf_SetPduMode = 0U;
    ForcedReturn_CanIf_SetPduMode      = E_OK;
    ASSERT_EQ(CanSM_RequestComMode(0U, COMM_FULL_COMMUNICATION), E_OK);
    SettleCanSmModeRequests();   // FULL_COM

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ASSERT_EQ(CanSM_RequestComMode(0U, COMM_NO_COMMUNICATION), E_OK);   // NO_COM へ向かう途中
    ComM_ModeType whileGoing = COMM_NO_COMMUNICATION;
    ASSERT_EQ(CanSM_GetCurrentComMode(0U, &whileGoing), E_OK);
    SettleCanSmModeRequests();
    ComM_ModeType after = COMM_FULL_COMMUNICATION;
    ASSERT_EQ(CanSM_GetCurrentComMode(0U, &after), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(whileGoing, COMM_FULL_COMMUNICATION);
    EXPECT_EQ(after, COMM_NO_COMMUNICATION);
}

// コントローラのモードを変えている最中（PRE_FULL_COM）の SILENT_COM は、拒否する。手順は、そのまま続く
TEST_F(CanSM_RequestComMode_Test, CanSM_RequestComMode_NG_SilentComDeniedWhileChangingControllerMode)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanIf_SetControllerMode = 0U;
    ForcedReturn_CanIf_SetControllerMode      = E_OK;
    FailFromCallCount_CanIf_SetPduMode = 0U;
    ForcedReturn_CanIf_SetPduMode      = E_OK;
    ASSERT_EQ(CanSM_RequestComMode(0U, COMM_FULL_COMMUNICATION), E_OK);   // FULL_COM へ向かう途中

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanSM_RequestComMode(0U, COMM_SILENT_COMMUNICATION);
    ComM_ModeType whileGoing = COMM_SILENT_COMMUNICATION;
    ASSERT_EQ(CanSM_GetCurrentComMode(0U, &whileGoing), E_OK);
    SettleCanSmModeRequests();
    ComM_ModeType after = COMM_NO_COMMUNICATION;
    ASSERT_EQ(CanSM_GetCurrentComMode(0U, &after), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(whileGoing, COMM_NO_COMMUNICATION);   // SILENT_COM へは移らない
    EXPECT_EQ(after, COMM_FULL_COMMUNICATION);      // 手順は、そのまま FULL_COM へ入り終える
}

// FULL_COM へ向かう途中で、コントローラが STARTED の段階（STARTED の完了待ち）に Bus-Off になっても、落とさない
TEST_F(CanSM_RequestComMode_Test, CanSM_RequestComMode_OK_BusOffWhileStartingFullComIsRecovered)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanIf_SetControllerMode = 0U;
    ForcedReturn_CanIf_SetControllerMode      = E_OK;
    FailFromCallCount_CanIf_SetPduMode = 0U;
    ForcedReturn_CanIf_SetPduMode      = E_OK;
    ASSERT_EQ(CanSM_RequestComMode(0U, COMM_FULL_COMMUNICATION), E_OK);
    CanSM_ControllerModeIndication(0U, CAN_CS_STOPPED);   // 次に待つのは、STARTED の完了
    WrapDem_Reset();
    WrapComM_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanSM_ControllerBusOff(0U);                           // Bus-Off
    const uint32 demAfterBusOff = CallCount_Dem_SetEventStatus;
    CanSM_ControllerModeIndication(0U, CAN_CS_STARTED);   // 遅れて届いた STARTED の完了
    ComM_ModeType mode = COMM_FULL_COMMUNICATION;
    ASSERT_EQ(CanSM_GetCurrentComMode(0U, &mode), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(demAfterBusOff, 1U);                              // Bus-Off を Dem へ報告した（PREFAILED）
    EXPECT_EQ(CallCount_ComM_BusSM_ModeIndication, 1U);         // ComM へは SILENT_COM（Bus-Off）だけ。FULL_COM へは入らない
    EXPECT_EQ(mode, COMM_NO_COMMUNICATION);                     // Bus-Off の回復中（FULL_COM ではない）
}

}  // namespace
