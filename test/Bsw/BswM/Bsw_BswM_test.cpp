/**
 * \file    Bsw_BswM_test.cpp
 * \brief   BswM.c（src/Bsw/BswM/BswM.c）の単体テスト（NG系）
 * \details BswM.c 全公開APIの `Det_ReportError()` 呼び出し箇所（NG ケースのみ）を
 *          1ファイルにまとめる。各ケースで、報告される ErrorId が仕様どおり
 *          正しい値になっていることを `Fake_Det_Hw.h`（`FakeDetHw_LastErrorId`）
 *          で検証する。
 *
 *          BswM には `BswM_Deinit()` が存在するため、Mcu/PduR/CanTp と異なり
 *          「未初期化状態」の検証にテスト専用のリセット関数は不要（各テストが
 *          明示的に `BswM_Deinit()` を呼んでから検証する）。
 *
 *          GoogleTest の main() は test_main.cpp に集約しているため、
 *          本ファイルでは定義しない。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "BswM.h"
#include "BswM_PBCfg.h"
#include "Wrap_BswM.h"
#include "Fake_Det_Hw.h"
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

/* BswM の単体テスト用フィクスチャ。
 * SetUp(): wrap の状態を戻し、BswM_Init(&BswM_Config) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): BswM_Deinit() で未初期化へ戻す。 */
class Bsw_BswM_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        WrapBswM_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        BswM_Init(&BswM_Config);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        BswM_Deinit();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

// ------------------------------------------------------------
// BswM_Init()
// ------------------------------------------------------------

TEST_F(Bsw_BswM_Test, BswM_Init_NG_NullConfigPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    BswM_Deinit();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BswM_Init(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, BSWM_E_PARAM_CONFIG);
}

// ------------------------------------------------------------
// BswM_Deinit()
// ------------------------------------------------------------

TEST_F(Bsw_BswM_Test, BswM_Deinit_NG_NoInit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BswM_Deinit();  // 1回目: SetUp() の Init を正常に解除する
    FakeDetHw_Reset();

    BswM_Deinit();  // 2回目: 既に未初期化のため NG

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, BSWM_E_NO_INIT);
}

// ------------------------------------------------------------
// BswM_GetVersionInfo()
// ------------------------------------------------------------

TEST_F(Bsw_BswM_Test, BswM_GetVersionInfo_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BswM_GetVersionInfo(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, BSWM_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// BswM_EcuM_CurrentState()
// ------------------------------------------------------------

TEST_F(Bsw_BswM_Test, BswM_EcuM_CurrentState_NG_NoInit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    BswM_Deinit();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BswM_EcuM_CurrentState(ECUM_STATE_RUN);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, BSWM_E_NO_INIT);
}

TEST_F(Bsw_BswM_Test, BswM_EcuM_CurrentState_NG_ReqModeOutOfRange)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BswM_EcuM_CurrentState(static_cast<EcuM_StateType>(0xFFU));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, BSWM_E_REQ_MODE_OUT_OF_RANGE);
}

// ------------------------------------------------------------
// BswM_ComM_CurrentMode()
// ------------------------------------------------------------

TEST_F(Bsw_BswM_Test, BswM_ComM_CurrentMode_NG_NoInit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    BswM_Deinit();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BswM_ComM_CurrentMode(0U, COMM_FULL_COMMUNICATION);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, BSWM_E_NO_INIT);
}

TEST_F(Bsw_BswM_Test, BswM_ComM_CurrentMode_NG_ReqModeOutOfRange)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BswM_ComM_CurrentMode(0U, static_cast<ComM_ModeType>(COMM_FULL_COMMUNICATION + 1U));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, BSWM_E_REQ_MODE_OUT_OF_RANGE);
}

// ------------------------------------------------------------
// BswM_Dcm_CommunicationMode_CurrentState()
// ------------------------------------------------------------

TEST_F(Bsw_BswM_Test, BswM_Dcm_CommunicationMode_CurrentState_NG_NoInit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    BswM_Deinit();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BswM_Dcm_CommunicationMode_CurrentState(0U, DCM_DISABLE_RX_TX_NORM_NM);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, BSWM_E_NO_INIT);
}

TEST_F(Bsw_BswM_Test, BswM_Dcm_CommunicationMode_CurrentState_NG_ReqModeOutOfRange)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BswM_Dcm_CommunicationMode_CurrentState(
        0U, static_cast<Dcm_CommunicationModeType>(DCM_DISABLE_RX_TX_NORM_NM + 1U));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, BSWM_E_REQ_MODE_OUT_OF_RANGE);
}

}  // namespace
