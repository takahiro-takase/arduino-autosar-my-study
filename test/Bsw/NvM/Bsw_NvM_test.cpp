/**
 * \file    Bsw_NvM_test.cpp
 * \brief   NvM.c（src/Bsw/NvM/NvM.c）の単体テスト（NG系）
 * \details NvM.c 全公開APIの `Det_ReportError()` 呼び出し箇所（NG ケースのみ）を
 *          1ファイルにまとめる。各ケースで、報告される ErrorId が仕様どおり
 *          正しい値になっていることを `Fake_Det_Hw.h`（`FakeDetHw_LastErrorId`）
 *          で検証する。
 *
 *          `NvM_ReadBlock()`/`NvM_WriteBlock()`/`NvM_RestoreBlockDefaults()` の
 *          `NVM_E_PARAM_ADDRESS`（`blk->RamBlockDataAddress == NULL` かつ
 *          出力/入力ポインタも NULL の場合のみ到達）は、本プロジェクトの実際の
 *          `NvM_Config`（NvM_PBCfg.c）が構成する全ブロックが恒久 RAM ブロック
 *          を持つため到達不能（NvM_Init() が固定の `NvM_Config` を直接参照し、
 *          テストから差し替え不能なため、NvM.c 側のコメントが示すとおり
 *          「本プロジェクトには存在しないが念のため」の防御分岐）。対象外とする。
 *
 *          NvM には `NvM_DeInit()` に相当する API が無く、`NvM_Cfg` は
 *          native_chain_tests バイナリ全体で共有される static のため、
 *          `NVM_E_NOT_INITIALIZED` の検証には `NvM_Test_ResetInitState()`
 *          （`NVM_UNIT_TEST` ビルドのみに存在するテスト専用関数、
 *          `Mcu_Test_ResetInitState()` と同じ設計）を使う。
 *
 *          2026-09、NvM/Fee/MemIf 自身の Det 検証のため、`NvM.c`/`MemIf.c`/
 *          `Fee.c` を実体リンクへ切り替えた（旧 `Fake_NvM.c` は撤去）。
 *          `stub/Hal/Fake_Fee_Hw.c` が RAM バッファで EEPROM を模擬し、
 *          `test/test_main.cpp` のカスタムリスナーが毎テスト開始前に
 *          自動でバッファを消去済み状態(0xFF)へ戻すため、`MemIf_Init()`→
 *          `NvM_Init()` を呼べば毎回決定的に「初回起動」から始まる
 *          （Fake_Fee_Hw.h 冒頭コメント参照）。
 *
 *          GoogleTest の main() は test_main.cpp に集約しているため、
 *          本ファイルでは定義しない。
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
class Bsw_NvM_Test : public ::testing::Test
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

// ------------------------------------------------------------
// NvM_ReadBlock()
// ------------------------------------------------------------

TEST_F(Bsw_NvM_Test, NvM_ReadBlock_NG_NotInitialized)
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

TEST_F(Bsw_NvM_Test, NvM_ReadBlock_NG_InvalidBlockId)
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

// ------------------------------------------------------------
// NvM_WriteBlock()
// ------------------------------------------------------------

TEST_F(Bsw_NvM_Test, NvM_WriteBlock_NG_NotInitialized)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    NvM_Test_ResetInitState();
    uint8 buf[16] = { 0 };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = NvM_WriteBlock(NVM_BLOCK_ID_DEM_MAGIC, buf);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, NVM_E_NOT_INITIALIZED);
}

TEST_F(Bsw_NvM_Test, NvM_WriteBlock_NG_InvalidBlockId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 buf[16] = { 0 };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = NvM_WriteBlock(kInvalidBlockId, buf);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, NVM_E_PARAM_BLOCK_ID);
}

// ------------------------------------------------------------
// NvM_RestoreBlockDefaults()
// ------------------------------------------------------------

TEST_F(Bsw_NvM_Test, NvM_RestoreBlockDefaults_NG_NotInitialized)
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

TEST_F(Bsw_NvM_Test, NvM_RestoreBlockDefaults_NG_InvalidBlockId)
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

TEST_F(Bsw_NvM_Test, NvM_RestoreBlockDefaults_NG_BlockWithoutDefaults)
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

// ------------------------------------------------------------
// NvM_SetBlockProtection()
// ------------------------------------------------------------

TEST_F(Bsw_NvM_Test, NvM_SetBlockProtection_NG_NotInitialized)
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

TEST_F(Bsw_NvM_Test, NvM_SetBlockProtection_NG_InvalidBlockId)
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

TEST_F(Bsw_NvM_Test, NvM_SetBlockProtection_NG_BlockPending)
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

// ------------------------------------------------------------
// NvM_GetErrorStatus()
// ------------------------------------------------------------

TEST_F(Bsw_NvM_Test, NvM_GetErrorStatus_NG_ParamData)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = NvM_GetErrorStatus(NVM_BLOCK_ID_DEM_MAGIC, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, NVM_E_PARAM_DATA);
}

TEST_F(Bsw_NvM_Test, NvM_GetErrorStatus_NG_NotInitialized)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    NvM_Test_ResetInitState();
    NvM_RequestResultType result;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = NvM_GetErrorStatus(NVM_BLOCK_ID_DEM_MAGIC, &result);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, NVM_E_NOT_INITIALIZED);
}

TEST_F(Bsw_NvM_Test, NvM_GetErrorStatus_NG_InvalidBlockId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    NvM_RequestResultType result;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = NvM_GetErrorStatus(kInvalidBlockId, &result);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, NVM_E_PARAM_BLOCK_ID);
}

// ------------------------------------------------------------
// NvM_MainFunction()
// ------------------------------------------------------------

TEST_F(Bsw_NvM_Test, NvM_MainFunction_NG_NotInitialized)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    NvM_Test_ResetInitState();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    NvM_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, NVM_E_NOT_INITIALIZED);
}

// ------------------------------------------------------------
// NvM_GetVersionInfo()（他BSWモジュール共通の慣例により未初期化チェック対象外）
// ------------------------------------------------------------

TEST_F(Bsw_NvM_Test, NvM_GetVersionInfo_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    NvM_GetVersionInfo(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, NVM_E_PARAM_POINTER);
}

}  // namespace
