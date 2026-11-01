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

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
}

TEST_F(CanSM_RequestComMode_Test, CanSM_RequestComMode_NG_FullComFailsWhenControllerCannotStart)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanIf_SetControllerMode = 0U;   // コントローラの状態変更は、常に失敗する
    ForcedReturn_CanIf_SetControllerMode      = E_NOT_OK;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanSM_RequestComMode(0U, COMM_FULL_COMMUNICATION);
    ComM_ModeType mode = COMM_FULL_COMMUNICATION;
    const Std_ReturnType retMode = CanSM_GetCurrentComMode(0U, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    ASSERT_EQ(retMode, E_OK);
    EXPECT_EQ(mode, COMM_NO_COMMUNICATION);   // 状態は変わらない
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
    FailFromCallCount_CanIf_SetPduMode = 0U;   // PDU モードの変更は、常に失敗する
    ForcedReturn_CanIf_SetPduMode      = E_NOT_OK;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanSM_RequestComMode(0U, COMM_SILENT_COMMUNICATION);
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
    ASSERT_EQ(CanSM_GetCurrentComMode(0U, &full), E_OK);
    ASSERT_EQ(CanSM_RequestComMode(0U, COMM_SILENT_COMMUNICATION), E_OK);
    ASSERT_EQ(CanSM_GetCurrentComMode(0U, &silent), E_OK);
    ASSERT_EQ(CanSM_RequestComMode(0U, COMM_NO_COMMUNICATION), E_OK);
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
    CanSM_ControllerBusOff(0U);   // Bus-Off の回復中になる
    const uint32 controllerCallsBefore = CallCount_CanIf_SetControllerMode;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanSM_RequestComMode(0U, COMM_NO_COMMUNICATION);
    FakeMillis_Value += static_cast<unsigned long>(CANSM_BUSOFF_RECOVERY_L1_MS) + 1UL;
    CanSM_MainFunction();   // 回復のタイマが満了しても、回復しない

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ComM_ModeType mode = COMM_FULL_COMMUNICATION;
    ASSERT_EQ(CanSM_GetCurrentComMode(0U, &mode), E_OK);
    EXPECT_EQ(ret, E_OK);                                        // 受理する（[SWS_CanSM_00369]）。以前は E_NOT_OK で捨てていた
    EXPECT_EQ(mode, COMM_NO_COMMUNICATION);                      // すぐに、NO_COM へ移る
    EXPECT_EQ(CallCount_CanIf_SetControllerMode, controllerCallsBefore + 1U);   // スリープの要求の 1 回だけ。回復の再起動はしない
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
    ASSERT_EQ(CanSM_RequestComMode(0U, COMM_SILENT_COMMUNICATION), E_OK);   // Bus-Off の前は SILENT_COM
    CanSM_ControllerBusOff(0U);   // Bus-Off の回復中になる
    const uint32 pduModeCallsBefore = CallCount_CanIf_SetPduMode;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanSM_RequestComMode(0U, COMM_FULL_COMMUNICATION);   // 回復の最中に、FULL_COM を要求する
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
    CanSM_ControllerBusOff(0U);   // Bus-Off の回復中になる
    const uint32 pduModeCallsBefore = CallCount_CanIf_SetPduMode;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanSM_RequestComMode(0U, COMM_SILENT_COMMUNICATION);   // 回復の最中に、SILENT_COM を要求する
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
    CanSM_ControllerBusOff(0U);   // Bus-Off の回復中になる
    FailFromCallCount_CanIf_SetPduMode = 1U;   // これ以降の PDU モードの変更は、失敗する
    ForcedReturn_CanIf_SetPduMode      = E_NOT_OK;
    CallCount_CanIf_SetPduMode = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanSM_RequestComMode(0U, COMM_SILENT_COMMUNICATION);
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

}  // namespace
