/**
 * \file    CanSM_ControllerModeIndication_test.cpp
 * \brief   CanSM_ControllerModeIndication() の単体テスト
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
class CanSM_ControllerModeIndication_Test : public ::testing::Test
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

TEST_F(CanSM_ControllerModeIndication_Test, CanSM_ControllerModeIndication_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanSM_DeInit();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanSM_ControllerModeIndication(0U, CAN_CS_STARTED);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, CANSM_E_UNINIT);
}

TEST_F(CanSM_ControllerModeIndication_Test, CanSM_ControllerModeIndication_NG_InvalidController)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanSM_ControllerModeIndication(1U, CAN_CS_STARTED);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, CANSM_E_PARAM_CONTROLLER);
}

// 完了通知は、現在は受け取るだけで、状態を変えない（コントローラへも何も要求しない）
TEST_F(CanSM_ControllerModeIndication_Test, CanSM_ControllerModeIndication_OK_DoesNotChangeStateInFullCom)
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
    const uint32 setModeBefore = CallCount_CanIf_SetControllerMode;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanSM_ControllerModeIndication(0U, CAN_CS_STARTED);
    ComM_ModeType mode = COMM_NO_COMMUNICATION;
    const Std_ReturnType retMode = CanSM_GetCurrentComMode(0U, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_EQ(retMode, E_OK);
    EXPECT_EQ(mode, COMM_FULL_COMMUNICATION);                          // 状態は変わらない
    EXPECT_EQ(CallCount_CanIf_SetControllerMode, setModeBefore);       // コントローラへも何も要求しない
}

// NO_COM で STOPPED の完了通知を受け取っても、起床とはみなさない（起床の検証は、CanSM_StartWakeupSource() で始める）
TEST_F(CanSM_ControllerModeIndication_Test, CanSM_ControllerModeIndication_OK_StoppedIndicationInNoComIsNotAWakeup)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanIf_SetControllerMode = 0U;
    ForcedReturn_CanIf_SetControllerMode      = E_OK;
    const uint32 setModeBefore = CallCount_CanIf_SetControllerMode;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanSM_ControllerModeIndication(0U, CAN_CS_STOPPED);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_CanIf_SetControllerMode, setModeBefore);   // 検証中へ進むための要求を出さない
}

// FULL_COM へ向かう途中は、待っているモードの完了通知だけが、手順を進める（待っていないモードの通知は、無視する）
TEST_F(CanSM_ControllerModeIndication_Test, CanSM_ControllerModeIndication_OK_OnlyTheAwaitedModeAdvancesTheSequence)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanIf_SetControllerMode = 0U;
    ForcedReturn_CanIf_SetControllerMode      = E_OK;
    FailFromCallCount_CanIf_SetPduMode = 0U;
    ForcedReturn_CanIf_SetPduMode      = E_OK;
    ASSERT_EQ(CanSM_RequestComMode(0U, COMM_FULL_COMMUNICATION), E_OK);   // 最初に待つのは STOPPED の完了

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanSM_ControllerModeIndication(0U, CAN_CS_STARTED);   // 待っていないモード（2 回流しても、進まない）
    CanSM_ControllerModeIndication(0U, CAN_CS_STARTED);
    ComM_ModeType afterWrong = COMM_FULL_COMMUNICATION;
    ASSERT_EQ(CanSM_GetCurrentComMode(0U, &afterWrong), E_OK);
    CanSM_ControllerModeIndication(0U, CAN_CS_STOPPED);
    CanSM_ControllerModeIndication(0U, CAN_CS_STARTED);
    ComM_ModeType afterAll = COMM_NO_COMMUNICATION;
    ASSERT_EQ(CanSM_GetCurrentComMode(0U, &afterAll), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(afterWrong, COMM_NO_COMMUNICATION);   // 進まない
    EXPECT_EQ(afterAll, COMM_FULL_COMMUNICATION);
}

// 手順の最中ではない（FULL_COM のまま）とき、完了通知は、何も起こさない
TEST_F(CanSM_ControllerModeIndication_Test, CanSM_ControllerModeIndication_OK_IndicationWhileNotWaitingIsIgnored)
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
    const uint32 setModeBefore = CallCount_CanIf_SetControllerMode;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanSM_ControllerModeIndication(0U, CAN_CS_SLEEP);
    CanSM_ControllerModeIndication(0U, CAN_CS_STOPPED);
    ComM_ModeType mode = COMM_NO_COMMUNICATION;
    ASSERT_EQ(CanSM_GetCurrentComMode(0U, &mode), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(mode, COMM_FULL_COMMUNICATION);
    EXPECT_EQ(CallCount_CanIf_SetControllerMode, setModeBefore);
}

}  // namespace
