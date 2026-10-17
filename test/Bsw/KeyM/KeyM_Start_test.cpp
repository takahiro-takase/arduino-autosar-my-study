/**
 * \file    KeyM_Start_test.cpp
 * \brief   KeyM_Start() の単体テスト
 * \details 鍵更新セッションを開く。既にセッションが開いていても E_OK を返し継続する（[SWS_KeyM_00086]）。
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

/* KeyM_Start() の単体テスト用フィクスチャ。
 * SetUp(): Crypto・CryIf・Csm を未初期化へ戻してから初期化し（実体をリンクしており、鍵の書き込みが実際に反映される）、KeyM_Init(NULL) を呼ぶ。DET の記録は初期化後に消す。
 * TearDown(): KeyM_Deinit() で鍵を消去してから、Crypto・CryIf・Csm を未初期化へ戻す（他のテストへ状態を持ち越さない）。 */
class KeyM_Start_Test : public ::testing::Test
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
class KeyM_Start_Base_Test : public ::testing::Test
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

TEST_F(KeyM_Start_Base_Test, KeyM_Start_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    KeyM_Deinit();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = KeyM_Start(KEYM_START_OEM_PRODUCTIONMODE, NULL, 0U, NULL, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, KEYM_E_UNINIT);
}

TEST_F(KeyM_Start_Test, KeyM_Start_OK_OpensSessionAndAcceptsRestart)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 before[CRYPTO_AES128_KEY_SIZE];
    ASSERT_EQ(ReadKey(before), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType retFirst = KeyM_Start(KEYM_START_WORKSHOPMODE, NULL, 0U, NULL, NULL);
    const Std_ReturnType retSecond = KeyM_Start(KEYM_START_OEM_PRODUCTIONMODE, NULL, 0U, NULL, NULL);

    /* セッションが開いたので鍵の更新を受け付ける。 */
    const Std_ReturnType retUpdate = KeyM_Update(&g_keyName, 1U, g_newKey, CRYPTO_AES128_KEY_SIZE, NULL, 0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(retFirst, E_OK);
    EXPECT_EQ(retSecond, E_OK);
    EXPECT_EQ(retUpdate, E_OK);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

}  // namespace
