/**
 * \file    KeyM_Finalize_test.cpp
 * \brief   KeyM_Finalize() の単体テスト
 * \details セッション中に更新した鍵を Csm_KeySetValid() で有効化し、セッションを閉じる。セッション未開始は E_NOT_OK。
 *          有効化に失敗した鍵があれば、残りを続けたうえで E_NOT_OK を返す。いずれもセッションは閉じる。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "KeyM.h"
#include "KeyM_Cfg.h"
#include "Csm.h"
#include "CryIf.h"
#include "Crypto.h"
#include "Crypto_Cfg.h"
#include "Crypto_Aes128.h"
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

/* 更新用の新しい鍵（16 バイト、初期鍵とは異なる値）。 */
const uint8 g_newKey[CRYPTO_AES128_KEY_SIZE] = {
    0xA0U, 0xA1U, 0xA2U, 0xA3U, 0xA4U, 0xA5U, 0xA6U, 0xA7U,
    0xA8U, 0xA9U, 0xAAU, 0xABU, 0xACU, 0xADU, 0xAEU, 0xAFU
};
const uint8 g_keyName = static_cast<uint8>(KEYM_CRYPTO_KEY_NAME_IMMOBILIZER_CMD);


/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* KeyM_Finalize() の単体テスト用フィクスチャ。
 * SetUp(): Crypto・CryIf・Csm を未初期化へ戻してから初期化し（実体をリンクしており、鍵の書き込みが実際に反映される）、KeyM_Init(NULL) を呼ぶ。DET の記録は初期化後に消す。
 * TearDown(): KeyM_Deinit() で鍵を消去してから、Crypto・CryIf・Csm を未初期化へ戻す（他のテストへ状態を持ち越さない）。 */
class KeyM_Finalize_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Crypto_Test_ResetInitState();
        CryIf_Test_ResetInitState();
        Csm_Test_ResetInitState();
        Crypto_Init();
        CryIf_Init();
        Csm_Init();
        KeyM_Init(NULL);
        FakeDetHw_Reset();
        FakeDetHw_LogSuppressed = 0U;
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        KeyM_Deinit();
        Crypto_Test_ResetInitState();
        CryIf_Test_ResetInitState();
        Csm_Test_ResetInitState();
    }

    /* 鍵 IMMOBILIZER_CMD の現在値を Csm 経由で読む。 */
    Std_ReturnType ReadKey(uint8* buf)
    {
        uint32 len = CRYPTO_AES128_KEY_SIZE;
        return Csm_KeyElementGet(CRYPTO_KEY_IMMOBILIZER_CMD, CRYPTO_KEY_ELEMENT_ID_CIPHER_KEY, buf, &len);
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(KeyM_Finalize_Test, KeyM_Finalize_OK_ValidatesUpdatedKeyAndClosesSession)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    KeyM_Start(KEYM_START_WORKSHOPMODE, NULL, 0U, NULL, NULL);
    ASSERT_EQ(KeyM_Update(&g_keyName, 1U, g_newKey, CRYPTO_AES128_KEY_SIZE, NULL, 0U), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = KeyM_Finalize(NULL, 0U, NULL, 0U);

    /* セッションが閉じたので、続けての更新は受け付けない。 */
    const Std_ReturnType retAfter = KeyM_Update(&g_keyName, 1U, g_newKey, CRYPTO_AES128_KEY_SIZE, NULL, 0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(retAfter, E_NOT_OK);
}

TEST_F(KeyM_Finalize_Test, KeyM_Finalize_NG_WithoutSessionIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（KeyM_Start() を呼ばない）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = KeyM_Finalize(NULL, 0U, NULL, 0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
}

TEST_F(KeyM_Finalize_Test, KeyM_Finalize_NG_ReportsFailureWhenKeySetValidFailsButClosesSession)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    KeyM_Start(KEYM_START_WORKSHOPMODE, NULL, 0U, NULL, NULL);
    ASSERT_EQ(KeyM_Update(&g_keyName, 1U, g_newKey, CRYPTO_AES128_KEY_SIZE, NULL, 0U), E_OK);
    FakeDetHw_LogSuppressed = 1U;
    Csm_Test_ResetInitState();   // Csm_KeySetValid() を失敗させる

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = KeyM_Finalize(NULL, 0U, NULL, 0U);
    const Std_ReturnType retAgain = KeyM_Finalize(NULL, 0U, NULL, 0U);   // セッションは閉じている

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(retAgain, E_NOT_OK);
}

}  // namespace
