/**
 * \file    Bsw_SecOC_RxFreshness_test.cpp
 * \brief   SecOC_RxIndication() の Freshness（リプレイ検知）の単体テスト
 * \details 初回は基準が無いので無条件に受理し、以降は「前回より新しい」（差 1〜127、8bit の折り返しを考慮）ときだけ受理して基準を進める。
 *          同じ値（リプレイ）や古い値は MAC が正しくても拒否し、通知関数へ FRESHNESSFAILURE を伝える。
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
#include "Csm_Cfg.h"
#include "CryIf.h"
#include "Crypto.h"
#include "Crypto_Cfg.h"
#include "Crypto_Cmac.h"
#include "Crypto_PBCfg.h"
#include "Com_Cfg.h"
#include "Fake_Det_Hw.h"
#include "Wrap_Com.h"
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

/* テスト用の VerificationStatusCallout が記録する値。 */
uint32                       g_calloutCount;
SecOC_VerificationStatusType g_calloutLast;

void TestCallout(SecOC_VerificationStatusType status)
{
    g_calloutCount++;
    g_calloutLast = status;
}

constexpr PduIdType TEST_RX_PDU_ID = SecOCConf_SecOCRxSecuredPdu_ImmobilizerCmd_Rx;


/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* SecOC_RxIndication()（Freshness 検証） の単体テスト用フィクスチャ。
 * SetUp(): Crypto・CryIf・Csm を初期化し（実体をリンクしており、MAC の検証が実際に行われる）、本番の RX Secured I-PDU 設定（ImmobilizerCmd）を
 *          基に、通知関数をテスト用に差し替えた設定で SecOC_Init() を呼ぶ。Com の呼び出し記録と DET の記録も初期化する。
 * BuildFrame(): 正しい MAC を付けた Secured I-PDU（Authentic 2 バイト + Freshness 1 バイト + MAC 3 バイト）を作る補助関数。
 * TearDown(): SecOC_DeInit() を呼び、Crypto・CryIf・Csm を未初期化へ戻す。 */
class Bsw_SecOC_RxFreshness_Test : public ::testing::Test
{
protected:
    SecOC_RxPduConfigType rxCfg;
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

        rxCfg = SecOC_Config.RxPdus[0];
        rxCfg.VerificationStatusCallout         = TestCallout;
        rxCfg.VerificationStatusPropagationMode = SECOC_VERIFICATION_STATUS_PROPAGATION_BOTH;
        cfg.RxPdus     = &rxCfg;
        cfg.RxPduCount = 1U;
        cfg.TxPdus     = NULL;
        cfg.TxPduCount = 0U;
        SecOC_Init(&cfg);

        WrapCom_Reset();
        FakeDetHw_Reset();
        g_calloutCount = 0U;
        g_calloutLast.freshnessValueID  = 0xFFFFU;
        g_calloutLast.verificationStatus = SECOC_AUTHENTICATIONBUILDFAILURE;
        g_calloutLast.secOCDataId       = 0xFFFFU;
        FakeDetHw_LogSuppressed = 0U;
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        SecOC_DeInit();
        Crypto_Test_ResetInitState();
        CryIf_Test_ResetInitState();
        Csm_Test_ResetInitState();
    }

    /* Authentic{0x11,0x22} + Freshness + MAC(上位 3 バイト) の 6 バイトを作る。 */
    PduInfoType BuildFrame(uint8 freshness, uint8* buf)
    {
        buf[0] = 0x11U;
        buf[1] = 0x22U;
        buf[2] = freshness;
        uint8 input[5];
        input[0] = static_cast<uint8>(rxCfg.DataId >> 8);
        input[1] = static_cast<uint8>(rxCfg.DataId & 0xFFU);
        input[2] = buf[0];
        input[3] = buf[1];
        input[4] = freshness;
        uint8 mac[CRYPTO_CMAC_SIZE];
        Crypto_Cmac_Calculate(Crypto_KeyTable[CRYPTO_KEY_IMMOBILIZER_CMD], input, 5U, mac);
        buf[3] = mac[0];
        buf[4] = mac[1];
        buf[5] = mac[2];
        PduInfoType info = { buf, 6U };
        return info;
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Bsw_SecOC_RxFreshness_Test, SecOC_RxIndication_OK_AcceptsNewerFreshnessAcrossWraparound)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 buf[6];
    PduInfoType f254 = BuildFrame(254U, buf);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    SecOC_RxIndication(TEST_RX_PDU_ID, &f254);   // 初回: 基準なしで受理
    uint8 buf2[6];
    PduInfoType f255 = BuildFrame(255U, buf2);
    SecOC_RxIndication(TEST_RX_PDU_ID, &f255);
    uint8 buf3[6];
    PduInfoType f2 = BuildFrame(2U, buf3);   // 255 → 2 は差 3: 折り返しても新しい
    SecOC_RxIndication(TEST_RX_PDU_ID, &f2);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Com_RxIndication, 3U);
    EXPECT_EQ(g_calloutLast.verificationStatus, SECOC_VERIFICATIONSUCCESS);
}

TEST_F(Bsw_SecOC_RxFreshness_Test, SecOC_RxIndication_NG_ReplayOfSameFreshnessIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 buf[6];
    PduInfoType info = BuildFrame(7U, buf);
    SecOC_RxIndication(TEST_RX_PDU_ID, &info);   // 1 回目は受理
    WrapCom_Reset();
    g_calloutCount = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    SecOC_RxIndication(TEST_RX_PDU_ID, &info);   // 同じフレームの再送（リプレイ）

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Com_RxIndication, 0U);
    EXPECT_EQ(g_calloutCount, 1U);
    EXPECT_EQ(g_calloutLast.verificationStatus, SECOC_FRESHNESSFAILURE);
}

TEST_F(Bsw_SecOC_RxFreshness_Test, SecOC_RxIndication_NG_StaleFreshnessIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 buf[6];
    PduInfoType newer = BuildFrame(100U, buf);
    SecOC_RxIndication(TEST_RX_PDU_ID, &newer);
    WrapCom_Reset();
    uint8 buf2[6];
    PduInfoType older = BuildFrame(50U, buf2);   // 差が 128 以上（古い方向）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    SecOC_RxIndication(TEST_RX_PDU_ID, &older);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Com_RxIndication, 0U);
    EXPECT_EQ(g_calloutLast.verificationStatus, SECOC_FRESHNESSFAILURE);
}

}  // namespace
