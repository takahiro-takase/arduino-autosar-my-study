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

TEST_F(Crypto_ProcessJob_Test, Crypto_ProcessJob_NG_NullInputPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 mac[CRYPTO_CMAC_SIZE];
    Crypto_JobType job = MakeMacGenerateJob(mac);
    job.inputPtr = NULL;

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

TEST_F(Crypto_ProcessJob_Test, Crypto_ProcessJob_NG_NullMacPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 mac[CRYPTO_CMAC_SIZE];
    Crypto_JobType job = MakeMacGenerateJob(mac);
    job.macPtr = NULL;

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

TEST_F(Crypto_ProcessJob_Test, Crypto_ProcessJob_NG_MacLengthAboveCmacSize)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 mac[CRYPTO_CMAC_SIZE];
    Crypto_JobType job = MakeMacGenerateJob(mac);
    job.macLength = CRYPTO_CMAC_SIZE + 1U;   // cryptoKeyId は有効、macLength だけが範囲外

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

TEST_F(Crypto_ProcessJob_Test, Crypto_ProcessJob_NG_KeyNotValidAfterKeyElementSet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 newKey[CRYPTO_AES128_KEY_SIZE] = { 0U };
    ASSERT_EQ(Crypto_KeyElementSet(0U, CRYPTO_KEY_ELEMENT_ID_CIPHER_KEY, newKey, sizeof(newKey)), E_OK);   // 鍵を書き換えると、有効化されるまで使えない
    uint8 mac[CRYPTO_CMAC_SIZE] = { 0 };
    Crypto_JobType job = MakeMacGenerateJob(mac);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Crypto_ProcessJob(CRYPTO_OBJECT_ID, &job);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, CRYPTO_E_KEY_NOT_VALID);
}

TEST_F(Crypto_ProcessJob_Test, Crypto_ProcessJob_NG_UnsupportedServiceIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 mac[CRYPTO_CMAC_SIZE] = { 0 };
    Crypto_JobType job = MakeMacGenerateJob(mac);
    job.service = static_cast<Crypto_ServiceInfoType>(0x7F);   // 対応していない用途

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Crypto_ProcessJob(CRYPTO_OBJECT_ID, &job);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
}

TEST_F(Crypto_ProcessJob_Test, Crypto_ProcessJob_OK_MacGenerateCopiesRequestedLengthOfCmac)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 full[CRYPTO_CMAC_SIZE] = { 0 };
    uint8 head[3] = { 0 };
    Crypto_JobType jobFull = MakeMacGenerateJob(full);
    Crypto_JobType jobHead = MakeMacGenerateJob(head);
    jobHead.macLength = 3U;   // 先頭 3 バイトだけ求める

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType retFull = Crypto_ProcessJob(CRYPTO_OBJECT_ID, &jobFull);
    const Std_ReturnType retHead = Crypto_ProcessJob(CRYPTO_OBJECT_ID, &jobHead);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(retFull, E_OK);
    EXPECT_EQ(retHead, E_OK);
    EXPECT_EQ(head[0], full[0]);   // 全長の MAC の先頭と一致する
    EXPECT_EQ(head[1], full[1]);
    EXPECT_EQ(head[2], full[2]);
    uint8 any = 0U;
    for (uint8 b = 0U; b < CRYPTO_CMAC_SIZE; b++) { any |= full[b]; }
    EXPECT_NE(any, 0U);            // 0 のままではない（実際に計算している）
}

}  // namespace
