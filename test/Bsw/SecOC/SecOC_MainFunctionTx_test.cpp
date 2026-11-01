/**
 * \file    SecOC_MainFunctionTx_test.cpp
 * \brief   SecOC_MainFunctionTx() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 SecOC_test.cpp）。
 *
 *          分割前の共通の説明は、旧 SecOC_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "SecOC.h"
#include "SecOC_Cfg.h"
#include "SecOC_PBCfg.h"
#include "Csm.h"
#include "CryIf.h"
#include "Fake_Det_Hw.h"
#include "Wrap_Csm.h"
#include "Wrap_PduR.h"
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

/* SecOC の単体テスト用フィクスチャ。
 * SetUp(): SecOC_Init(&SecOC_Config) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。MakeValidSecuredPdu() は、有効な Secured I-PDU を作る補助関数。
 * TearDown(): SecOC_DeInit() を呼び、Csm と CryIf を未初期化の状態へ戻す。 */
class SecOC_MainFunctionTx_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        SecOC_Init(&SecOC_Config);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        SecOC_DeInit();
        Csm_Test_ResetInitState();
        CryIf_Test_ResetInitState();
    }

    PduInfoType MakeValidSecuredPdu(uint8* buf)
    {
        for (uint8 i = 0U; i < 6U; i++)
            buf[i] = 0U;
        PduInfoType info = { buf, 6U };  // SecOC_Config の RxPdu[0].SecuredPduLength=6
        return info;
    }
};

/* SecOC_MainFunctionTx() の、Csm_MacGenerate() の失敗時の扱い（[SWS_SecOC_00155]/[SWS_SecOC_00166]/[SWS_SecOC_00227]/[SWS_SecOC_00228]）の
 * 単体テスト用フィクスチャ。
 * SetUp(): TX Secured I-PDU を 1 件（Authentic 2 バイト、Freshness 1 バイト、MAC 3 バイト）持つ設定で SecOC_Init() を呼ぶ
 *          （CMakeLists.txt で、SecOC.c の SECOC_TX_PDU_MAX を 1 にしている）。Csm_MacGenerate() は wrap で制御する
 *          （本番の Csm のジョブは MAC の検証用だけで、生成用が無い）。
 * Attempts(): SecOCAuthenticationBuildAttempts を設定して、SecOC_Init() をやり直す。
 * TearDown(): SecOC_DeInit() と、Csm・CryIf の未初期化への復帰。 */
class SecOC_MainFunctionTx_CsmFailure_Test : public ::testing::Test
{
protected:
    SecOC_TxPduConfigType txCfg;
    SecOC_ConfigType      cfg;

    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;
        txCfg = {};
        txCfg.SecOCTxPduId       = 0U;
        txCfg.DataId             = 0x0300U;
        txCfg.AuthenticPduLength = 2U;
        txCfg.FreshnessOffset    = 2U;
        txCfg.FreshnessLength    = 1U;
        txCfg.MacOffset          = 3U;
        txCfg.MacTxLength        = 3U;
        txCfg.SecuredPduLength   = 6U;
        txCfg.CsmJobId           = 0U;
        txCfg.PduRSrcPduId       = 0U;
        cfg = {};
        cfg.TxPdus     = &txCfg;
        cfg.TxPduCount = 1U;
        SecOC_Init(&cfg);

        WrapPduR_Reset();
        WrapCsm_Reset();
        FailFromCallCount_PduR_SecOCTransmit = 1U;   // PduR_SecOCTransmit() は、実体を呼ばずに成功する（PduR は未初期化）
        ForcedReturn_PduR_SecOCTransmit      = E_OK;
        FakeDetHw_Reset();
        FakeDetHw_LogSuppressed = 0U;
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        SecOC_DeInit();
        Csm_Test_ResetInitState();
        CryIf_Test_ResetInitState();
        WrapPduR_Reset();
        WrapCsm_Reset();
    }

    // SecOCAuthenticationBuildAttempts を設定して、初期化し直す。
    void Attempts(uint16 attempts)
    {
        FakeDetHw_LogSuppressed = 1U;
        SecOC_DeInit();
        txCfg.AuthenticationBuildAttempts = attempts;
        SecOC_Init(&cfg);
        FakeDetHw_Reset();
        FakeDetHw_LogSuppressed = 0U;
    }

    // Csm_MacGenerate() が、今回の呼び出しから、ret を返すようにする。
    static void CsmReturns(Std_ReturnType ret)
    {
        FailFromCallCount_Csm_MacGenerate = 1U;
        ForcedReturn_Csm_MacGenerate      = ret;
    }

    // Authentic I-PDU（0x11、0x22）を渡す（SecOC_IfTransmit()）。
    static void Queue()
    {
        uint8 authentic[2] = { 0x11U, 0x22U };
        PduInfoType info = { authentic, 2U };
        ASSERT_EQ(SecOC_IfTransmit(0U, &info), E_OK);
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(SecOC_MainFunctionTx_Test, SecOC_MainFunctionTx_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    SecOC_DeInit();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    SecOC_MainFunctionTx();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, SECOC_E_UNINIT);
}

