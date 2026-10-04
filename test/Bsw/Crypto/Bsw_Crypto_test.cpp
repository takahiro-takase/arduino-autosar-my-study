/**
 * \file    Bsw_Crypto_test.cpp
 * \brief   Crypto.c（src/Bsw/Crypto/Crypto.c）の単体テスト（NG系）
 * \details Crypto.c 全公開APIの `Det_ReportError()` 呼び出し箇所（NG ケースの
 *          み）を1ファイルにまとめる。各ケースで、報告される ErrorId が
 *          仕様どおり正しい値になっていることを `Fake_Det_Hw.h`
 *          （`FakeDetHw_LastErrorId`）で検証する。
 *
 *          `Crypto_Init()` の `CRYPTO_E_INIT_FAILED`（AES-128 自己診断失敗）は
 *          FIPS-197 の既知テストベクタと実装の比較のみで判定され、本プロジェクトに
 *          意図的に失敗させる注入経路が無いため到達不能（正しい実装である限り
 *          常に成功する）。対象外とする。
 *
 *          Crypto には `Crypto_DeInit()` に相当する API が無く、
 *          `Crypto_Initialized` は native_chain_tests バイナリ全体で共有される
 *          static のため、`CRYPTO_E_UNINIT` の検証には
 *          `Crypto_Test_ResetInitState()`（`CRYPTO_UNIT_TEST` ビルドのみに
 *          存在するテスト専用関数、`Mcu_Test_ResetInitState()` と同じ設計）を
 *          使う。
 *
 *          GoogleTest の main() は test_main.cpp に集約しているため、
 *          本ファイルでは定義しない。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Crypto.h"
#include "Crypto_Aes128.h"
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

class Bsw_Crypto_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        Crypto_Init();
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Crypto_Test_ResetInitState();
    }

    Crypto_JobType MakeMacGenerateJob(uint8* macBuf)
    {
        Crypto_JobType job = {};
        job.service         = CRYPTO_MACGENERATE;
        job.cryptoKeyId     = 0U;
        job.inputPtr        = kInput;
        job.inputLength     = sizeof(kInput);
        job.macPtr          = macBuf;
        job.macLength       = CRYPTO_CMAC_SIZE;
        job.verifyResultPtr = NULL;
        return job;
    }

    static const uint8 kInput[4];
};

const uint8 Bsw_Crypto_Test::kInput[4] = { 0x01U, 0x02U, 0x03U, 0x04U };

// ------------------------------------------------------------
// Crypto_GetVersionInfo()
// ------------------------------------------------------------

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Bsw_Crypto_Test, Crypto_GetVersionInfo_NG_NullPointer)
{
    Crypto_GetVersionInfo(NULL);

    EXPECT_EQ(FakeDetHw_LastErrorId, CRYPTO_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// Crypto_ProcessJob()
// ------------------------------------------------------------

TEST_F(Bsw_Crypto_Test, Crypto_ProcessJob_NG_Uninit)
{
    Crypto_Test_ResetInitState();
    uint8 mac[CRYPTO_CMAC_SIZE];
    Crypto_JobType job = MakeMacGenerateJob(mac);

    Std_ReturnType ret = Crypto_ProcessJob(CRYPTO_OBJECT_ID, &job);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CRYPTO_E_UNINIT);
}

TEST_F(Bsw_Crypto_Test, Crypto_ProcessJob_NG_InvalidObjectId)
{
    uint8 mac[CRYPTO_CMAC_SIZE];
    Crypto_JobType job = MakeMacGenerateJob(mac);

    Std_ReturnType ret = Crypto_ProcessJob(CRYPTO_OBJECT_ID + 1U, &job);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CRYPTO_E_PARAM_HANDLE);
}

TEST_F(Bsw_Crypto_Test, Crypto_ProcessJob_NG_NullJobPtr)
{
    Std_ReturnType ret = Crypto_ProcessJob(CRYPTO_OBJECT_ID, NULL);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CRYPTO_E_PARAM_POINTER);
}

TEST_F(Bsw_Crypto_Test, Crypto_ProcessJob_NG_InvalidCryptoKeyId)
{
    uint8 mac[CRYPTO_CMAC_SIZE];
    Crypto_JobType job = MakeMacGenerateJob(mac);
    job.cryptoKeyId = CRYPTO_KEY_COUNT;

    Std_ReturnType ret = Crypto_ProcessJob(CRYPTO_OBJECT_ID, &job);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CRYPTO_E_PARAM_HANDLE);
}

TEST_F(Bsw_Crypto_Test, Crypto_ProcessJob_NG_NullVerifyResultPtr)
{
    uint8 mac[CRYPTO_CMAC_SIZE] = { 0 };
    Crypto_JobType job = MakeMacGenerateJob(mac);
    job.service         = CRYPTO_MACVERIFY;
    job.verifyResultPtr = NULL;

    Std_ReturnType ret = Crypto_ProcessJob(CRYPTO_OBJECT_ID, &job);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CRYPTO_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// Crypto_KeyElementSet()
// ------------------------------------------------------------

TEST_F(Bsw_Crypto_Test, Crypto_KeyElementSet_NG_Uninit)
{
    Crypto_Test_ResetInitState();
    uint8 key[CRYPTO_AES128_KEY_SIZE] = { 0 };

    Std_ReturnType ret = Crypto_KeyElementSet(0U, CRYPTO_KEY_ELEMENT_ID_CIPHER_KEY, key, sizeof(key));

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CRYPTO_E_UNINIT);
}

TEST_F(Bsw_Crypto_Test, Crypto_KeyElementSet_NG_InvalidHandle)
{
    uint8 key[CRYPTO_AES128_KEY_SIZE] = { 0 };

    Std_ReturnType ret = Crypto_KeyElementSet(CRYPTO_KEY_COUNT, CRYPTO_KEY_ELEMENT_ID_CIPHER_KEY, key, sizeof(key));

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CRYPTO_E_PARAM_HANDLE);
}

TEST_F(Bsw_Crypto_Test, Crypto_KeyElementSet_NG_NullKeyPtr)
{
    Std_ReturnType ret = Crypto_KeyElementSet(0U, CRYPTO_KEY_ELEMENT_ID_CIPHER_KEY, NULL, CRYPTO_AES128_KEY_SIZE);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CRYPTO_E_PARAM_POINTER);
}

TEST_F(Bsw_Crypto_Test, Crypto_KeyElementSet_NG_KeySizeMismatch)
{
    uint8 key[CRYPTO_AES128_KEY_SIZE] = { 0 };

    Std_ReturnType ret = Crypto_KeyElementSet(0U, CRYPTO_KEY_ELEMENT_ID_CIPHER_KEY, key, CRYPTO_AES128_KEY_SIZE - 1U);

    EXPECT_EQ(ret, CRYPTO_E_KEY_SIZE_MISMATCH);
    EXPECT_EQ(FakeDetHw_LastErrorId, CRYPTO_E_PARAM_VALUE);
}

// ------------------------------------------------------------
// Crypto_KeyElementGet()
// ------------------------------------------------------------

TEST_F(Bsw_Crypto_Test, Crypto_KeyElementGet_NG_Uninit)
{
    Crypto_Test_ResetInitState();
    uint8  buf[CRYPTO_AES128_KEY_SIZE];
    uint32 len = sizeof(buf);

    Std_ReturnType ret = Crypto_KeyElementGet(0U, CRYPTO_KEY_ELEMENT_ID_CIPHER_KEY, buf, &len);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CRYPTO_E_UNINIT);
}

TEST_F(Bsw_Crypto_Test, Crypto_KeyElementGet_NG_InvalidHandle)
{
    uint8  buf[CRYPTO_AES128_KEY_SIZE];
    uint32 len = sizeof(buf);

    Std_ReturnType ret = Crypto_KeyElementGet(CRYPTO_KEY_COUNT, CRYPTO_KEY_ELEMENT_ID_CIPHER_KEY, buf, &len);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CRYPTO_E_PARAM_HANDLE);
}

TEST_F(Bsw_Crypto_Test, Crypto_KeyElementGet_NG_NullPointer)
{
    uint32 len = CRYPTO_AES128_KEY_SIZE;

    Std_ReturnType ret = Crypto_KeyElementGet(0U, CRYPTO_KEY_ELEMENT_ID_CIPHER_KEY, NULL, &len);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CRYPTO_E_PARAM_POINTER);
}

TEST_F(Bsw_Crypto_Test, Crypto_KeyElementGet_NG_SmallBuffer)
{
    uint8  buf[CRYPTO_AES128_KEY_SIZE];
    uint32 len = CRYPTO_AES128_KEY_SIZE - 1U;

    Std_ReturnType ret = Crypto_KeyElementGet(0U, CRYPTO_KEY_ELEMENT_ID_CIPHER_KEY, buf, &len);

    EXPECT_EQ(ret, CRYPTO_E_SMALL_BUFFER);
    EXPECT_EQ(FakeDetHw_LastErrorId, CRYPTO_E_PARAM_VALUE);
}

TEST_F(Bsw_Crypto_Test, Crypto_KeyElementGet_NG_BufferTooLarge)
{
    uint8  buf[CRYPTO_AES128_KEY_SIZE + 1U];
    uint32 len = sizeof(buf);

    Std_ReturnType ret = Crypto_KeyElementGet(0U, CRYPTO_KEY_ELEMENT_ID_CIPHER_KEY, buf, &len);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CRYPTO_E_PARAM_VALUE);
}

// ------------------------------------------------------------
// Crypto_KeySetValid()
// ------------------------------------------------------------

TEST_F(Bsw_Crypto_Test, Crypto_KeySetValid_NG_Uninit)
{
    Crypto_Test_ResetInitState();

    Std_ReturnType ret = Crypto_KeySetValid(0U);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CRYPTO_E_UNINIT);
}

TEST_F(Bsw_Crypto_Test, Crypto_KeySetValid_NG_InvalidHandle)
{
    Std_ReturnType ret = Crypto_KeySetValid(CRYPTO_KEY_COUNT);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CRYPTO_E_PARAM_HANDLE);
}

}  // namespace
