/**
 * \file    Csm_KeyElementGet_test.cpp
 * \brief   Csm_KeyElementGet() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 Csm_test.cpp）。
 *
 *          分割前の共通の説明は、旧 Csm_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Csm.h"
#include "CryIf.h"
#include "Crypto.h"
#include "Crypto_Cfg.h"
#include "Crypto_Cmac.h"
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

/* Csm の単体テスト用フィクスチャ。
 * SetUp(): Csm_Init() を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): Csm・CryIf・Crypto をそれぞれ未初期化の状態へ戻す。 */
class Csm_KeyElementGet_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        Csm_Init();
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Csm_Test_ResetInitState();
        CryIf_Test_ResetInitState();
        Crypto_Test_ResetInitState();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Csm_KeyElementGet_Test, Csm_KeyElementGet_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Csm_Test_ResetInitState();
    uint8  buf[16];
    uint32 len = sizeof(buf);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Csm_KeyElementGet(0U, 1U, buf, &len);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CSM_E_UNINIT);
}

TEST_F(Csm_KeyElementGet_Test, Csm_KeyElementGet_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint32 len = 16U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Csm_KeyElementGet(0U, 1U, NULL, &len);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CSM_E_PARAM_POINTER);
}

TEST_F(Csm_KeyElementGet_Test, Csm_KeyElementGet_NG_InvalidKeyId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8  buf[16];
    uint32 len = sizeof(buf);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Csm_KeyElementGet(CRYPTO_KEY_COUNT, 1U, buf, &len);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CSM_E_PARAM_HANDLE);
}

TEST_F(Csm_KeyElementGet_Test, Csm_KeyElementGet_NG_ServiceNotStarted)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8  buf[16];
    uint32 len = sizeof(buf);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Csm_KeyElementGet(0U, 1U, buf, &len);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CSM_E_SERVICE_NOT_STARTED);
}

TEST_F(Csm_KeyElementGet_Test, Csm_KeyElementGet_NG_SmallBuffer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // 下位層まで実チェーンを通す必要があるため、ここでのみ Crypto/CryIf も
    // 初期化する（他のテストは CryIf 未初期化のまま SERVICE_NOT_STARTED で
    // 早期returnするため不要）。
    FakeDetHw_LogSuppressed = 1U;
    Crypto_Init();
    CryIf_Init();
    FakeDetHw_LogSuppressed = 0U;

    uint8  buf[16];
    uint32 len = CRYPTO_AES128_KEY_SIZE - 1U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Csm_KeyElementGet(0U, 1U, buf, &len);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, CRYPTO_E_SMALL_BUFFER);
    EXPECT_EQ(FakeDetHw_LastErrorId, CSM_E_SMALL_BUFFER);
}

TEST_F(Csm_KeyElementGet_Test, Csm_KeyElementGet_NG_NullKeyLengthPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 buf[16] = { 0U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Csm_KeyElementGet(0U, 1U, buf, NULL);   // keyPtr は有効、keyLengthPtr だけ NULL

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CSM_E_PARAM_POINTER);
}

}  // namespace
