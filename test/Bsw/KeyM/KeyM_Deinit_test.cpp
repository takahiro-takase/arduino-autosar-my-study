/**
 * \file    KeyM_Deinit_test.cpp
 * \brief   KeyM_Deinit() の単体テスト
 * \details Deinit は RAM 上の鍵材料を全ゼロで上書きして破棄し（[SWS_KeyM_00048]）、セッションを閉じて未初期化へ戻る。
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

/* KeyM_Deinit() の単体テスト用フィクスチャ。
 * SetUp(): Crypto・CryIf・Csm を未初期化へ戻してから初期化し（実体をリンクしており、鍵の書き込みが実際に反映される）、KeyM_Init(NULL) を呼ぶ。DET の記録は初期化後に消す。
 * TearDown(): KeyM_Deinit() で鍵を消去してから、Crypto・CryIf・Csm を未初期化へ戻す（他のテストへ状態を持ち越さない）。 */
class KeyM_Deinit_Test : public ::testing::Test
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

/* KeyM の単体テスト用フィクスチャ。
 * SetUp(): KeyM_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): KeyM_Deinit() で未初期化へ戻す。 */
class KeyM_Deinit_Base_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        KeyM_Init(NULL);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        KeyM_Deinit();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(KeyM_Deinit_Base_Test, KeyM_Deinit_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    KeyM_Deinit();  // 1回目: SetUp() の Init を正常に解除する
    FakeDetHw_Reset();

    KeyM_Deinit();  // 2回目: 既に未初期化のため NG

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, KEYM_E_UNINIT);
}

TEST_F(KeyM_Deinit_Test, KeyM_Deinit_OK_ErasesKeyMaterialAndClosesSession)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    KeyM_Start(KEYM_START_WORKSHOPMODE, NULL, 0U, NULL, NULL);
    ASSERT_EQ(KeyM_Update(&g_keyName, 1U, g_newKey, CRYPTO_AES128_KEY_SIZE, NULL, 0U), E_OK);
    ASSERT_EQ(KeyM_Finalize(NULL, 0U, NULL, 0U), E_OK);
    uint8 key[CRYPTO_AES128_KEY_SIZE];
    memset(key, 0xFF, sizeof(key));

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    KeyM_Deinit();
    const Std_ReturnType retRead = ReadKey(key);
    FakeDetHw_Reset();
    const Std_ReturnType retStart = KeyM_Start(KEYM_START_WORKSHOPMODE, NULL, 0U, NULL, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_EQ(retRead, E_OK);
    for (uint32 i = 0U; i < CRYPTO_AES128_KEY_SIZE; i++)
    {
        EXPECT_EQ(key[i], 0U);   // 鍵材料は全ゼロ
    }
    EXPECT_EQ(retStart, E_NOT_OK);   // 未初期化へ戻っている
    EXPECT_EQ(FakeDetHw_LastErrorId, KEYM_E_UNINIT);
}

}  // namespace
