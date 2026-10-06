/**
 * \file    Bsw_CryIf_test.cpp
 * \brief   CryIf.c（src/Bsw/CryIf/CryIf.c）の単体テスト（NG系）
 * \details CryIf.c 全公開APIの `Det_ReportError()` 呼び出し箇所（NG ケースの
 *          み）を1ファイルにまとめる。各ケースで、報告される ErrorId が
 *          仕様どおり正しい値になっていることを `Fake_Det_Hw.h`
 *          （`FakeDetHw_LastErrorId`）で検証する。
 *
 *          CryIf には `CryIf_DeInit()` に相当する API が無く、
 *          `CryIf_Initialized` は native_chain_tests バイナリ全体で共有される
 *          static のため、`CRYIF_E_UNINIT` の検証には
 *          `CryIf_Test_ResetInitState()`（`CRYIF_UNIT_TEST` ビルドのみに
 *          存在するテスト専用関数、`Mcu_Test_ResetInitState()` と同じ設計）を
 *          使う。
 *
 *          `CryIf_Init()` 自体は失敗条件を持たないため（Crypto.c と異なり
 *          自己診断等を行わず常に成功する）、`CRYIF_E_INIT_FAILED` は
 *          ソース上に定義のみで実際には報告されない（対象外）。
 *
 *          GoogleTest の main() は test_main.cpp に集約しているため、
 *          本ファイルでは定義しない。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "CryIf.h"
#include "Crypto_Cfg.h"
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

/* CryIf の単体テスト用フィクスチャ。
 * SetUp(): CryIf_Init() を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): 未初期化の状態へ戻す。 */
class Bsw_CryIf_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        CryIf_Init();
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        CryIf_Test_ResetInitState();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

// ------------------------------------------------------------
// CryIf_GetVersionInfo()
// ------------------------------------------------------------

TEST_F(Bsw_CryIf_Test, CryIf_GetVersionInfo_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CryIf_Test_ResetInitState();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CryIf_GetVersionInfo(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, CRYIF_E_UNINIT);
}

TEST_F(Bsw_CryIf_Test, CryIf_GetVersionInfo_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CryIf_GetVersionInfo(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, CRYIF_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// CryIf_ProcessJob()
// ------------------------------------------------------------

TEST_F(Bsw_CryIf_Test, CryIf_ProcessJob_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CryIf_Test_ResetInitState();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CryIf_ProcessJob(CRYIF_CHANNEL_ID, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CRYIF_E_UNINIT);
}

TEST_F(Bsw_CryIf_Test, CryIf_ProcessJob_NG_InvalidChannelId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CryIf_ProcessJob(CRYIF_CHANNEL_ID + 1U, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CRYIF_E_PARAM_HANDLE);
}

TEST_F(Bsw_CryIf_Test, CryIf_ProcessJob_NG_NullJobPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CryIf_ProcessJob(CRYIF_CHANNEL_ID, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CRYIF_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// CryIf_KeyElementSet()
// ------------------------------------------------------------

TEST_F(Bsw_CryIf_Test, CryIf_KeyElementSet_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CryIf_Test_ResetInitState();
    uint8 key[16] = { 0 };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CryIf_KeyElementSet(0U, 1U, key, sizeof(key));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CRYIF_E_UNINIT);
}

TEST_F(Bsw_CryIf_Test, CryIf_KeyElementSet_NG_NullKeyPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CryIf_KeyElementSet(0U, 1U, NULL, 16U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CRYIF_E_PARAM_POINTER);
}

TEST_F(Bsw_CryIf_Test, CryIf_KeyElementSet_NG_ZeroKeyLength)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 key[16] = { 0 };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CryIf_KeyElementSet(0U, 1U, key, 0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CRYIF_E_PARAM_VALUE);
}

TEST_F(Bsw_CryIf_Test, CryIf_KeyElementSet_NG_InvalidCryIfKeyId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 key[16] = { 0 };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CryIf_KeyElementSet(CRYPTO_KEY_COUNT, 1U, key, sizeof(key));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CRYIF_E_PARAM_HANDLE);
}

// ------------------------------------------------------------
// CryIf_KeySetValid()
// ------------------------------------------------------------

TEST_F(Bsw_CryIf_Test, CryIf_KeySetValid_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CryIf_Test_ResetInitState();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CryIf_KeySetValid(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CRYIF_E_UNINIT);
}

TEST_F(Bsw_CryIf_Test, CryIf_KeySetValid_NG_InvalidCryIfKeyId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CryIf_KeySetValid(CRYPTO_KEY_COUNT);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CRYIF_E_PARAM_HANDLE);
}

// ------------------------------------------------------------
// CryIf_KeyElementGet()
// ------------------------------------------------------------

TEST_F(Bsw_CryIf_Test, CryIf_KeyElementGet_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CryIf_Test_ResetInitState();
    uint8  buf[16];
    uint32 len = sizeof(buf);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CryIf_KeyElementGet(0U, 1U, buf, &len);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CRYIF_E_UNINIT);
}

TEST_F(Bsw_CryIf_Test, CryIf_KeyElementGet_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint32 len = 16U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CryIf_KeyElementGet(0U, 1U, NULL, &len);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CRYIF_E_PARAM_POINTER);
}

TEST_F(Bsw_CryIf_Test, CryIf_KeyElementGet_NG_ZeroLength)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8  buf[16];
    uint32 len = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CryIf_KeyElementGet(0U, 1U, buf, &len);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CRYIF_E_PARAM_VALUE);
}

TEST_F(Bsw_CryIf_Test, CryIf_KeyElementGet_NG_InvalidCryIfKeyId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8  buf[16];
    uint32 len = sizeof(buf);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CryIf_KeyElementGet(CRYPTO_KEY_COUNT, 1U, buf, &len);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CRYIF_E_PARAM_HANDLE);
}

}  // namespace
