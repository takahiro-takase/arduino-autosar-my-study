/**
 * \file    PduR_SecOCTxConfirmation_test.cpp
 * \brief   PduR_SecOCTxConfirmation() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 PduR_test.cpp）。
 *
 *          分割前の共通の説明は、旧 PduR_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "PduR.h"
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

uint32   g_ConfFctCallCount = 0U;

PduIdType g_LastDestPduId   = 0U;

Std_ReturnType g_LastResult = E_OK;

void TestConfFct(PduIdType DestPduId, Std_ReturnType result)
{
    g_ConfFctCallCount++;
    g_LastDestPduId = DestPduId;
    g_LastResult    = result;
}

const PduR_TxRoutingPathType kSecOCTestTxPath = {
    /* SrcPduId */             10U,
    /* CanIfTxPduId */         0U,   // 本テストでは未使用
    /* ConfDestPduId */        20U,
    /* ConfFct */              TestConfFct,
    /* TransmitOverrideFct */  NULL
};

const PduR_PBConfigType kSecOCTestConfig = {
    /* RxPaths */     NULL,
    /* RxPathCount */ 0U,
    /* TxPaths */     &kSecOCTestTxPath,
    /* TxPathCount */ 1U
};

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* PduR_SecOCTxConfirmation() の単体テスト用フィクスチャ。
 * SetUp(): DET の記録と、送信確認コールバック（ConfFct）の呼び出し回数・引数の記録を初期化する。 */
class PduR_SecOCTxConfirmationX_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;
        FakeDetHw_Reset();
        g_ConfFctCallCount = 0U;
        g_LastDestPduId    = 0U;
        g_LastResult       = E_OK;
    }

    /* PduR に DeInit() は存在せず、PduR_ConfigPtr は native_chain バイナリ
     * 全体で共有される static であり一度 Init すると戻せない。そのため
     * 「未初期化」を検証するテストケースはここでは書かない（他のテスト
     * ファイルが先に PduR_Init() を呼んでいる可能性があり、実行順序に
     * よって偽陽性/偽陰性になる。IsolatedComTxFixtureBase 冒頭コメント
     * 参照）。以下のテストは全て自身で PduR_Init() を呼んでから検証する
     * ため、この制約の影響を受けない。 */
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(PduR_SecOCTxConfirmationX_Test, PduR_SecOCTxConfirmation_NG_NoMatchingRouteReportsDetAndDoesNotCallConfFct)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PduR_Init(&kSecOCTestConfig);
    FakeDetHw_LogSuppressed = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    PduR_SecOCTxConfirmation(99U, E_OK);  // 99 は kSecOCTestTxPath.SrcPduId と不一致

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(g_ConfFctCallCount, 0U);
    EXPECT_EQ(FakeDetHw_LastErrorId, PDUR_E_PDU_ID_INVALID);
}

TEST_F(PduR_SecOCTxConfirmationX_Test, PduR_SecOCTxConfirmation_OK_MatchingRouteInvokesConfFctWithConfDestPduId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PduR_Init(&kSecOCTestConfig);
    FakeDetHw_LogSuppressed = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    PduR_SecOCTxConfirmation(10U, E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(g_ConfFctCallCount, 1U);
    EXPECT_EQ(g_LastDestPduId, 20U);
    EXPECT_EQ(g_LastResult, E_OK);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(PduR_SecOCTxConfirmationX_Test, PduR_SecOCTxConfirmation_OK_ForwardsFailureResultUnchanged)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PduR_Init(&kSecOCTestConfig);
    FakeDetHw_LogSuppressed = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    PduR_SecOCTxConfirmation(10U, E_NOT_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(g_ConfFctCallCount, 1U);
    EXPECT_EQ(g_LastResult, E_NOT_OK);
}

}  // namespace
