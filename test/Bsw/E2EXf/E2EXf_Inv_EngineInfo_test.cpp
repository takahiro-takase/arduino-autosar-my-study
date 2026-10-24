/**
 * \file    E2EXf_Inv_EngineInfo_test.cpp
 * \brief   E2EXf_Inv_EngineInfo() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 E2EXf_test.cpp）。
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
 *          分割前の共通の説明は、旧 E2EXf_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "E2EXf.h"
#include "Fake_Det_Hw.h"
#include "Wrap_E2E.h"
#include "Wrap_E2E_P05.h"
#include "E2EXf_PBCfg.h"
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
class E2EXf_Inv_EngineInfo_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        WrapE2E_Reset();               // 他のテストファイルの故障注入（E2E_SMCheck など）が漏れ伝わらないようリセット
        WrapE2EP05_Reset();
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

TEST_F(E2EXf_Inv_EngineInfo_Test, E2EXf_Inv_EngineInfo_NG_Uninit)
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
    uint8 ret = E2EXf_Inv_EngineInfo(buf, &bufLen, NULL, 7U, &checkStatus);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_SAFETY_HARD_RUNTIMEERROR);
    EXPECT_EQ(checkStatus, E2E_P05STATUS_ERROR);
    EXPECT_EQ(FakeDetHw_LastErrorId, E2EXF_E_UNINIT);
}

TEST_F(E2EXf_Inv_EngineInfo_Test, E2EXf_Inv_EngineInfo_NG_NullCheckStatusPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8  buf[7];
    uint32 bufLen = sizeof(buf);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    uint8 ret = E2EXf_Inv_EngineInfo(buf, &bufLen, NULL, 7U, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_SAFETY_HARD_RUNTIMEERROR);
    EXPECT_EQ(FakeDetHw_LastErrorId, E2EXF_E_PARAM_POINTER);
}

TEST_F(E2EXf_Inv_EngineInfo_Test, E2EXf_Inv_EngineInfo_NG_NullBufferPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint32 bufLen = 7U;
    E2E_P05StatusType checkStatus;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    uint8 ret = E2EXf_Inv_EngineInfo(NULL, &bufLen, NULL, 7U, &checkStatus);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_SAFETY_HARD_RUNTIMEERROR);
    EXPECT_EQ(checkStatus, E2E_P05STATUS_ERROR);
    EXPECT_EQ(FakeDetHw_LastErrorId, E2EXF_E_PARAM_POINTER);
}

TEST_F(E2EXf_Inv_EngineInfo_Test, E2EXf_Inv_EngineInfo_NG_NullBufferLengthPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 buf[7] = { 0U };
    E2E_P05StatusType checkStatus;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    uint8 ret = E2EXf_Inv_EngineInfo(buf, NULL, NULL, 7U, &checkStatus);   // CheckStatus は有効、bufferLength だけ NULL

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_SAFETY_HARD_RUNTIMEERROR);
    EXPECT_EQ(FakeDetHw_LastErrorId, E2EXF_E_PARAM_POINTER);
}

TEST_F(E2EXf_Inv_EngineInfo_Test, E2EXf_Inv_EngineInfo_NG_WrongInputLength)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8  buf[7] = { 0U };
    uint32 bufLen = sizeof(buf);
    E2E_P05StatusType checkStatus = E2E_P05STATUS_OK;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    uint8 ret = E2EXf_Inv_EngineInfo(buf, &bufLen, NULL, 6U, &checkStatus);   // PDU の長さ（DataLength = 7）と合わない

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_SAFETY_HARD_RUNTIMEERROR);
    EXPECT_EQ(checkStatus, E2E_P05STATUS_ERROR);
    EXPECT_EQ(FakeDetHw_LastErrorId, E2EXF_E_PARAM);
}

TEST_F(E2EXf_Inv_EngineInfo_Test, E2EXf_Inv_EngineInfo_NG_InputLengthAboveUint16)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8  buf[7] = { 0U };
    uint32 bufLen = sizeof(buf);
    E2E_P05StatusType checkStatus = E2E_P05STATUS_OK;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    uint8 ret = E2EXf_Inv_EngineInfo(buf, &bufLen, NULL, 0x10000U + 7U, &checkStatus);   // 切り詰めると 7 に見える長さ

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_SAFETY_HARD_RUNTIMEERROR);
    EXPECT_EQ(checkStatus, E2E_P05STATUS_ERROR);
    EXPECT_EQ(FakeDetHw_LastErrorId, E2EXF_E_PARAM);
}

TEST_F(E2EXf_Inv_EngineInfo_Test, E2EXf_Inv_EngineInfo_OK_NoNewDataIsReportedAsNoNewData)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    E2EXf_PBCfg_Init();   // 状態機械と初回受信の待ちを初期状態へ戻す
    uint32 bufLen = 99U;
    E2E_P05StatusType checkStatus = E2E_P05STATUS_OK;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    uint8 ret = E2EXf_Inv_EngineInfo(NULL, &bufLen, NULL, 0U, &checkStatus);   // buffer が NULL で長さ 0: 新しいデータが無い（[SWS_E2EXf_00105]）

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_NE(ret, E_SAFETY_HARD_RUNTIMEERROR);
    EXPECT_EQ(ret & 0x0FU, (uint8)E2E_P_NONEWDATA);   // 戻り値の下位ニブルが NONEWDATA（[SWS_E2EXf_00027]）
    EXPECT_EQ(checkStatus, E2E_P05STATUS_NONEWDATA);
    EXPECT_EQ(bufLen, 0U);                            // [SWS_E2EXf_00114]
}

}  // namespace
