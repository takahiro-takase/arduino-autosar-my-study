/**
 * \file    NvM_SetBlockProtection_test.cpp
 * \brief   NvM_SetBlockProtection() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 NvM_test.cpp）。
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
class NvM_SetBlockProtection_Test : public ::testing::Test
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

TEST_F(NvM_SetBlockProtection_Test, NvM_SetBlockProtection_NG_NotInitialized)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    NvM_Test_ResetInitState();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = NvM_SetBlockProtection(NVM_BLOCK_ID_DEM_MAGIC, TRUE);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, NVM_E_NOT_INITIALIZED);
}

TEST_F(NvM_SetBlockProtection_Test, NvM_SetBlockProtection_NG_InvalidBlockId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = NvM_SetBlockProtection(kInvalidBlockId, TRUE);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, NVM_E_PARAM_BLOCK_ID);
}

TEST_F(NvM_SetBlockProtection_Test, NvM_SetBlockProtection_NG_BlockPending)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // NVM_BLOCK_ID_DEM_EXTENDED は Redundant=1（UseCrcCompMechanism 対象外、
    // NvM_PBCfg.c 参照）のため、内容に関わらず必ず非同期ジョブへ積まれる。
    ASSERT_EQ(NvM_WriteBlock(NVM_BLOCK_ID_DEM_EXTENDED, NULL), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = NvM_SetBlockProtection(NVM_BLOCK_ID_DEM_EXTENDED, TRUE);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, NVM_E_BLOCK_PENDING);
}

TEST_F(NvM_SetBlockProtection_Test, NvM_SetBlockProtection_OK_ProtectionBlocksWritesUntilReleased)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 pattern[15];
    for (uint8 i = 0U; i < 15U; i++) { pattern[i] = 0xA5U; }

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType retSet = NvM_SetBlockProtection(NVM_BLOCK_ID_DEM_STATUS, TRUE);
    Std_ReturnType retWriteProtected = NvM_WriteBlock(NVM_BLOCK_ID_DEM_STATUS, pattern);
    Std_ReturnType retRelease = NvM_SetBlockProtection(NVM_BLOCK_ID_DEM_STATUS, FALSE);
    Std_ReturnType retWriteReleased = NvM_WriteBlock(NVM_BLOCK_ID_DEM_STATUS, pattern);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(retSet, E_OK);
    EXPECT_EQ(retWriteProtected, E_NOT_OK);   // 保護中は書き込めない
    EXPECT_EQ(retRelease, E_OK);
    EXPECT_EQ(retWriteReleased, E_OK);        // 解除すれば書き込める
}

}  // namespace
