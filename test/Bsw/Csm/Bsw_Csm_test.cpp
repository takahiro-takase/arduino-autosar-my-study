/**
 * \file    Bsw_Csm_test.cpp
 * \brief   Csm.c（src/Bsw/Csm/Csm.c）の単体テスト（NG系）
 * \details Csm.c 全公開APIの `Det_ReportError()` 呼び出し箇所（NG ケースの
 *          み）を1ファイルにまとめる。各ケースで、報告される ErrorId が
 *          仕様どおり正しい値になっていることを `Fake_Det_Hw.h`
 *          （`FakeDetHw_LastErrorId`）で検証する。
 *
 *          `Csm_MacGenerate()` の `CSM_E_SERVICE_NOT_STARTED`（[SWS_Csm_91010]、
 *          CryIf 未初期化）は、本プロジェクトの `Csm_JobConfigData`
 *          （Csm_PBCfg.c）が `CRYPTO_MACVERIFY` の1件しか構成しないため、
 *          あらゆる jobId で「一致するジョブなし」（`CSM_E_PARAM_HANDLE`）が
 *          先に確定してしまい到達不能。対象外とする（`Csm_MacVerify()` 側は
 *          その1件が MACVERIFY のため到達可能で、通常どおりテストする）。
 *
 *          Csm には `Csm_DeInit()` に相当する API が無く、`Csm_Initialized` は
 *          native_chain_tests バイナリ全体で共有される static のため、
 *          `CSM_E_UNINIT` の検証には `Csm_Test_ResetInitState()`
 *          （`CSM_UNIT_TEST` ビルドのみに存在するテスト専用関数、
 *          `Mcu_Test_ResetInitState()` と同じ設計）を使う。
 *
 *          `CSM_E_SERVICE_NOT_STARTED` 系のテストは、SetUp() が意図的に
 *          `CryIf_Init()` を呼ばない（native_chain バイナリ全体で
 *          `CryIf_Initialized` は既定 0、かつ `Bsw_CryIf_test.cpp` 側が
 *          自身の TearDown() で必ず未初期化へ戻すため、実行順序に関わらず
 *          安全）ことに依存する。
 *
 *          GoogleTest の main() は test_main.cpp に集約しているため、
 *          本ファイルでは定義しない。
 */
#include <gtest/gtest.h>

extern "C" {
#include "Csm.h"
#include "CryIf.h"
#include "Crypto.h"
#include "Crypto_Cfg.h"
#include "Crypto_Cmac.h"
#include "Fake_Det_Hw.h"
}

namespace
{

class Bsw_Csm_Test : public ::testing::Test
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

// ------------------------------------------------------------
// Csm_GetVersionInfo()
// ------------------------------------------------------------

TEST_F(Bsw_Csm_Test, Csm_GetVersionInfo_NG_Uninit)
{
    Csm_Test_ResetInitState();

    Csm_GetVersionInfo(NULL);

    EXPECT_EQ(FakeDetHw_LastErrorId, CSM_E_UNINIT);
}

TEST_F(Bsw_Csm_Test, Csm_GetVersionInfo_NG_NullPointer)
{
    Csm_GetVersionInfo(NULL);

    EXPECT_EQ(FakeDetHw_LastErrorId, CSM_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// Csm_MacGenerate()
// ------------------------------------------------------------

TEST_F(Bsw_Csm_Test, Csm_MacGenerate_NG_Uninit)
{
    Csm_Test_ResetInitState();
    uint8  data[1] = { 0 };
    uint8  mac[CRYPTO_CMAC_SIZE];
    uint32 macLen = sizeof(mac);

    Std_ReturnType ret = Csm_MacGenerate(CSM_JOB_ID_IMMOBILIZER_CMD_VERIFY, CRYPTO_OPERATIONMODE_SINGLECALL,
                                          data, sizeof(data), mac, &macLen);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CSM_E_UNINIT);
}

TEST_F(Bsw_Csm_Test, Csm_MacGenerate_NG_NullPointer)
{
    uint32 macLen = CRYPTO_CMAC_SIZE;

    Std_ReturnType ret = Csm_MacGenerate(CSM_JOB_ID_IMMOBILIZER_CMD_VERIFY, CRYPTO_OPERATIONMODE_SINGLECALL,
                                          NULL, 1U, NULL, &macLen);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CSM_E_PARAM_POINTER);
}

TEST_F(Bsw_Csm_Test, Csm_MacGenerate_NG_SmallBuffer)
{
    uint8  data[1] = { 0 };
    uint8  mac[CRYPTO_CMAC_SIZE + 1U];
    uint32 macLen = sizeof(mac);  // CRYPTO_CMAC_SIZE を超過

    Std_ReturnType ret = Csm_MacGenerate(CSM_JOB_ID_IMMOBILIZER_CMD_VERIFY, CRYPTO_OPERATIONMODE_SINGLECALL,
                                          data, sizeof(data), mac, &macLen);

    EXPECT_EQ(ret, CRYPTO_E_SMALL_BUFFER);
    EXPECT_EQ(FakeDetHw_LastErrorId, CSM_E_SMALL_BUFFER);
}

TEST_F(Bsw_Csm_Test, Csm_MacGenerate_NG_NoMatchingJob)
{
    // Csm_JobConfigData には CRYPTO_MACGENERATE のジョブが1件も無い
    // （ファイル冒頭コメント参照）ため、どの jobId でも一致しない。
    uint8  data[1] = { 0 };
    uint8  mac[CRYPTO_CMAC_SIZE];
    uint32 macLen = sizeof(mac);

    Std_ReturnType ret = Csm_MacGenerate(CSM_JOB_ID_IMMOBILIZER_CMD_VERIFY, CRYPTO_OPERATIONMODE_SINGLECALL,
                                          data, sizeof(data), mac, &macLen);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CSM_E_PARAM_HANDLE);
}

