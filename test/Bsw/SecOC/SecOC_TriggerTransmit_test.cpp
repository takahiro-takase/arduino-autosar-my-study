/**
 * \file    SecOC_TriggerTransmit_test.cpp
 * \brief   SecOC_TriggerTransmit() の単体テスト
 * \details 下位層が、送信の直前に、SecOC が保持する Secured I-PDU を取りに来る経路（[SWS_SecOC_00068]）。
 *          SecOC_IfTransmit() で Authentic I-PDU を渡し、SecOC_MainFunctionTx() が Secured I-PDU を組み立てた後で、
 *          複製できること、繰り返し複製できること、まだ組み立てていないときは E_NOT_OK（[SWS_SecOC_00150]）、
 *          バッファが小さいときは何も複製しないこと、不正な TxPduId、NULL ポインタ、未初期化を確認する。
 *
 *          本番の設定は TX 方向で SecOC を使わない（SECOC_TX_PDU_COUNT=0）ので、本テストは、TX Secured I-PDU を
 *          1 件持つテスト専用の設定で SecOC を初期化する（CMakeLists.txt で、SecOC.c の SECOC_TX_PDU_MAX を 1 にしている）。
 *          Csm のジョブは、本番にある検証用のジョブ（MAC の生成には使えない）を指すだけで、MAC の値は確認しない。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "SecOC.h"
#include "SecOC_Cfg.h"
#include "Csm.h"
#include "Csm_Cfg.h"
#include "CryIf.h"
#include "Crypto.h"
#include "Fake_Det_Hw.h"
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

const PduIdType kTxPduId = 0U;

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* SetUp(): Crypto・CryIf・Csm を初期化し、TX Secured I-PDU を 1 件（Authentic 2 バイト、Freshness 1 バイト、MAC 3 バイトの
 *          合計 6 バイト）持つ設定で SecOC_Init() を呼ぶ。PduR への送信の記録と、DET の記録を初期化する。
 * Authenticate(): Authentic I-PDU（0x11、0x22）を渡して、SecOC_MainFunctionTx() で Secured I-PDU を組み立てる補助関数。
 * TearDown(): SecOC_DeInit() を呼び、Crypto・CryIf・Csm を未初期化へ戻す。 */
class SecOC_TriggerTransmit_Test : public ::testing::Test
{
protected:
    SecOC_TxPduConfigType txCfg;
    SecOC_ConfigType      cfg;

    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Crypto_Test_ResetInitState();
        CryIf_Test_ResetInitState();
        Csm_Test_ResetInitState();
        Crypto_Init();
        CryIf_Init();
        Csm_Init();

        txCfg = {};
        txCfg.SecOCTxPduId       = kTxPduId;
        txCfg.DataId             = 0x0300U;
        txCfg.AuthenticPduLength = 2U;
        txCfg.FreshnessOffset    = 2U;
        txCfg.FreshnessLength    = 1U;
        txCfg.MacOffset          = 3U;
        txCfg.MacTxLength        = 3U;
        txCfg.SecuredPduLength   = 6U;
        txCfg.CsmJobId           = CSM_JOB_ID_IMMOBILIZER_CMD_VERIFY;
        txCfg.PduRSrcPduId       = 0U;
        cfg = {};
        cfg.TxPdus     = &txCfg;
        cfg.TxPduCount = 1U;
        SecOC_Init(&cfg);

