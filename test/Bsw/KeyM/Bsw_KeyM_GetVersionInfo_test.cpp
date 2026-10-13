/**
 * \file    Bsw_KeyM_GetVersionInfo_test.cpp
 * \brief   KeyM_GetVersionInfo() の単体テスト
 * \details バージョン情報を、KeyM_Cfg.h の定数どおりに書き込む。
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

/* KeyM_GetVersionInfo() の単体テスト用フィクスチャ。
 * SetUp(): Crypto・CryIf・Csm を未初期化へ戻してから初期化し（実体をリンクしており、鍵の書き込みが実際に反映される）、KeyM_Init(NULL) を呼ぶ。DET の記録は初期化後に消す。
 * TearDown(): KeyM_Deinit() で鍵を消去してから、Crypto・CryIf・Csm を未初期化へ戻す（他のテストへ状態を持ち越さない）。 */
class Bsw_KeyM_GetVersionInfo_Test : public ::testing::Test
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

TEST_F(Bsw_KeyM_GetVersionInfo_Test, KeyM_GetVersionInfo_OK_ReturnsConfiguredVersion)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Std_VersionInfoType info = { 0xFFFFU, 0xFFFFU, 0xFFU, 0xFFU, 0xFFU };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    KeyM_GetVersionInfo(&info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(info.vendorID, KEYM_VENDOR_ID);
    EXPECT_EQ(info.moduleID, KEYM_MODULE_ID);
    EXPECT_EQ(info.sw_major_version, KEYM_SW_MAJOR_VERSION);
    EXPECT_EQ(info.sw_minor_version, KEYM_SW_MINOR_VERSION);
    EXPECT_EQ(info.sw_patch_version, KEYM_SW_PATCH_VERSION);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

}  // namespace
