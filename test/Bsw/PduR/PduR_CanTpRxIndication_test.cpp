/**
 * \file    PduR_CanTpRxIndication_test.cpp
 * \brief   PduR_CanTpRxIndication() の単体テスト
 * \details 2026-11、CanTp ↔ Dcm の TP の経路を、仕様どおり PduR 経由にした（PduR の TP の API を追加）。
 *          上位層（Dcm）と下位層（CanTp）は、呼ばれた引数を記録するだけの偽物（テスト専用の関数）で置き換え、
 *          PduR の TP のルーティングパス（ID の変換）と、DET の報告を確かめる。
 *          ID は、下位層側・上位層側で、わざと別の値にしてある（取り違えを検出するため）。
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

// 上位層（Dcm）の RX コールバックの偽物。呼ばれた回数と引数を記録する。
uint32            gStartCount;
uint32            gCopyCount;
uint32            gIndCount;
PduIdType         gUpperId;
const PduInfoType* gUpperInfo;
PduLengthType     gUpperTpSduLength;
PduLengthType*    gUpperBufferSizePtr;
Std_ReturnType    gUpperResult;
BufReq_ReturnType gUpperReturn;
PduLengthType     gUpperBufferSize;   // 偽物が bufferSizePtr へ書く値

BufReq_ReturnType FakeStartOfReception(PduIdType id, const PduInfoType* info, PduLengthType TpSduLength,
                                       PduLengthType* bufferSizePtr)
{
    gStartCount++;
    gUpperId            = id;
    gUpperInfo          = info;
    gUpperTpSduLength   = TpSduLength;
    gUpperBufferSizePtr = bufferSizePtr;
    *bufferSizePtr      = gUpperBufferSize;
    return gUpperReturn;
}

BufReq_ReturnType FakeCopyRxData(PduIdType id, const PduInfoType* info, PduLengthType* bufferSizePtr)
{
    gCopyCount++;
    gUpperId            = id;
    gUpperInfo          = info;
    gUpperBufferSizePtr = bufferSizePtr;
    *bufferSizePtr      = gUpperBufferSize;
    return gUpperReturn;
}

void FakeRxIndication(PduIdType id, Std_ReturnType result)
{
    gIndCount++;
    gUpperId     = id;
    gUpperResult = result;
}

// 下位層（CanTp）が渡す RX の ID は 11、上位層（Dcm）の RX の ID は 22。
const PduR_TpRxRoutingPathType kTestTpRxPath = {
    /* SrcPduId */            11U,
    /* DestPduId */           22U,
    /* StartOfReceptionFct */ FakeStartOfReception,
    /* CopyRxDataFct */       FakeCopyRxData,
    /* RxIndicationFct */     FakeRxIndication
};

const PduR_PBConfigType kTestConfig = {
    /* RxPaths */     NULL,
    /* RxPathCount */ 0U,
    /* TxPaths */     NULL,
    /* TxPathCount */ 0U,
    /* PBConfigId */  0U,
    /* RoutingPathGroups */ NULL,
    /* RoutingPathGroupCount */ 0U,
    /* TpRxPaths */     &kTestTpRxPath,
    /* TpRxPathCount */ 1U,
    /* TpTxPaths */     NULL,
    /* TpTxPathCount */ 0U
};

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* PduR_CanTpRxIndication() の単体テスト用フィクスチャ。
 * SetUp(): 偽物の記録を消し、PduR_Init(&kTestConfig) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。 */
class PduR_CanTpRxIndication_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        gStartCount = 0U;
        gCopyCount  = 0U;
        gIndCount   = 0U;
        gUpperId    = 0xFFFFU;
        gUpperInfo  = NULL;
        gUpperTpSduLength   = 0U;
        gUpperBufferSizePtr = NULL;
        gUpperResult        = 0xFFU;
        gUpperReturn        = BUFREQ_OK;
        gUpperBufferSize    = 0U;
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        PduR_Init(&kTestConfig);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    // PduR に DeInit() は無いため TearDown は書かない。未初期化状態が必要なテストは各自 PduR_Test_ResetInitState() を呼ぶ。
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(PduR_CanTpRxIndication_Test, PduR_CanTpRxIndication_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PduR_Test_ResetInitState();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    PduR_CanTpRxIndication(11U, E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, PDUR_E_UNINIT);
    EXPECT_EQ(gIndCount, 0U);
}

TEST_F(PduR_CanTpRxIndication_Test, PduR_CanTpRxIndication_NG_NoMatchingRoute)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    PduR_CanTpRxIndication(22U, E_OK);   // 22 は上位層側の ID

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, PDUR_E_PDU_ID_INVALID);
    EXPECT_EQ(gIndCount, 0U);
}

TEST_F(PduR_CanTpRxIndication_Test, PduR_CanTpRxIndication_OK_ForwardsOkToUpperLayerWithTranslatedId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    PduR_CanTpRxIndication(11U, E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(gIndCount, 1U);
    EXPECT_EQ(gUpperId, 22U);      // CanTp の ID（11）から、Dcm の ID（22）へ変換する
    EXPECT_EQ(gUpperResult, E_OK);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(PduR_CanTpRxIndication_Test, PduR_CanTpRxIndication_OK_ForwardsNotOkToUpperLayer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    PduR_CanTpRxIndication(11U, E_NOT_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(gIndCount, 1U);
    EXPECT_EQ(gUpperResult, E_NOT_OK);   // 受信の中断も、そのまま伝える
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}


}  // namespace