        WrapPduR_Reset();
        FakeDetHw_Reset();
        FakeDetHw_LogSuppressed = 0U;
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        SecOC_DeInit();
        Crypto_Test_ResetInitState();
        CryIf_Test_ResetInitState();
        Csm_Test_ResetInitState();
        WrapPduR_Reset();
    }

    void Authenticate()
    {
        uint8 authentic[2] = { 0x11U, 0x22U };
        PduInfoType info = { authentic, 2U };
        ASSERT_EQ(SecOC_IfTransmit(kTxPduId, &info), E_OK);
        SecOC_MainFunctionTx();
        ASSERT_EQ(CallCount_PduR_SecOCTransmit, 1U);   // Secured I-PDU を組み立てて、PduR へ送信を要求した
        FakeDetHw_Reset();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(SecOC_TriggerTransmit_Test, SecOC_TriggerTransmit_OK_CopiesSecuredPduHeldAfterMainFunctionTx)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Authenticate();
    uint8 buf[8];
    for (uint8 i = 0U; i < 8U; i++) { buf[i] = 0xEEU; }
    PduInfoType info = { buf, 8U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = SecOC_TriggerTransmit(kTxPduId, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(info.SduLength, 6U);       // Secured I-PDU の長さ
    EXPECT_EQ(buf[0], 0x11U);            // Authentic I-PDU
    EXPECT_EQ(buf[1], 0x22U);
    EXPECT_EQ(buf[2], 0x00U);            // Freshness Value（最初の送信は 0）
    EXPECT_EQ(buf[6], 0xEEU);            // Secured I-PDU の長さを超える部分には、触れない
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(SecOC_TriggerTransmit_Test, SecOC_TriggerTransmit_NG_SecondRequestWithoutNewSecuredPduIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Authenticate();
    uint8 buf[8];
    for (uint8 i = 0U; i < 8U; i++) { buf[i] = 0xEEU; }
    PduInfoType info = { buf, 8U };
    uint8 first[8];
    PduInfoType firstInfo = { first, 8U };
    ASSERT_EQ(SecOC_TriggerTransmit(kTxPduId, &firstInfo), E_OK);   // 1 回目で、Secured I-PDU を下位層へ渡す

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = SecOC_TriggerTransmit(kTxPduId, &info);   // 組み立て直す前に、再び要求する

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);   // 渡した Secured I-PDU は解放済み。同じ Freshness Value を 2 回送らない（リプレイの防止）
    EXPECT_EQ(buf[0], 0xEEU);
}

TEST_F(SecOC_TriggerTransmit_Test, SecOC_TriggerTransmit_OK_ConfirmationAfterFetchReauthenticatesWithNewFreshness)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Authenticate();
    uint8 buf[8];
    for (uint8 i = 0U; i < 8U; i++) { buf[i] = 0xEEU; }
    PduInfoType info = { buf, 8U };
    uint8 first[8];
    PduInfoType firstInfo = { first, 8U };
    ASSERT_EQ(SecOC_TriggerTransmit(kTxPduId, &firstInfo), E_OK);
    SecOC_TxConfirmation(kTxPduId, E_OK);   // 下位層が、その送信を確認する

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    SecOC_MainFunctionTx();   // 保持している Authentic I-PDU を、新しく設定されたものとして、再び認証する
    const Std_ReturnType ret = SecOC_TriggerTransmit(kTxPduId, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_PduR_SecOCTransmit, 2U);   // 下位層へ、新しい Secured I-PDU の送信を要求した（[SWS_SecOC_00069]）
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(buf[0], 0x11U);                       // Authentic I-PDU は、そのまま
    EXPECT_EQ(buf[1], 0x22U);
    EXPECT_EQ(buf[2], 0x01U);                       // Freshness Value は、進んでいる（1 回目は 0）
    EXPECT_EQ(first[2], 0x00U);
}

TEST_F(SecOC_TriggerTransmit_Test, SecOC_TriggerTransmit_OK_ConfirmationWithoutFetchDoesNotReauthenticate)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Authenticate();   // 直接の送信（トリガ送信を使わない）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    SecOC_TxConfirmation(kTxPduId, E_OK);
    SecOC_MainFunctionTx();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_PduR_SecOCTransmit, 1U);   // トリガ送信で取り出していないので、再び認証して送らない
}

TEST_F(SecOC_TriggerTransmit_Test, SecOC_TriggerTransmit_NG_NewAuthenticPduInvalidatesHeldSecuredPdu)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Authenticate();
    uint8 buf[8];
    for (uint8 i = 0U; i < 8U; i++) { buf[i] = 0xEEU; }
    PduInfoType info = { buf, 8U };
    uint8 newer[2] = { 0x33U, 0x44U };
    PduInfoType newerInfo = { newer, 2U };
    ASSERT_EQ(SecOC_IfTransmit(kTxPduId, &newerInfo), E_OK);   // 新しい Authentic I-PDU を、組み立て直す前に受け取る

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = SecOC_TriggerTransmit(kTxPduId, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);   // 古い Secured I-PDU（古い Authentic I-PDU のもの）を、渡さない
    EXPECT_EQ(buf[0], 0xEEU);
}