TEST_F(SecOC_MainFunctionTx_CsmFailure_Test, SecOC_MainFunctionTx_OK_CsmSuccessSendsSecuredPdu)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Queue();
    CsmReturns(E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    SecOC_MainFunctionTx();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_PduR_SecOCTransmit, 1U);   // Secured I-PDU の送信を、PduR へ要求する
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(SecOC_MainFunctionTx_CsmFailure_Test, SecOC_MainFunctionTx_NG_CsmNotOkDropsAuthenticPduAndReportsCryptoFailure)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Queue();
    CsmReturns(E_NOT_OK);   // 回復可能なエラーではない（[SWS_SecOC_00227] の注記）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    SecOC_MainFunctionTx();
    const uint32 transmitsAfterFirst = CallCount_PduR_SecOCTransmit;
    const uint8 errorAfterFirst = FakeDetHw_LastErrorId;
    const uint8 apiAfterFirst = FakeDetHw_LastApiId;
    SecOC_MainFunctionTx();   // 捨てているので、次の周期でも、やり直さない

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(transmitsAfterFirst, 0U);                     // MAC が無い Secured I-PDU は、送らない
    EXPECT_EQ(errorAfterFirst, SECOC_E_CRYPTO_FAILURE);     // [SWS_SecOC_00166]
    EXPECT_EQ(apiAfterFirst, SECOC_API_ID_MAIN_FUNCTION_TX);
    EXPECT_EQ(CallCount_Csm_MacGenerate, 1U);               // 次の周期では、Csm を呼ばない（Authentic I-PDU を捨てた）
    EXPECT_EQ(CallCount_PduR_SecOCTransmit, 0U);
    EXPECT_EQ(CallCount_PduR_SecOCTxConfirmation, 1U);      // 上位層へ、送信の失敗（E_NOT_OK）を、送信確認として通知する（1 回だけ）
}

TEST_F(SecOC_MainFunctionTx_CsmFailure_Test, SecOC_MainFunctionTx_NG_CsmBusyWithoutAttemptsIsFinalAtFirstFailure)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Queue();
    CsmReturns(CRYPTO_E_BUSY);   // 回復可能なエラー。ただし、SecOCAuthenticationBuildAttempts が未設定（1 回だけ試す）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    SecOC_MainFunctionTx();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, SECOC_E_CRYPTO_FAILURE);   // 試行の回数が上限（1）に達したので、断念する（[SWS_SecOC_00155]）
    EXPECT_EQ(CallCount_PduR_SecOCTransmit, 0U);
}

TEST_F(SecOC_MainFunctionTx_CsmFailure_Test, SecOC_MainFunctionTx_OK_CsmBusyRetriesUntilAttemptsReached)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Attempts(3U);
    Queue();
    CsmReturns(CRYPTO_E_BUSY);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    SecOC_MainFunctionTx();                       // 1 回目: 回復可能なエラー。やり直す
    const uint32 reportsAfter1 = FakeDetHw_ReportCount;
    SecOC_MainFunctionTx();                       // 2 回目: やり直す
    const uint32 reportsAfter2 = FakeDetHw_ReportCount;
    SecOC_MainFunctionTx();                       // 3 回目: 試行の回数が上限（3）に達する。断念する
    const uint32 csmCallsAfter3 = CallCount_Csm_MacGenerate;
    SecOC_MainFunctionTx();                       // 断念した後は、やり直さない

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(reportsAfter1, 0U);                 // 上限に達していない間は、DET へ報告しない（[SWS_SecOC_00228]）
    EXPECT_EQ(reportsAfter2, 0U);
    EXPECT_EQ(csmCallsAfter3, 3U);
    EXPECT_EQ(FakeDetHw_LastErrorId, SECOC_E_CRYPTO_FAILURE);   // 上限に達したので、報告する（[SWS_SecOC_00155]）
    EXPECT_EQ(CallCount_Csm_MacGenerate, 3U);     // 断念した後は、Csm を呼ばない
    EXPECT_EQ(CallCount_PduR_SecOCTransmit, 0U);
    EXPECT_EQ(CallCount_PduR_SecOCTxConfirmation, 1U);   // 断念したときに、1 回だけ通知する（再試行の間は、通知しない）
}

