/**
 * \file    PduR_DcmTransmit_test.cpp
 * \brief   PduR_DcmTransmit() の単体テスト
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

// 上位層（Dcm）の TX コールバックと、下位層（CanTp）の CanTp_Transmit() の偽物。呼ばれた回数と引数を記録する。
uint32             gTransmitCount;
uint32             gCopyCount;
uint32             gConfCount;
PduIdType          gId;
const PduInfoType* gInfo;
const RetryInfoType* gRetry;
PduLengthType*     gAvailPtr;
Std_ReturnType     gResult;
Std_ReturnType     gTransmitReturn;
BufReq_ReturnType  gCopyReturn;
PduLengthType      gAvail;   // 偽物が availableDataPtr へ書く値

Std_ReturnType FakeCanTpTransmit(PduIdType TxSduId, const PduInfoType* PduInfoPtr)
{
    gTransmitCount++;
    gId   = TxSduId;
    gInfo = PduInfoPtr;
    return gTransmitReturn;
}

BufReq_ReturnType FakeCopyTxData(PduIdType id, const PduInfoType* info, const RetryInfoType* retry,
                                 PduLengthType* availableDataPtr)
{
    gCopyCount++;
    gId       = id;
    gInfo     = info;
    gRetry    = retry;
    gAvailPtr = availableDataPtr;
    *availableDataPtr = gAvail;
    return gCopyReturn;
}

void FakeTxConfirmation(PduIdType id, Std_ReturnType result)
{
    gConfCount++;
    gId     = id;
    gResult = result;
}

// 上位層（Dcm）の TX の ID は 33、下位層（CanTp）の TX の N-SDU の ID は 44。
const PduR_TpTxRoutingPathType kTestTpTxPath = {
    /* SrcPduId */          33U,
    /* DestPduId */         44U,
    /* TransmitFct */       FakeCanTpTransmit,
    /* CopyTxDataFct */     FakeCopyTxData,
    /* TxConfirmationFct */ FakeTxConfirmation
};

const PduR_PBConfigType kTestConfig = {
    /* RxPaths */     NULL,
    /* RxPathCount */ 0U,
    /* TxPaths */     NULL,
    /* TxPathCount */ 0U,
    /* PBConfigId */  0U,
    /* RoutingPathGroups */ NULL,
    /* RoutingPathGroupCount */ 0U,
    /* TpRxPaths */     NULL,
    /* TpRxPathCount */ 0U,
    /* TpTxPaths */     &kTestTpTxPath,
    /* TpTxPathCount */ 1U
};

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* PduR_DcmTransmit() の単体テスト用フィクスチャ。
 * SetUp(): 偽物の記録を消し、PduR_Init(&kTestConfig) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。 */
class PduR_DcmTransmit_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        gTransmitCount = 0U;
        gCopyCount     = 0U;
        gConfCount     = 0U;
        gId       = 0xFFFFU;
        gInfo     = NULL;
        gRetry    = NULL;
        gAvailPtr = NULL;
        gResult   = 0xFFU;
        gTransmitReturn = E_OK;
        gCopyReturn     = BUFREQ_OK;
        gAvail          = 0U;
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

TEST_F(PduR_DcmTransmit_Test, PduR_DcmTransmit_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PduR_Test_ResetInitState();
    PduInfoType info = { NULL, 6U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = PduR_DcmTransmit(33U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, PDUR_E_UNINIT);
    EXPECT_EQ(gTransmitCount, 0U);
}

TEST_F(PduR_DcmTransmit_Test, PduR_DcmTransmit_NG_NullInfo)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = PduR_DcmTransmit(33U, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, PDUR_E_PARAM_POINTER);
    EXPECT_EQ(gTransmitCount, 0U);
}

TEST_F(PduR_DcmTransmit_Test, PduR_DcmTransmit_NG_NoMatchingRoute)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PduInfoType info = { NULL, 6U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = PduR_DcmTransmit(44U, &info);   // 44 は下位層側の ID。上位層側（SrcPduId=33）の検索には当たらない

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, PDUR_E_PDU_ID_INVALID);
    EXPECT_EQ(gTransmitCount, 0U);
}

TEST_F(PduR_DcmTransmit_Test, PduR_DcmTransmit_OK_ForwardsToCanTpWithTranslatedId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PduInfoType info = { NULL, 6U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = PduR_DcmTransmit(33U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(gTransmitCount, 1U);
    EXPECT_EQ(gId, 44U);        // 上位層の ID（33）から、CanTp の N-SDU の ID（44）へ変換する
    EXPECT_EQ(gInfo, &info);    // info は、そのまま渡す
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(PduR_DcmTransmit_Test, PduR_DcmTransmit_OK_ReturnsCanTpReturnValue)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PduInfoType info = { NULL, 6U };
    gTransmitReturn = E_NOT_OK;   // CanTp が受け付けない

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = PduR_DcmTransmit(33U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(gTransmitCount, 1U);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);   // 下位層の拒否は、DET に報告しない
}


}  // namespace
