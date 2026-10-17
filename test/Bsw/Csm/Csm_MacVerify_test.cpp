/**
 * \file    Csm_MacVerify_test.cpp
 * \brief   Csm_MacVerify() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 Csm_test.cpp）。
 *
 *          `Csm_MacGenerate()` の `CSM_E_SERVICE_NOT_STARTED`（[SWS_Csm_91010]、
 *          CryIf 未初期化）は、本プロジェクトの `Csm_JobConfigData`
 *          （Csm_PBCfg.c）が `CRYPTO_MACVERIFY` の1件しか構成しないため、
 *          あらゆる jobId で「一致するジョブなし」（`CSM_E_PARAM_HANDLE`）が
 *          先に確定してしまい到達不能。対象外とする（`Csm_MacVerify()` 側は
 *          その1件が MACVERIFY のため到達可能で、通常どおりテストする）。
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
class Csm_MacVerify_Test : public ::testing::Test
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

TEST_F(Csm_MacVerify_Test, Csm_MacVerify_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Csm_Test_ResetInitState();
    uint8 data[1] = { 0 };
    uint8 mac[CRYPTO_CMAC_SIZE] = { 0 };
    Crypto_VerifyResultType verifyResult;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Csm_MacVerify(CSM_JOB_ID_IMMOBILIZER_CMD_VERIFY, CRYPTO_OPERATIONMODE_SINGLECALL,
                                        data, sizeof(data), mac, CRYPTO_CMAC_SIZE * 8U, &verifyResult);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CSM_E_UNINIT);
}

TEST_F(Csm_MacVerify_Test, Csm_MacVerify_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Crypto_VerifyResultType verifyResult;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Csm_MacVerify(CSM_JOB_ID_IMMOBILIZER_CMD_VERIFY, CRYPTO_OPERATIONMODE_SINGLECALL,
                                        NULL, 1U, NULL, CRYPTO_CMAC_SIZE * 8U, &verifyResult);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CSM_E_PARAM_POINTER);
}

TEST_F(Csm_MacVerify_Test, Csm_MacVerify_NG_InvalidMacLengthNotByteAligned)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 data[1] = { 0 };
    uint8 mac[CRYPTO_CMAC_SIZE] = { 0 };
    Crypto_VerifyResultType verifyResult;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Csm_MacVerify(CSM_JOB_ID_IMMOBILIZER_CMD_VERIFY, CRYPTO_OPERATIONMODE_SINGLECALL,
                                        data, sizeof(data), mac, 7U, &verifyResult);  // 8の倍数でない

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CSM_E_PARAM_HANDLE);
}

TEST_F(Csm_MacVerify_Test, Csm_MacVerify_NG_InvalidMacLengthExceedsCmacSize)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 data[1] = { 0 };
    uint8 mac[CRYPTO_CMAC_SIZE] = { 0 };
    Crypto_VerifyResultType verifyResult;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Csm_MacVerify(CSM_JOB_ID_IMMOBILIZER_CMD_VERIFY, CRYPTO_OPERATIONMODE_SINGLECALL,
                                        data, sizeof(data), mac, (CRYPTO_CMAC_SIZE + 1U) * 8U, &verifyResult);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CSM_E_PARAM_HANDLE);
}

TEST_F(Csm_MacVerify_Test, Csm_MacVerify_NG_NoMatchingJob)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 data[1] = { 0 };
    uint8 mac[CRYPTO_CMAC_SIZE] = { 0 };
    Crypto_VerifyResultType verifyResult;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Csm_MacVerify(CSM_JOB_ID_IMMOBILIZER_CMD_VERIFY + 1U, CRYPTO_OPERATIONMODE_SINGLECALL,
                                        data, sizeof(data), mac, CRYPTO_CMAC_SIZE * 8U, &verifyResult);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CSM_E_PARAM_HANDLE);
}

TEST_F(Csm_MacVerify_Test, Csm_MacVerify_NG_ServiceNotStarted)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // CryIf_Init() を意図的に呼ばない（ファイル冒頭コメント参照）。
    uint8 data[1] = { 0 };
    uint8 mac[CRYPTO_CMAC_SIZE] = { 0 };
    Crypto_VerifyResultType verifyResult;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Csm_MacVerify(CSM_JOB_ID_IMMOBILIZER_CMD_VERIFY, CRYPTO_OPERATIONMODE_SINGLECALL,
                                        data, sizeof(data), mac, CRYPTO_CMAC_SIZE * 8U, &verifyResult);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CSM_E_SERVICE_NOT_STARTED);
}

TEST_F(Csm_MacVerify_Test, Csm_MacVerify_NG_NullMacPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    const uint8 data[1] = { 0x01U };
    Crypto_VerifyResultType verifyResult;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Csm_MacVerify(CSM_JOB_ID_IMMOBILIZER_CMD_VERIFY, CRYPTO_OPERATIONMODE_SINGLECALL,
                                        data, 1U, NULL, CRYPTO_CMAC_SIZE * 8U, &verifyResult);   // dataPtr は有効、macPtr だけ NULL

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CSM_E_PARAM_POINTER);
}

TEST_F(Csm_MacVerify_Test, Csm_MacVerify_NG_NullVerifyPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    const uint8 data[1] = { 0x01U };
    const uint8 mac[CRYPTO_CMAC_SIZE] = { 0U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Csm_MacVerify(CSM_JOB_ID_IMMOBILIZER_CMD_VERIFY, CRYPTO_OPERATIONMODE_SINGLECALL,
                                        data, 1U, mac, CRYPTO_CMAC_SIZE * 8U, NULL);   // verifyPtr だけ NULL

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CSM_E_PARAM_POINTER);
}

}  // namespace
