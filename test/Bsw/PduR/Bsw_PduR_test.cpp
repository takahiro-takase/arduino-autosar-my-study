/**
 * \file    Bsw_PduR_test.cpp
 * \brief   PduR.c（src/Bsw/PduR/PduR.c）の単体テスト（GoogleTest / PlatformIO
 *          `[env:native_chain]`）。
 *
 * \details 2026-09、モジュール単位のテストファイルを1モジュール1ファイルへ
 *          集約する方針のもと、`Bsw_PduR_SecOCTxConfirmation_test.cpp` と
 *          旧 `Bsw_PduR_test.cpp`（NG系のみ）の2ファイルを本ファイルへ統合した。
 *          各セクションの経緯は元ファイルのコメントをそのまま引き継ぐ。
 */
#include <gtest/gtest.h>

extern "C" {
#include "PduR.h"
#include "Fake_Det_Hw.h"
}

namespace
{

// ============================================================================
// PduR_SecOCTxConfirmation()（旧 Bsw_PduR_SecOCTxConfirmation_test.cpp）
// ============================================================================

/**
 * \details `PduR_SecOCTxConfirmation()` は `PduR_CanIfTxConfirmation()` と同じ
 *          `PduR_FindTxPath()` 共通処理を再利用する薄い関数のため、`PduR.c` を
 *          実体でリンクしてローカルな `PduR_PBConfigType` で直接検証する
 *          （Com/CanIf/Can 等は関与しないため、それらは未初期化のままでよい）。
 */

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
    PduR_Init(&kSecOCTestConfig);
    FakeDetHw_LogSuppressed = 0U;

    PduR_SecOCTxConfirmation(10U, E_OK);

    EXPECT_EQ(g_ConfFctCallCount, 1U);
    EXPECT_EQ(g_LastDestPduId, 20U);
    EXPECT_EQ(g_LastResult, E_OK);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(Bsw_PduR_SecOCTxConfirmation_Test, OK_ForwardsFailureResultUnchanged)
{
    PduR_Init(&kSecOCTestConfig);
    FakeDetHw_LogSuppressed = 0U;

    PduR_SecOCTxConfirmation(10U, E_NOT_OK);

    EXPECT_EQ(g_ConfFctCallCount, 1U);
    EXPECT_EQ(g_LastResult, E_NOT_OK);
}

TEST_F(Bsw_PduR_SecOCTxConfirmation_Test, NG_NoMatchingRouteReportsDetAndDoesNotCallConfFct)
{
    PduR_Init(&kSecOCTestConfig);
    FakeDetHw_LogSuppressed = 0U;

    PduR_SecOCTxConfirmation(99U, E_OK);  // 99 は kSecOCTestTxPath.SrcPduId と不一致

    EXPECT_EQ(g_ConfFctCallCount, 0U);
    EXPECT_EQ(FakeDetHw_LastErrorId, PDUR_E_PDU_ID_INVALID);
}

// ============================================================================
// その他公開APIの NG系（旧 Bsw_PduR_test.cpp）
// ============================================================================

/**
 * \details 上記セクションが未カバーだった `Det_ReportError()` 呼び出し箇所
 *          （PduR_Init/PduR_GetVersionInfo/PduR_ComTransmit/PduR_SecOCTransmit/
 *          PduR_ComRxIndication/PduR_CanIfTxConfirmation）の NG ケースのみを
 *          まとめる。各ケースで、報告される ErrorId が仕様どおり正しい値に
 *          なっていることを `Fake_Det_Hw.h`（`FakeDetHw_LastErrorId`）で検証する。
 *
 *          PduR には Mcu 同様 DeInit() に相当する API が無く、`PduR_ConfigPtr`
 *          は native_chain_tests バイナリ全体で共有される static のため、
 *          `PDUR_E_UNINIT` の検証には `PduR_Test_ResetInitState()`
 *          （`PDUR_UNIT_TEST` ビルドのみに存在するテスト専用関数、
 *          `Mcu_Test_ResetInitState()` と同じ設計）を使う。これにより
 *          上記セクション冒頭コメントが説明していた「実行順序に依存するため
 *          未初期化ケースは書かない」という制約を受けずに済む。
 */

const PduR_TxRoutingPathType kTestTxPath = {
    /* SrcPduId */             10U,
    /* CanIfTxPduId */         0U,
    /* ConfDestPduId */        20U,
    /* ConfFct */              NULL,
    /* TransmitOverrideFct */  NULL
};

const PduR_RxDestType kTestRxDest = {
    /* Module */    PDUR_MODULE_COM,
    /* DestPduId */ 30U,
    /* RxIndFct */  NULL
};

const PduR_RxRoutingPathType kTestRxPath = {
    /* SrcPduId */  40U,
    /* Dests */     &kTestRxDest,
    /* DestCount */ 1U
};

const PduR_PBConfigType kTestConfig = {
    &kTestRxPath, 1U,
    &kTestTxPath, 1U
};

class Bsw_PduR_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        PduR_Init(&kTestConfig);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    // PduR に DeInit() は無いため TearDown は書かない。未初期化状態が必要な
    // テストは各自 PduR_Test_ResetInitState() を明示的に呼ぶ（ファイル冒頭
    // コメント参照）。
};

// ------------------------------------------------------------
// PduR_Init()
// ------------------------------------------------------------

