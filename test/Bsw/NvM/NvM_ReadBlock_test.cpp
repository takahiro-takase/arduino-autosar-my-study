/**
 * \file    NvM_ReadBlock_test.cpp
 * \brief   NvM_ReadBlock() の単体テスト
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
class NvM_ReadBlock_Test : public ::testing::Test
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

TEST_F(NvM_ReadBlock_Test, NvM_ReadBlock_NG_NotInitialized)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    NvM_Test_ResetInitState();
    uint8 buf[16];

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = NvM_ReadBlock(NVM_BLOCK_ID_DEM_MAGIC, buf);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, NVM_E_NOT_INITIALIZED);
}

TEST_F(NvM_ReadBlock_Test, NvM_ReadBlock_NG_InvalidBlockId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 buf[16];

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = NvM_ReadBlock(kInvalidBlockId, buf);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, NVM_E_PARAM_BLOCK_ID);
}

}  // namespace
