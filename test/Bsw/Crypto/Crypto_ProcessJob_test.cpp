/**
 * \file    Crypto_ProcessJob_test.cpp
 * \brief   Crypto_ProcessJob() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 Crypto_test.cpp）。
 *
 *          分割前の共通の説明は、旧 Crypto_test.cpp の冒頭コメント（git の履歴）を参照。
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

/* Crypto の単体テスト用フィクスチャ。
 * SetUp(): Crypto_Init() を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。MAC 生成ジョブを組み立てる補助関数 MakeMacGenerateJob() も持つ。
 * TearDown(): 未初期化の状態へ戻す。 */
class Crypto_ProcessJob_Test : public ::testing::Test
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

const uint8 Crypto_ProcessJob_Test::kInput[4] = { 0x01U, 0x02U, 0x03U, 0x04U };

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Crypto_ProcessJob_Test, Crypto_ProcessJob_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Crypto_Test_ResetInitState();
    uint8 mac[CRYPTO_CMAC_SIZE];
    Crypto_JobType job = MakeMacGenerateJob(mac);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Crypto_ProcessJob(CRYPTO_OBJECT_ID, &job);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CRYPTO_E_UNINIT);
}

TEST_F(Crypto_ProcessJob_Test, Crypto_ProcessJob_NG_InvalidObjectId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 mac[CRYPTO_CMAC_SIZE];
    Crypto_JobType job = MakeMacGenerateJob(mac);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Crypto_ProcessJob(CRYPTO_OBJECT_ID + 1U, &job);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CRYPTO_E_PARAM_HANDLE);
}

TEST_F(Crypto_ProcessJob_Test, Crypto_ProcessJob_NG_NullJobPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Crypto_ProcessJob(CRYPTO_OBJECT_ID, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CRYPTO_E_PARAM_POINTER);
}

TEST_F(Crypto_ProcessJob_Test, Crypto_ProcessJob_NG_InvalidCryptoKeyId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 mac[CRYPTO_CMAC_SIZE];
    Crypto_JobType job = MakeMacGenerateJob(mac);
    job.cryptoKeyId = CRYPTO_KEY_COUNT;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Crypto_ProcessJob(CRYPTO_OBJECT_ID, &job);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CRYPTO_E_PARAM_HANDLE);
}

TEST_F(Crypto_ProcessJob_Test, Crypto_ProcessJob_NG_NullVerifyResultPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 mac[CRYPTO_CMAC_SIZE] = { 0 };
    Crypto_JobType job = MakeMacGenerateJob(mac);
    job.service         = CRYPTO_MACVERIFY;
    job.verifyResultPtr = NULL;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Crypto_ProcessJob(CRYPTO_OBJECT_ID, &job);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CRYPTO_E_PARAM_POINTER);
}

}  // namespace
