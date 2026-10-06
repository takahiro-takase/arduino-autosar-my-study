/**
 * \file    Bsw_E2EXf_test.cpp
 * \brief   E2EXf.c（src/Bsw/E2EXf/E2EXf.c）の単体テスト（NG系）
 * \details E2EXf.c 全公開APIの `Det_ReportError()` 呼び出し箇所（NG ケースの
 *          み）を1ファイルにまとめる。各ケースで、報告される ErrorId が
 *          仕様どおり正しい値になっていることを `Fake_Det_Hw.h`
 *          （`FakeDetHw_LastErrorId`）で検証する。
 *
 *          `E2EXf_Inv_EngineInfo()`/`E2EXf_Inv_AbsInfo()` それぞれの
 *          「`E2E_P05Check()` 自体が失敗した場合」の防御分岐
 *          （`E2EXF_E_PARAM_POINTER` 追加報告）は、両関数とも
 *          `E2E_P05Check()` へ常に固定長（`E2EConfig->DataLength` そのもの）を
 *          渡す呼び出し規約のため、`E2E_P05.c` 内の「wrong length」条件
 *          （`Length != Config->DataLength`）に到達せず、かつ Config/State も
 *          常に非NULLの static 構造体を渡すため「NULL」条件にも到達しない
 *          （E2EXf.c 内のコメント「通常は到達しない」、E2E_P05.c 内のコメント
 *          「この分岐自体は現状到達しない」といずれも明記されている）。対象外
 *          とする。
 *
 *          E2EXf には `E2EXf_DeInit()` が存在するため、Mcu/PduR/CanTp と異なり
 *          「未初期化状態」の検証にテスト専用のリセット関数は不要（各テストが
 *          明示的に `E2EXf_DeInit()` を呼んでから検証する）。
 *
 *          GoogleTest の main() は test_main.cpp に集約しているため、
 *          本ファイルでは定義しない。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "E2EXf.h"
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

/* E2EXf（E2E Transformer）の単体テスト用フィクスチャ。
 * SetUp(): E2EXf_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): E2EXf_DeInit() で未初期化へ戻す。 */
class Bsw_E2EXf_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        E2EXf_Init(NULL);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        E2EXf_DeInit();  // 既に未初期化でも Det 報告されるだけで無害
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

// ------------------------------------------------------------
// E2EXf_E2EHealthStatus()
// ------------------------------------------------------------

TEST_F(Bsw_E2EXf_Test, E2EXf_E2EHealthStatus_NG_NullBufferLengthPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 buf[5];

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    uint8 ret = E2EXf_E2EHealthStatus(buf, NULL, NULL, 0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_SAFETY_HARD_RUNTIMEERROR);
    EXPECT_EQ(FakeDetHw_LastErrorId, E2EXF_E_PARAM_POINTER);
}

TEST_F(Bsw_E2EXf_Test, E2EXf_E2EHealthStatus_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    E2EXf_DeInit();
    uint8  buf[5];
    uint32 bufLen = sizeof(buf);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    uint8 ret = E2EXf_E2EHealthStatus(buf, &bufLen, NULL, 0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_SAFETY_HARD_RUNTIMEERROR);
    EXPECT_EQ(FakeDetHw_LastErrorId, E2EXF_E_UNINIT);
}