TEST_F(SecOC_MainFunctionTx_CsmFailure_Test, SecOC_MainFunctionTx_OK_CsmQueueFullIsRecoverable)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Attempts(2U);
    Queue();
    CsmReturns(CRYPTO_E_QUEUE_FULL);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    SecOC_MainFunctionTx();   // 1 回目: 回復可能なエラー

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);   // やり直すので、まだ報告しない
    EXPECT_EQ(CallCount_PduR_SecOCTransmit, 0U);
}

TEST_F(SecOC_MainFunctionTx_CsmFailure_Test, SecOC_MainFunctionTx_OK_RetrySucceedsAndSends)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Attempts(3U);
    Queue();
    CsmReturns(CRYPTO_E_BUSY);
    SecOC_MainFunctionTx();   // 1 回目: 回復可能なエラー
    CsmReturns(E_OK);         // 次の周期では、成功する

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    SecOC_MainFunctionTx();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Csm_MacGenerate, 2U);
    EXPECT_EQ(CallCount_PduR_SecOCTransmit, 1U);   // やり直しが成功して、送信する
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(SecOC_MainFunctionTx_CsmFailure_Test, SecOC_MainFunctionTx_OK_NewTransmitRequestResetsAttemptCounter)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Attempts(3U);
    Queue();
    CsmReturns(CRYPTO_E_BUSY);
    SecOC_MainFunctionTx();   // 試行 1
    SecOC_MainFunctionTx();   // 試行 2
    Queue();                  // 新しい送信要求。試行の回数を 0 に戻す（[SWS_SecOC_00226]）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    SecOC_MainFunctionTx();   // 新しい送信要求の、試行 1

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);   // 試行の回数が戻っているので、まだ断念しない（戻っていなければ、ここで 3 回目になる）
    EXPECT_EQ(CallCount_Csm_MacGenerate, 3U);
}

TEST_F(SecOC_MainFunctionTx_CsmFailure_Test, SecOC_MainFunctionTx_OK_SuccessResetsAttemptCounterForReauthentication)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Attempts(3U);
    Queue();
    CsmReturns(CRYPTO_E_BUSY);
    SecOC_MainFunctionTx();   // 試行 1（回復可能なエラー）
    SecOC_MainFunctionTx();   // 試行 2
    CsmReturns(E_OK);
    SecOC_MainFunctionTx();   // 成功して、送信する（試行の回数を 0 に戻す）
    uint8 buf[8];
    PduInfoType info = { buf, 8U };
    ASSERT_EQ(SecOC_TriggerTransmit(0U, &info), E_OK);   // 下位層が、Secured I-PDU を取り出す
    SecOC_TxConfirmation(0U, E_OK);                      // 送信が確認される。Authentic I-PDU を再び認証する（[SWS_SecOC_00069]）
    CsmReturns(CRYPTO_E_BUSY);
    FakeDetHw_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    SecOC_MainFunctionTx();   // 再認証の、試行 1
    SecOC_MainFunctionTx();   // 再認証の、試行 2

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);   // 試行の回数が 0 から数え直されるので、まだ断念しない（戻っていなければ、ここで断念する）
}

TEST_F(SecOC_MainFunctionTx_CsmFailure_Test, SecOC_MainFunctionTx_NG_CsmFailureDoesNotAdvanceFreshness)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Queue();
    CsmReturns(E_NOT_OK);
    SecOC_MainFunctionTx();   // 失敗して、Authentic I-PDU を捨てる
    CsmReturns(E_OK);
    Queue();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    SecOC_MainFunctionTx();
    PduInfoType secured = { NULL, 0U };
    uint8 buf[8] = { 0xEEU, 0xEEU, 0xEEU, 0xEEU, 0xEEU, 0xEEU, 0xEEU, 0xEEU };
    secured.SduDataPtr = buf;
    secured.SduLength  = 8U;
    const Std_ReturnType ret = SecOC_TriggerTransmit(0U, &secured);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_PduR_SecOCTransmit, 1U);
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(buf[2], 0x00U);   // 失敗した試行では、Freshness Value を進めない（最初に送る Secured I-PDU の Freshness Value は 0）
}

}  // namespace
