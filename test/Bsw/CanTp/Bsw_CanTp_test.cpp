/**
 * \file    Bsw_CanTp_test.cpp
 * \brief   CanTp.c（src/Bsw/CanTp/CanTp.c）の単体テスト（NG系）
 * \details CanTp は `test/Bsw/DcmStack/`/`test/Bsw/ComStack/` 等の複数モジュール
 *          結合テストでのみ間接的に検証されており、モジュール単品での標準
 *          テストファイルが存在しなかった。本ファイルは CanTp.c の全
 *          `Det_ReportError()` 呼び出し箇所（NG ケースのみ）を1ファイルに
 *          まとめ、報告される ErrorId が仕様どおり正しい値になっていることを
 *          `Fake_Det_Hw.h`（`FakeDetHw_LastErrorId`）で検証する。
 *
 *          CanTp には DeInit() に相当する API が無く、`CanTp_Initialized` は
 *          native_chain_tests バイナリ全体で共有される static のため、
 *          `CANTP_E_UNINIT` の検証には `CanTp_Test_ResetInitState()`
 *          （`CANTP_UNIT_TEST` ビルドのみに存在するテスト専用関数、
 *          `Mcu_Test_ResetInitState()` と同じ設計）を使う。
 *
 *          GoogleTest の main() は test_main.cpp に集約しているため、
 *          本ファイルでは定義しない。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "CanTp.h"
#include "Fake_Det_Hw.h"
#include "Fake_Millis.h"
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

/* CanTp の単体テスト用フィクスチャ。
 * SetUp(): 時刻を初期化し、CanTp_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。 */
class Bsw_CanTp_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        CanTp_Init(NULL);              // CfgPtr は常に NULL（CanTp.h 参照）
        FakeDetHw_Reset();              // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    // CanTp に DeInit() は無いため TearDown は書かない。未初期化状態が必要な
    // テストは各自 CanTp_Test_ResetInitState() を明示的に呼ぶ（ファイル冒頭
    // コメント参照）。
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

// ------------------------------------------------------------
// CanTp_GetVersionInfo()
// ------------------------------------------------------------

TEST_F(Bsw_CanTp_Test, CanTp_GetVersionInfo_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTp_GetVersionInfo(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, CANTP_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// CanTp_Transmit()
// ------------------------------------------------------------

TEST_F(Bsw_CanTp_Test, CanTp_Transmit_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanTp_Test_ResetInitState();
    uint8 sdu[4] = { 0 };
    PduInfoType info = { sdu, 4U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CanTp_Transmit(CANTP_TX_SDU_ID, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANTP_E_UNINIT);
}

TEST_F(Bsw_CanTp_Test, CanTp_Transmit_NG_InvalidTxSduId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 sdu[4] = { 0 };
    PduInfoType info = { sdu, 4U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CanTp_Transmit(CANTP_TX_SDU_ID + 1U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANTP_E_INVALID_TX_ID);
}

TEST_F(Bsw_CanTp_Test, CanTp_Transmit_NG_NullPduInfoPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CanTp_Transmit(CANTP_TX_SDU_ID, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANTP_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// CanTp_MainFunction()
// ------------------------------------------------------------

TEST_F(Bsw_CanTp_Test, CanTp_MainFunction_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanTp_Test_ResetInitState();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTp_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, CANTP_E_UNINIT);
}

// ------------------------------------------------------------
// CanTp_RxIndication()
// ------------------------------------------------------------

TEST_F(Bsw_CanTp_Test, CanTp_RxIndication_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanTp_Test_ResetInitState();
    uint8 sdu[8] = { 0 };
    PduInfoType info = { sdu, 8U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTp_RxIndication(CANTP_RX_SDU_ID, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, CANTP_E_UNINIT);
}

TEST_F(Bsw_CanTp_Test, CanTp_RxIndication_NG_InvalidRxPduId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 sdu[8] = { 0 };
    PduInfoType info = { sdu, 8U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTp_RxIndication(CANTP_RX_SDU_ID + 1U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, CANTP_E_INVALID_RX_ID);
}

TEST_F(Bsw_CanTp_Test, CanTp_RxIndication_NG_NullPduInfoPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTp_RxIndication(CANTP_RX_SDU_ID, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, CANTP_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// CanTp_TxConfirmation()
// ------------------------------------------------------------

TEST_F(Bsw_CanTp_Test, CanTp_TxConfirmation_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanTp_Test_ResetInitState();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTp_TxConfirmation(CANTP_TX_SDU_ID, E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, CANTP_E_UNINIT);
}

}  // namespace