// ------------------------------------------------------------
// Csm_MacVerify()
// ------------------------------------------------------------

TEST_F(Bsw_Csm_Test, Csm_MacVerify_NG_Uninit)
{
    Csm_Test_ResetInitState();
    uint8 data[1] = { 0 };
    uint8 mac[CRYPTO_CMAC_SIZE] = { 0 };
    Crypto_VerifyResultType verifyResult;

    Std_ReturnType ret = Csm_MacVerify(CSM_JOB_ID_IMMOBILIZER_CMD_VERIFY, CRYPTO_OPERATIONMODE_SINGLECALL,
                                        data, sizeof(data), mac, CRYPTO_CMAC_SIZE * 8U, &verifyResult);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CSM_E_UNINIT);
}

TEST_F(Bsw_Csm_Test, Csm_MacVerify_NG_NullPointer)
{
    Crypto_VerifyResultType verifyResult;

    Std_ReturnType ret = Csm_MacVerify(CSM_JOB_ID_IMMOBILIZER_CMD_VERIFY, CRYPTO_OPERATIONMODE_SINGLECALL,
                                        NULL, 1U, NULL, CRYPTO_CMAC_SIZE * 8U, &verifyResult);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CSM_E_PARAM_POINTER);
}

TEST_F(Bsw_Csm_Test, Csm_MacVerify_NG_InvalidMacLengthNotByteAligned)
{
    uint8 data[1] = { 0 };
    uint8 mac[CRYPTO_CMAC_SIZE] = { 0 };
    Crypto_VerifyResultType verifyResult;

    Std_ReturnType ret = Csm_MacVerify(CSM_JOB_ID_IMMOBILIZER_CMD_VERIFY, CRYPTO_OPERATIONMODE_SINGLECALL,
                                        data, sizeof(data), mac, 7U, &verifyResult);  // 8の倍数でない

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CSM_E_PARAM_HANDLE);
}

TEST_F(Bsw_Csm_Test, Csm_MacVerify_NG_InvalidMacLengthExceedsCmacSize)
{
    uint8 data[1] = { 0 };
    uint8 mac[CRYPTO_CMAC_SIZE] = { 0 };
    Crypto_VerifyResultType verifyResult;

    Std_ReturnType ret = Csm_MacVerify(CSM_JOB_ID_IMMOBILIZER_CMD_VERIFY, CRYPTO_OPERATIONMODE_SINGLECALL,
                                        data, sizeof(data), mac, (CRYPTO_CMAC_SIZE + 1U) * 8U, &verifyResult);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CSM_E_PARAM_HANDLE);
}

TEST_F(Bsw_Csm_Test, Csm_MacVerify_NG_NoMatchingJob)
{
    uint8 data[1] = { 0 };
    uint8 mac[CRYPTO_CMAC_SIZE] = { 0 };
    Crypto_VerifyResultType verifyResult;

    Std_ReturnType ret = Csm_MacVerify(CSM_JOB_ID_IMMOBILIZER_CMD_VERIFY + 1U, CRYPTO_OPERATIONMODE_SINGLECALL,
                                        data, sizeof(data), mac, CRYPTO_CMAC_SIZE * 8U, &verifyResult);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CSM_E_PARAM_HANDLE);
}

TEST_F(Bsw_Csm_Test, Csm_MacVerify_NG_ServiceNotStarted)
{
    // CryIf_Init() を意図的に呼ばない（ファイル冒頭コメント参照）。
    uint8 data[1] = { 0 };
    uint8 mac[CRYPTO_CMAC_SIZE] = { 0 };
    Crypto_VerifyResultType verifyResult;

    Std_ReturnType ret = Csm_MacVerify(CSM_JOB_ID_IMMOBILIZER_CMD_VERIFY, CRYPTO_OPERATIONMODE_SINGLECALL,
                                        data, sizeof(data), mac, CRYPTO_CMAC_SIZE * 8U, &verifyResult);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CSM_E_SERVICE_NOT_STARTED);
}