TEST_F(SecOC_TriggerTransmit_Test, SecOC_TriggerTransmit_NG_NoSecuredPduYet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 buf[8];
    for (uint8 i = 0U; i < 8U; i++) { buf[i] = 0xEEU; }
    PduInfoType info = { buf, 8U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = SecOC_TriggerTransmit(kTxPduId, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);            // 組み立て済みの Secured I-PDU が無い（[SWS_SecOC_00150]）
    EXPECT_EQ(buf[0], 0xEEU);            // 何もコピーしない
    EXPECT_EQ(info.SduLength, 8U);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);   // 設定どおりの動作なので、DET へは報告しない
}

TEST_F(SecOC_TriggerTransmit_Test, SecOC_TriggerTransmit_NG_AuthenticPduNotYetSecured)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 buf[8];
    for (uint8 i = 0U; i < 8U; i++) { buf[i] = 0xEEU; }
    PduInfoType info = { buf, 8U };
    uint8 authentic[2] = { 0x11U, 0x22U };
    PduInfoType authInfo = { authentic, 2U };
    ASSERT_EQ(SecOC_IfTransmit(kTxPduId, &authInfo), E_OK);   // Authentic I-PDU を渡しただけで、まだ組み立てていない

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = SecOC_TriggerTransmit(kTxPduId, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(buf[0], 0xEEU);
}

TEST_F(SecOC_TriggerTransmit_Test, SecOC_TriggerTransmit_NG_BufferTooSmallCopiesNothing)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Authenticate();
    uint8 buf[8];
    for (uint8 i = 0U; i < 8U; i++) { buf[i] = 0xEEU; }
    PduInfoType info = { buf, 5U };   // Secured I-PDU の長さ（6）より小さい

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = SecOC_TriggerTransmit(kTxPduId, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(buf[0], 0xEEU);          // 何もコピーしない
    EXPECT_EQ(info.SduLength, 5U);     // PduInfoPtr を変えない
}

TEST_F(SecOC_TriggerTransmit_Test, SecOC_TriggerTransmit_NG_InvalidTxPduId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Authenticate();
    uint8 buf[8];
    for (uint8 i = 0U; i < 8U; i++) { buf[i] = 0xEEU; }
    PduInfoType info = { buf, 8U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = SecOC_TriggerTransmit(kTxPduId + 1U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, SECOC_E_INVALID_PDU_SDU_ID);
    EXPECT_EQ(buf[0], 0xEEU);
}

TEST_F(SecOC_TriggerTransmit_Test, SecOC_TriggerTransmit_NG_NullPduInfoPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = SecOC_TriggerTransmit(kTxPduId, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, SECOC_E_PARAM_POINTER);
}

TEST_F(SecOC_TriggerTransmit_Test, SecOC_TriggerTransmit_NG_NullSduDataPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PduInfoType info = { NULL, 8U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = SecOC_TriggerTransmit(kTxPduId, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, SECOC_E_PARAM_POINTER);
}

TEST_F(SecOC_TriggerTransmit_Test, SecOC_TriggerTransmit_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 buf[8];
    for (uint8 i = 0U; i < 8U; i++) { buf[i] = 0xEEU; }
    PduInfoType info = { buf, 8U };
    SecOC_DeInit();   // 未初期化にする
    FakeDetHw_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = SecOC_TriggerTransmit(kTxPduId, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, SECOC_E_UNINIT);
    EXPECT_EQ(buf[0], 0xEEU);
}

TEST_F(SecOC_TriggerTransmit_Test, SecOC_TriggerTransmit_NG_DeInitDiscardsHeldSecuredPdu)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Authenticate();
    uint8 buf[8];
    for (uint8 i = 0U; i < 8U; i++) { buf[i] = 0xEEU; }
    PduInfoType info = { buf, 8U };
    SecOC_DeInit();
    SecOC_Init(&cfg);   // 初期化し直す
    FakeDetHw_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = SecOC_TriggerTransmit(kTxPduId, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);            // 保持していた Secured I-PDU は、捨てられている
    EXPECT_EQ(buf[0], 0xEEU);
}

}  // namespace
