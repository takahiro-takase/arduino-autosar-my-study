/**
 * \file    Bsw_PduR_SecOCTxConfirmation_test.cpp
 * \brief   PduR_SecOCTxConfirmation()の単体テスト（GoogleTest / PlatformIO `[env:native_chain]`）。
 *
 * \details `PduR_SecOCTxConfirmation()` は `PduR_CanIfTxConfirmation()` と同じ
 *          `PduR_FindTxPath()` 共通処理を再利用する薄い関数のため、`PduR.c` を
 *          実体でリンクしてローカルな `PduR_PBConfigType` で直接検証する
 *          （Com/CanIf/Can 等は関与しないため、それらは未初期化のままでよい）。
 */
#include <gtest/gtest.h>

extern "C" {
#include "PduR.h"
#include "Hal_Det_Hw_fake.h"
}

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

const PduR_TxRoutingPathType kTestTxPath = {
    /* SrcPduId */             10U,
    /* CanIfTxPduId */         0U,   // 本テストでは未使用
    /* ConfDestPduId */        20U,
    /* ConfFct */              TestConfFct,
    /* TransmitOverrideFct */  NULL
};

const PduR_PBConfigType kTestConfig = {
    /* RxPaths */     NULL,
    /* RxPathCount */ 0U,
    /* TxPaths */     &kTestTxPath,
    /* TxPathCount */ 1U
};

class Bsw_PduR_SecOCTxConfirmation_Test : public ::testing::Test
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

TEST_F(Bsw_PduR_SecOCTxConfirmation_Test, OK_MatchingRouteInvokesConfFctWithConfDestPduId)
{
    PduR_Init(&kTestConfig);
    FakeDetHw_LogSuppressed = 0U;

    PduR_SecOCTxConfirmation(10U, E_OK);

    EXPECT_EQ(g_ConfFctCallCount, 1U);
    EXPECT_EQ(g_LastDestPduId, 20U);
    EXPECT_EQ(g_LastResult, E_OK);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(Bsw_PduR_SecOCTxConfirmation_Test, OK_ForwardsFailureResultUnchanged)
{
    PduR_Init(&kTestConfig);
    FakeDetHw_LogSuppressed = 0U;

    PduR_SecOCTxConfirmation(10U, E_NOT_OK);

    EXPECT_EQ(g_ConfFctCallCount, 1U);
    EXPECT_EQ(g_LastResult, E_NOT_OK);
}

TEST_F(Bsw_PduR_SecOCTxConfirmation_Test, NG_NoMatchingRouteReportsDetAndDoesNotCallConfFct)
{
    PduR_Init(&kTestConfig);
    FakeDetHw_LogSuppressed = 0U;

    PduR_SecOCTxConfirmation(99U, E_OK);  // 99 は kTestTxPath.SrcPduId と不一致

    EXPECT_EQ(g_ConfFctCallCount, 0U);
    EXPECT_EQ(FakeDetHw_LastErrorId, PDUR_E_PDU_ID_INVALID);
}

}  // namespace