TEST_F(Bsw_E2EXf_Test, E2EXf_E2EHealthStatus_NG_NullBufferPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint32 bufLen = 5U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    uint8 ret = E2EXf_E2EHealthStatus(NULL, &bufLen, NULL, 0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_SAFETY_HARD_RUNTIMEERROR);
    EXPECT_EQ(FakeDetHw_LastErrorId, E2EXF_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// E2EXf_Inv_EngineInfo()
// ------------------------------------------------------------

TEST_F(Bsw_E2EXf_Test, E2EXf_Inv_EngineInfo_NG_NullCheckStatusPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8  buf[7];
    uint32 bufLen = sizeof(buf);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    uint8 ret = E2EXf_Inv_EngineInfo(buf, &bufLen, NULL, 0U, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_SAFETY_HARD_RUNTIMEERROR);
    EXPECT_EQ(FakeDetHw_LastErrorId, E2EXF_E_PARAM_POINTER);
}

TEST_F(Bsw_E2EXf_Test, E2EXf_Inv_EngineInfo_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    E2EXf_DeInit();
    uint8  buf[7];
    uint32 bufLen = sizeof(buf);
    E2E_P05StatusType checkStatus;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    uint8 ret = E2EXf_Inv_EngineInfo(buf, &bufLen, NULL, 0U, &checkStatus);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_SAFETY_HARD_RUNTIMEERROR);
    EXPECT_EQ(checkStatus, E2E_P05STATUS_ERROR);
    EXPECT_EQ(FakeDetHw_LastErrorId, E2EXF_E_UNINIT);
}

TEST_F(Bsw_E2EXf_Test, E2EXf_Inv_EngineInfo_NG_NullBufferPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint32 bufLen = 7U;
    E2E_P05StatusType checkStatus;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    uint8 ret = E2EXf_Inv_EngineInfo(NULL, &bufLen, NULL, 0U, &checkStatus);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_SAFETY_HARD_RUNTIMEERROR);
    EXPECT_EQ(checkStatus, E2E_P05STATUS_ERROR);
    EXPECT_EQ(FakeDetHw_LastErrorId, E2EXF_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// E2EXf_Inv_AbsInfo()
// ------------------------------------------------------------

TEST_F(Bsw_E2EXf_Test, E2EXf_Inv_AbsInfo_NG_NullCheckStatusPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8  buf[6];
    uint32 bufLen = sizeof(buf);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    uint8 ret = E2EXf_Inv_AbsInfo(buf, &bufLen, NULL, 0U, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_SAFETY_HARD_RUNTIMEERROR);
    EXPECT_EQ(FakeDetHw_LastErrorId, E2EXF_E_PARAM_POINTER);
}

TEST_F(Bsw_E2EXf_Test, E2EXf_Inv_AbsInfo_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    E2EXf_DeInit();
    uint8  buf[6];
    uint32 bufLen = sizeof(buf);
    E2E_P05StatusType checkStatus;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    uint8 ret = E2EXf_Inv_AbsInfo(buf, &bufLen, NULL, 0U, &checkStatus);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_SAFETY_HARD_RUNTIMEERROR);
    EXPECT_EQ(checkStatus, E2E_P05STATUS_ERROR);
    EXPECT_EQ(FakeDetHw_LastErrorId, E2EXF_E_UNINIT);
}

TEST_F(Bsw_E2EXf_Test, E2EXf_Inv_AbsInfo_NG_NullBufferPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint32 bufLen = 6U;
    E2E_P05StatusType checkStatus;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    uint8 ret = E2EXf_Inv_AbsInfo(NULL, &bufLen, NULL, 0U, &checkStatus);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_SAFETY_HARD_RUNTIMEERROR);
    EXPECT_EQ(checkStatus, E2E_P05STATUS_ERROR);
    EXPECT_EQ(FakeDetHw_LastErrorId, E2EXF_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// E2EXf_DeInit()
// ------------------------------------------------------------

TEST_F(Bsw_E2EXf_Test, E2EXf_DeInit_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    E2EXf_DeInit();  // 1回目: SetUp() の Init を正常に解除する
    FakeDetHw_Reset();

    E2EXf_DeInit();  // 2回目: 既に未初期化のため NG

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, E2EXF_E_UNINIT);
}

// ------------------------------------------------------------
// E2EXf_GetVersionInfo()（他BSWモジュール共通の慣例により未初期化チェック対象外）
// ------------------------------------------------------------

TEST_F(Bsw_E2EXf_Test, E2EXf_GetVersionInfo_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    E2EXf_GetVersionInfo(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, E2EXF_E_PARAM_POINTER);
}

}  // namespace