// ------------------------------------------------------------
// Csm_KeyElementSet()
// ------------------------------------------------------------

TEST_F(Bsw_Csm_Test, Csm_KeyElementSet_NG_Uninit)
{
    Csm_Test_ResetInitState();
    uint8 key[16] = { 0 };

    Std_ReturnType ret = Csm_KeyElementSet(0U, 1U, key, sizeof(key));

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CSM_E_UNINIT);
}

TEST_F(Bsw_Csm_Test, Csm_KeyElementSet_NG_NullPointer)
{
    Std_ReturnType ret = Csm_KeyElementSet(0U, 1U, NULL, 16U);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CSM_E_PARAM_POINTER);
}

TEST_F(Bsw_Csm_Test, Csm_KeyElementSet_NG_InvalidKeyId)
{
    uint8 key[16] = { 0 };

    Std_ReturnType ret = Csm_KeyElementSet(CRYPTO_KEY_COUNT, 1U, key, sizeof(key));

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CSM_E_PARAM_HANDLE);
}

TEST_F(Bsw_Csm_Test, Csm_KeyElementSet_NG_ServiceNotStarted)
{
    uint8 key[16] = { 0 };

    Std_ReturnType ret = Csm_KeyElementSet(0U, 1U, key, sizeof(key));

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CSM_E_SERVICE_NOT_STARTED);
}

// ------------------------------------------------------------
// Csm_KeySetValid()
// ------------------------------------------------------------

TEST_F(Bsw_Csm_Test, Csm_KeySetValid_NG_Uninit)
{
    Csm_Test_ResetInitState();

    Std_ReturnType ret = Csm_KeySetValid(0U);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CSM_E_UNINIT);
}

TEST_F(Bsw_Csm_Test, Csm_KeySetValid_NG_InvalidKeyId)
{
    Std_ReturnType ret = Csm_KeySetValid(CRYPTO_KEY_COUNT);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CSM_E_PARAM_HANDLE);
}

TEST_F(Bsw_Csm_Test, Csm_KeySetValid_NG_ServiceNotStarted)
{
    Std_ReturnType ret = Csm_KeySetValid(0U);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CSM_E_SERVICE_NOT_STARTED);
}

// ------------------------------------------------------------
// Csm_KeyElementGet()
// ------------------------------------------------------------

TEST_F(Bsw_Csm_Test, Csm_KeyElementGet_NG_Uninit)
{
    Csm_Test_ResetInitState();
    uint8  buf[16];
    uint32 len = sizeof(buf);

    Std_ReturnType ret = Csm_KeyElementGet(0U, 1U, buf, &len);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CSM_E_UNINIT);
}

TEST_F(Bsw_Csm_Test, Csm_KeyElementGet_NG_NullPointer)
{
    uint32 len = 16U;

    Std_ReturnType ret = Csm_KeyElementGet(0U, 1U, NULL, &len);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CSM_E_PARAM_POINTER);
}

TEST_F(Bsw_Csm_Test, Csm_KeyElementGet_NG_InvalidKeyId)
{
    uint8  buf[16];
    uint32 len = sizeof(buf);

    Std_ReturnType ret = Csm_KeyElementGet(CRYPTO_KEY_COUNT, 1U, buf, &len);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CSM_E_PARAM_HANDLE);
}

TEST_F(Bsw_Csm_Test, Csm_KeyElementGet_NG_ServiceNotStarted)
{
    uint8  buf[16];
    uint32 len = sizeof(buf);

    Std_ReturnType ret = Csm_KeyElementGet(0U, 1U, buf, &len);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CSM_E_SERVICE_NOT_STARTED);
}

TEST_F(Bsw_Csm_Test, Csm_KeyElementGet_NG_SmallBuffer)
{
    // 下位層まで実チェーンを通す必要があるため、ここでのみ Crypto/CryIf も
    // 初期化する（他のテストは CryIf 未初期化のまま SERVICE_NOT_STARTED で
    // 早期returnするため不要）。
    FakeDetHw_LogSuppressed = 1U;
    Crypto_Init();
    CryIf_Init();
    FakeDetHw_LogSuppressed = 0U;

    uint8  buf[16];
    uint32 len = CRYPTO_AES128_KEY_SIZE - 1U;

    Std_ReturnType ret = Csm_KeyElementGet(0U, 1U, buf, &len);

    EXPECT_EQ(ret, CRYPTO_E_SMALL_BUFFER);
    EXPECT_EQ(FakeDetHw_LastErrorId, CSM_E_SMALL_BUFFER);
}

}  // namespace
