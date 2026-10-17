/**
 * \file    NvM_RestoreBlockDefaults_test.cpp
 * \brief   NvM_RestoreBlockDefaults() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 NvM_test.cpp）。
 *
 *          `NvM_ReadBlock()`/`NvM_WriteBlock()`/`NvM_RestoreBlockDefaults()` の
 *          `NVM_E_PARAM_ADDRESS`（`blk->RamBlockDataAddress == NULL` かつ
 *          出力/入力ポインタも NULL の場合のみ到達）は、本プロジェクトの実際の
 *          `NvM_Config`（NvM_PBCfg.c）が構成する全ブロックが恒久 RAM ブロック
 *          を持つため到達不能（NvM_Init() が固定の `NvM_Config` を直接参照し、
 *          テストから差し替え不能なため、NvM.c 側のコメントが示すとおり
 *          「本プロジェクトには存在しないが念のため」の防御分岐）。対象外とする。
 *
 *          分割前の共通の説明は、旧 NvM_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "NvM.h"
#include "MemIf.h"
#include "Fake_Det_Hw.h"
#include "NvM_Cfg.h"
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

/* NvM（上記以外の API）の単体テスト用フィクスチャ。
 * SetUp(): MemIf_Init() と NvM_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。範囲外のブロック ID を表す kInvalidBlockId も用意する。
 * TearDown(): 未初期化の状態へ戻す。 */
class NvM_RestoreBlockDefaults_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        MemIf_Init();
        NvM_Init(NULL);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        NvM_Test_ResetInitState();
    }

    static const NvM_BlockIdType kInvalidBlockId = NVM_BLOCK_COUNT;
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(NvM_RestoreBlockDefaults_Test, NvM_RestoreBlockDefaults_NG_NotInitialized)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    NvM_Test_ResetInitState();
    uint8 buf[16];

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = NvM_RestoreBlockDefaults(NVM_BLOCK_ID_DEM_MAGIC, buf);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, NVM_E_NOT_INITIALIZED);
}

TEST_F(NvM_RestoreBlockDefaults_Test, NvM_RestoreBlockDefaults_NG_InvalidBlockId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 buf[16];

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = NvM_RestoreBlockDefaults(kInvalidBlockId, buf);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, NVM_E_PARAM_BLOCK_ID);
}

TEST_F(NvM_RestoreBlockDefaults_Test, NvM_RestoreBlockDefaults_NG_BlockWithoutDefaults)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // NVM_BLOCK_ID_DEM_AGING は RomBlockDataAddress=NULL（NvM_PBCfg.c 参照）。
    uint8 buf[16];

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = NvM_RestoreBlockDefaults(NVM_BLOCK_ID_DEM_AGING, buf);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, NVM_E_BLOCK_WITHOUT_DEFAULTS);
}

TEST_F(NvM_RestoreBlockDefaults_Test, NvM_RestoreBlockDefaults_NG_WriteProtectedBlock)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(NvM_SetBlockProtection(NVM_BLOCK_ID_DEM_STATUS, TRUE), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = NvM_RestoreBlockDefaults(NVM_BLOCK_ID_DEM_STATUS, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    NvM_RequestResultType result = NVM_REQ_OK;
    ASSERT_EQ(NvM_GetErrorStatus(NVM_BLOCK_ID_DEM_STATUS, &result), E_OK);
    EXPECT_NE(result, NVM_REQ_PENDING);   // 書き込みを積まない
}

TEST_F(NvM_RestoreBlockDefaults_Test, NvM_RestoreBlockDefaults_OK_CopiesRomDefaultsAndQueuesWrite)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 pattern[15];
    for (uint8 i = 0U; i < 15U; i++) { pattern[i] = 0xA5U; }
    ASSERT_EQ(NvM_WriteBlock(NVM_BLOCK_ID_DEM_STATUS, pattern), E_OK);   // RAM ミラーを、既定値と異なる内容にする
    uint8 dest[15];
    for (uint8 i = 0U; i < 15U; i++) { dest[i] = 0xEEU; }

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = NvM_RestoreBlockDefaults(NVM_BLOCK_ID_DEM_STATUS, dest);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    uint8 ram[15] = { 0U };
    ASSERT_EQ(NvM_ReadBlock(NVM_BLOCK_ID_DEM_STATUS, ram), E_OK);
    EXPECT_NE(ram[0], 0xA5U);        // RAM ミラーが ROM の既定値へ戻る
    EXPECT_EQ(dest[0], ram[0]);      // 呼び出し元の領域へも、同じ内容を渡す
    EXPECT_EQ(dest[14], ram[14]);
}

}  // namespace
