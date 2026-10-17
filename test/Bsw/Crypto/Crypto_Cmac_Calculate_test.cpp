/**
 * \file    Crypto_Cmac_Calculate_test.cpp
 * \brief   Crypto_Cmac_Calculate() の単体テスト
 * \details AES-128 の CMAC を、RFC 4493 の Section 4 にある 4 つの例（メッセージ長 0・16・40・64 バイト）の値で確かめる。
 *          長さが 16 の倍数かどうか、空かどうか、複数のブロックにまたがるかどうかの、全ての分岐を通る。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Crypto_Cmac.h"
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

// RFC 4493 の Section 4 の例（AES-128 の CMAC）。
const uint8 kKey[16] = { 0x2BU, 0x7EU, 0x15U, 0x16U, 0x28U, 0xAEU, 0xD2U, 0xA6U, 0xABU, 0xF7U, 0x15U, 0x88U, 0x09U, 0xCFU, 0x4FU, 0x3CU };
const uint8 kMessage[64] = {
    0x6BU, 0xC1U, 0xBEU, 0xE2U, 0x2EU, 0x40U, 0x9FU, 0x96U, 0xE9U, 0x3DU, 0x7EU, 0x11U, 0x73U, 0x93U, 0x17U, 0x2AU,
    0xAEU, 0x2DU, 0x8AU, 0x57U, 0x1EU, 0x03U, 0xACU, 0x9CU, 0x9EU, 0xB7U, 0x6FU, 0xACU, 0x45U, 0xAFU, 0x8EU, 0x51U,
    0x30U, 0xC8U, 0x1CU, 0x46U, 0xA3U, 0x5CU, 0xE4U, 0x11U, 0xE5U, 0xFBU, 0xC1U, 0x19U, 0x1AU, 0x0AU, 0x52U, 0xEFU,
    0xF6U, 0x9FU, 0x24U, 0x45U, 0xDFU, 0x4FU, 0x9BU, 0x17U, 0xADU, 0x2BU, 0x41U, 0x7BU, 0xE6U, 0x6CU, 0x37U, 0x10U
};

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* Crypto_Cmac_Calculate() の単体テスト用フィクスチャ。
 * SetUp()/TearDown(): なし（Crypto_Cmac_Calculate() は状態を持たない）。 */
class Crypto_Cmac_Calculate_Test : public ::testing::Test
{
protected:
    // 準備と後始末は不要（Crypto_Cmac_Calculate() は状態を持たない）。
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Crypto_Cmac_Calculate_Test, Crypto_Cmac_Calculate_OK_EmptyMessageMatchesRfc4493)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    const uint8 expected[16] = { 0xBBU, 0x1DU, 0x69U, 0x29U, 0xE9U, 0x59U, 0x37U, 0x28U, 0x7FU, 0xA3U, 0x7DU, 0x12U, 0x9BU, 0x75U, 0x67U, 0x46U };
    uint8 mac[16] = { 0U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Crypto_Cmac_Calculate(kKey, kMessage, 0U, mac);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    for (uint8 i = 0U; i < 16U; i++)
    {
        EXPECT_EQ(mac[i], expected[i]) << "byte " << static_cast<int>(i);
    }
}

TEST_F(Crypto_Cmac_Calculate_Test, Crypto_Cmac_Calculate_OK_SingleCompleteBlockMatchesRfc4493)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    const uint8 expected[16] = { 0x07U, 0x0AU, 0x16U, 0xB4U, 0x6BU, 0x4DU, 0x41U, 0x44U, 0xF7U, 0x9BU, 0xDDU, 0x9DU, 0xD0U, 0x4AU, 0x28U, 0x7CU };
    uint8 mac[16] = { 0U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Crypto_Cmac_Calculate(kKey, kMessage, 16U, mac);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    for (uint8 i = 0U; i < 16U; i++)
    {
        EXPECT_EQ(mac[i], expected[i]) << "byte " << static_cast<int>(i);
    }
}

TEST_F(Crypto_Cmac_Calculate_Test, Crypto_Cmac_Calculate_OK_IncompleteLastBlockMatchesRfc4493)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    const uint8 expected[16] = { 0xDFU, 0xA6U, 0x67U, 0x47U, 0xDEU, 0x9AU, 0xE6U, 0x30U, 0x30U, 0xCAU, 0x32U, 0x61U, 0x14U, 0x97U, 0xC8U, 0x27U };
    uint8 mac[16] = { 0U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Crypto_Cmac_Calculate(kKey, kMessage, 40U, mac);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    for (uint8 i = 0U; i < 16U; i++)
    {
        EXPECT_EQ(mac[i], expected[i]) << "byte " << static_cast<int>(i);
    }
}

TEST_F(Crypto_Cmac_Calculate_Test, Crypto_Cmac_Calculate_OK_FourCompleteBlocksMatchRfc4493)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    const uint8 expected[16] = { 0x51U, 0xF0U, 0xBEU, 0xBFU, 0x7EU, 0x3BU, 0x9DU, 0x92U, 0xFCU, 0x49U, 0x74U, 0x17U, 0x79U, 0x36U, 0x3CU, 0xFEU };
    uint8 mac[16] = { 0U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Crypto_Cmac_Calculate(kKey, kMessage, 64U, mac);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    for (uint8 i = 0U; i < 16U; i++)
    {
        EXPECT_EQ(mac[i], expected[i]) << "byte " << static_cast<int>(i);
    }
}

}  // namespace