TEST_F(Bsw_PduR_Test, PduR_Init_NG_NullConfigPtr)
{
    PduR_Test_ResetInitState();

    PduR_Init(NULL);

    EXPECT_EQ(FakeDetHw_LastErrorId, PDUR_E_INIT_FAILED);
}

TEST_F(Bsw_PduR_Test, PduR_Init_NG_RxPathHasNoDests)
{
    PduR_Test_ResetInitState();
    const PduR_RxRoutingPathType badRxPath = { 40U, NULL, 0U };
    const PduR_PBConfigType badConfig = { &badRxPath, 1U, NULL, 0U };

    PduR_Init(&badConfig);

    EXPECT_EQ(FakeDetHw_LastErrorId, PDUR_E_INIT_FAILED);
}

// ------------------------------------------------------------
// PduR_GetVersionInfo()
// ------------------------------------------------------------

TEST_F(Bsw_PduR_Test, PduR_GetVersionInfo_NG_NullPointer)
{
    PduR_GetVersionInfo(NULL);

    EXPECT_EQ(FakeDetHw_LastErrorId, PDUR_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// PduR_ComTransmit()
// ------------------------------------------------------------

TEST_F(Bsw_PduR_Test, PduR_ComTransmit_NG_Uninit)
{
    PduR_Test_ResetInitState();
    uint8 sdu[8] = { 0 };
    PduInfoType info = { sdu, 1U };

    Std_ReturnType ret = PduR_ComTransmit(10U, &info);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, PDUR_E_UNINIT);
}

TEST_F(Bsw_PduR_Test, PduR_ComTransmit_NG_NullPduInfoPtr)
{
    Std_ReturnType ret = PduR_ComTransmit(10U, NULL);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, PDUR_E_PARAM_POINTER);
}

TEST_F(Bsw_PduR_Test, PduR_ComTransmit_NG_NoMatchingRoute)
{
    uint8 sdu[8] = { 0 };
    PduInfoType info = { sdu, 1U };

    Std_ReturnType ret = PduR_ComTransmit(99U, &info);  // 99 は kTestTxPath.SrcPduId と不一致

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, PDUR_E_PDU_ID_INVALID);
}

// ------------------------------------------------------------
// PduR_SecOCTransmit()
// ------------------------------------------------------------

TEST_F(Bsw_PduR_Test, PduR_SecOCTransmit_NG_Uninit)
{
    PduR_Test_ResetInitState();
    uint8 sdu[8] = { 0 };
    PduInfoType info = { sdu, 1U };

    Std_ReturnType ret = PduR_SecOCTransmit(10U, &info);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, PDUR_E_UNINIT);
}

TEST_F(Bsw_PduR_Test, PduR_SecOCTransmit_NG_NullPduInfoPtr)
{
    Std_ReturnType ret = PduR_SecOCTransmit(10U, NULL);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, PDUR_E_PARAM_POINTER);
}

TEST_F(Bsw_PduR_Test, PduR_SecOCTransmit_NG_NoMatchingRoute)
{
    uint8 sdu[8] = { 0 };
    PduInfoType info = { sdu, 1U };

    Std_ReturnType ret = PduR_SecOCTransmit(99U, &info);  // 99 は kTestTxPath.SrcPduId と不一致

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, PDUR_E_PDU_ID_INVALID);
}

// ------------------------------------------------------------
// PduR_ComRxIndication()
// ------------------------------------------------------------

TEST_F(Bsw_PduR_Test, PduR_ComRxIndication_NG_Uninit)
{
    PduR_Test_ResetInitState();
    uint8 sdu[8] = { 0 };
    PduInfoType info = { sdu, 1U };

    PduR_ComRxIndication(40U, &info);

    EXPECT_EQ(FakeDetHw_LastErrorId, PDUR_E_UNINIT);
}

TEST_F(Bsw_PduR_Test, PduR_ComRxIndication_NG_NullPduInfoPtr)
{
    PduR_ComRxIndication(40U, NULL);

    EXPECT_EQ(FakeDetHw_LastErrorId, PDUR_E_PARAM_POINTER);
}

TEST_F(Bsw_PduR_Test, PduR_ComRxIndication_NG_NoMatchingRoute)
{
    uint8 sdu[8] = { 0 };
    PduInfoType info = { sdu, 1U };

    PduR_ComRxIndication(99U, &info);  // 99 は kTestRxPath.SrcPduId と不一致

    EXPECT_EQ(FakeDetHw_LastErrorId, PDUR_E_PDU_ID_INVALID);
}

// ------------------------------------------------------------
// PduR_CanIfTxConfirmation()
// ------------------------------------------------------------

TEST_F(Bsw_PduR_Test, PduR_CanIfTxConfirmation_NG_Uninit)
{
    PduR_Test_ResetInitState();

    PduR_CanIfTxConfirmation(10U, E_OK);

    EXPECT_EQ(FakeDetHw_LastErrorId, PDUR_E_UNINIT);
}

TEST_F(Bsw_PduR_Test, PduR_CanIfTxConfirmation_NG_NoMatchingRoute)
{
    PduR_CanIfTxConfirmation(99U, E_OK);  // 99 は kTestTxPath.SrcPduId と不一致

    EXPECT_EQ(FakeDetHw_LastErrorId, PDUR_E_PDU_ID_INVALID);
}

}  // namespace
