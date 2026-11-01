/**
 * \file    CanSM_ControllerBusOff_test.cpp
 * \brief   CanSM_ControllerBusOff() の単体テスト
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
class CanSM_ControllerBusOff_Test : public ::testing::Test
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

TEST_F(CanSM_ControllerBusOff_Test, CanSM_ControllerBusOff_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanSM_DeInit();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanSM_ControllerBusOff(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, CANSM_E_UNINIT);
}

TEST_F(CanSM_ControllerBusOff_Test, CanSM_ControllerBusOff_NG_InvalidController)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanSM_ControllerBusOff(1U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, CANSM_E_PARAM_CONTROLLER);
}

TEST_F(CanSM_ControllerBusOff_Test, CanSM_ControllerBusOff_OK_IgnoredOutsideFullAndSilentCom)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    const uint32 setModeBefore = CallCount_CanIf_SetControllerMode;   // 初期化直後は NO_COM

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanSM_ControllerBusOff(0U);
    ComM_ModeType mode = COMM_FULL_COMMUNICATION;
    const Std_ReturnType retMode = CanSM_GetCurrentComMode(0U, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(retMode, E_OK);
    EXPECT_EQ(mode, COMM_NO_COMMUNICATION);                            // 状態は変わらない（回復シーケンスを始めない）
    EXPECT_EQ(CallCount_CanIf_SetControllerMode, setModeBefore);       // コントローラの停止も要求しない
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(CanSM_ControllerBusOff_Test, CanSM_ControllerBusOff_OK_ProceedsToBusOffEvenWhenControllerStopFails)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanIf_SetControllerMode = 0U;   // コントローラの状態変更は、常に成功する
    ForcedReturn_CanIf_SetControllerMode      = E_OK;
    FailFromCallCount_CanIf_SetPduMode = 0U;   // PDU モードの変更は、常に成功する
    ForcedReturn_CanIf_SetPduMode      = E_OK;
    ASSERT_EQ(CanSM_RequestComMode(0U, COMM_FULL_COMMUNICATION), E_OK);
    FailFromCallCount_CanIf_SetControllerMode = 0U;   // コントローラの状態変更は、常に失敗する
    ForcedReturn_CanIf_SetControllerMode      = E_NOT_OK;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanSM_ControllerBusOff(0U);
    ComM_ModeType mode = COMM_NO_COMMUNICATION;
    const Std_ReturnType retMode = CanSM_GetCurrentComMode(0U, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_EQ(retMode, E_OK);
    EXPECT_NE(mode, COMM_FULL_COMMUNICATION);   // 停止の要求が失敗しても、BUS_OFF の状態（上位へは SILENT）へ移る
}

// [SWS_CanSM_00522]/[00605]: Bus-Off を検出するたびに、Dem へ CAN_BUSOFF の PREFAILED を報告する
TEST_F(CanSM_ControllerBusOff_Test, CanSM_ControllerBusOff_OK_ReportsPreFailedToDemOnEachDetection)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanIf_SetControllerMode = 0U;   // コントローラの状態変更は、常に成功する
    ForcedReturn_CanIf_SetControllerMode      = E_OK;
    FailFromCallCount_CanIf_SetPduMode = 0U;   // PDU モードの変更は、常に成功する
    ForcedReturn_CanIf_SetPduMode      = E_OK;
    ASSERT_EQ(CanSM_RequestComMode(0U, COMM_FULL_COMMUNICATION), E_OK);
    WrapDem_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanSM_ControllerBusOff(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Dem_SetEventStatus, 1U);
    EXPECT_EQ(LastEventId_Dem_SetEventStatus, DemConf_DemEventParameter_CAN_BUSOFF);
    EXPECT_EQ(LastEventStatus_Dem_SetEventStatus, DEM_EVENT_STATUS_PREFAILED);
}

// 確定していない Bus-Off の検出中（BUS_OFF の状態）に、もう一度 Bus-Off が通知されても、報告を重ねない
TEST_F(CanSM_ControllerBusOff_Test, CanSM_ControllerBusOff_OK_DoesNotReportAgainWhileAlreadyBusOff)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanIf_SetControllerMode = 0U;
    ForcedReturn_CanIf_SetControllerMode      = E_OK;
    FailFromCallCount_CanIf_SetPduMode = 0U;
    ForcedReturn_CanIf_SetPduMode      = E_OK;
    ASSERT_EQ(CanSM_RequestComMode(0U, COMM_FULL_COMMUNICATION), E_OK);
    CanSM_ControllerBusOff(0U);   // 1 回目（BUS_OFF の状態へ入る）
    WrapDem_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanSM_ControllerBusOff(0U);   // 2 回目（回復の途中。無視する）

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Dem_SetEventStatus, 0U);
}

}  // namespace
