/**
 * \file    Bsw_SecOC_OverrideIndefinite_test.cpp
 * \brief   SecOC_VerifyStatusOverride() の無期限 Fail 強制と解除の単体テスト
 * \details 無期限 Fail 強制中は、実際の検証が成功しても Com へ渡さず、通知関数へ失敗を伝える。ただし実際の検証が成功していれば Freshness の基準は進める。
 *          解除すると、次の正しいフレームから通常どおり受理する。overrideStatus=41（Pass 強制）などの未対応値は拒否する。
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

/* SecOC_VerifyStatusOverride()（無期限 Fail 強制） の単体テスト用フィクスチャ。
 * SetUp(): Crypto・CryIf・Csm を初期化し（実体をリンクしており、MAC の検証が実際に行われる）、本番の RX Secured I-PDU 設定（ImmobilizerCmd）を
 *          基に、通知関数をテスト用に差し替えた設定で SecOC_Init() を呼ぶ。Com の呼び出し記録と DET の記録も初期化する。
 * BuildFrame(): 正しい MAC を付けた Secured I-PDU（Authentic 2 バイト + Freshness 1 バイト + MAC 3 バイト）を作る補助関数。
 * TearDown(): SecOC_DeInit() を呼び、Crypto・CryIf・Csm を未初期化へ戻す。 */
class Bsw_SecOC_OverrideIndefinite_Test : public ::testing::Test
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

TEST_F(Bsw_SecOC_OverrideIndefinite_Test, SecOC_VerifyStatusOverride_OK_IndefiniteFailBlocksUntilCanceled)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 buf[6];
    ASSERT_EQ(SecOC_VerifyStatusOverride(TEST_RX_PDU_ID, SECOC_OVERRIDE_FAIL_INDEFINITE, 0U), E_OK);
    PduInfoType f1 = BuildFrame(1U, buf);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    SecOC_RxIndication(TEST_RX_PDU_ID, &f1);
    const uint32 comWhileForced = CallCount_Com_RxIndication;
    const SecOC_VerificationResultType statusWhileForced = g_calloutLast.verificationStatus;

    ASSERT_EQ(SecOC_VerifyStatusOverride(TEST_RX_PDU_ID, SECOC_OVERRIDE_CANCEL, 0U), E_OK);
    uint8 buf2[6];
    PduInfoType f2 = BuildFrame(2U, buf2);
    SecOC_RxIndication(TEST_RX_PDU_ID, &f2);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(comWhileForced, 0U);
    EXPECT_EQ(statusWhileForced, SECOC_VERIFICATIONFAILURE);
    EXPECT_EQ(CallCount_Com_RxIndication, 1U);   // 解除後は受理（基準は強制中も進んでいる）
    EXPECT_EQ(g_calloutLast.verificationStatus, SECOC_VERIFICATIONSUCCESS);
}

TEST_F(Bsw_SecOC_OverrideIndefinite_Test, SecOC_VerifyStatusOverride_NG_UnsupportedStatusIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType retForcePass = SecOC_VerifyStatusOverride(TEST_RX_PDU_ID, 41U, 0U);   // Pass 強制は非対応
    const Std_ReturnType retUnknown = SecOC_VerifyStatusOverride(TEST_RX_PDU_ID, 99U, 0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(retForcePass, E_NOT_OK);
    EXPECT_EQ(retUnknown, E_NOT_OK);
}

}  // namespace
