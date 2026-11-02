/**
 * \file    PduR_CanTpCopyTxData_test.cpp
 * \brief   PduR_CanTpCopyTxData() の単体テスト
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

/* PduR_CanTpCopyTxData() の単体テスト用フィクスチャ。
 * SetUp(): 偽物の記録を消し、PduR_Init(&kTestConfig) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。 */
class PduR_CanTpCopyTxData_Test : public ::testing::Test
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

TEST_F(PduR_CanTpCopyTxData_Test, PduR_CanTpCopyTxData_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PduR_Test_ResetInitState();
    uint8 buf[4] = { 0 };
    PduInfoType info = { buf, 4U };
    PduLengthType avail = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const BufReq_ReturnType ret = PduR_CanTpCopyTxData(44U, &info, NULL, &avail);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, BUFREQ_E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, PDUR_E_UNINIT);
    EXPECT_EQ(gCopyCount, 0U);
}

TEST_F(PduR_CanTpCopyTxData_Test, PduR_CanTpCopyTxData_NG_NullInfo)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PduLengthType avail = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const BufReq_ReturnType ret = PduR_CanTpCopyTxData(44U, NULL, NULL, &avail);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, BUFREQ_E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, PDUR_E_PARAM_POINTER);
    EXPECT_EQ(gCopyCount, 0U);
}

TEST_F(PduR_CanTpCopyTxData_Test, PduR_CanTpCopyTxData_NG_NullAvailableDataPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 buf[4] = { 0 };
    PduInfoType info = { buf, 4U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const BufReq_ReturnType ret = PduR_CanTpCopyTxData(44U, &info, NULL, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, BUFREQ_E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, PDUR_E_PARAM_POINTER);
    EXPECT_EQ(gCopyCount, 0U);
}

TEST_F(PduR_CanTpCopyTxData_Test, PduR_CanTpCopyTxData_NG_NoMatchingRoute)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 buf[4] = { 0 };
    PduInfoType info = { buf, 4U };
    PduLengthType avail = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const BufReq_ReturnType ret = PduR_CanTpCopyTxData(33U, &info, NULL, &avail);   // 33 は上位層側の ID。下位層側（DestPduId=44）の検索には当たらない

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, BUFREQ_E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, PDUR_E_PDU_ID_INVALID);
    EXPECT_EQ(gCopyCount, 0U);
}

TEST_F(PduR_CanTpCopyTxData_Test, PduR_CanTpCopyTxData_OK_ForwardsToUpperLayerWithTranslatedId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 buf[4] = { 0 };
    PduInfoType info = { buf, 4U };
    RetryInfoType retry = { TP_DATARETRY, 2U };
    PduLengthType avail = 0U;
    gAvail = 14U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const BufReq_ReturnType ret = PduR_CanTpCopyTxData(44U, &info, &retry, &avail);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, BUFREQ_OK);
    EXPECT_EQ(gCopyCount, 1U);
    EXPECT_EQ(gId, 33U);          // CanTp の N-SDU の ID（44）から、Dcm の ID（33）へ変換する
    EXPECT_EQ(gInfo, &info);      // info・retry・availableDataPtr は、そのまま渡す
    EXPECT_EQ(gRetry, &retry);
    EXPECT_EQ(gAvailPtr, &avail);
    EXPECT_EQ(avail, 14U);        // 上位層が書いた残りのデータ量が、呼び出し元へ届く
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(PduR_CanTpCopyTxData_Test, PduR_CanTpCopyTxData_OK_NullRetryIsPassedAsIs)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 buf[4] = { 0 };
    PduInfoType info = { buf, 4U };
    PduLengthType avail = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const BufReq_ReturnType ret = PduR_CanTpCopyTxData(44U, &info, NULL, &avail);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, BUFREQ_OK);
    EXPECT_TRUE(gRetry == NULL);   // retry は NULL でよい（コピーしたデータをすぐに捨ててよい）。そのまま渡す
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(PduR_CanTpCopyTxData_Test, PduR_CanTpCopyTxData_OK_ReturnsUpperLayerReturnValue)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 buf[4] = { 0 };
    PduInfoType info = { buf, 4U };
    PduLengthType avail = 0U;
    gCopyReturn = BUFREQ_E_BUSY;   // 上位層に、まだ必要な量のデータがない

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const BufReq_ReturnType ret = PduR_CanTpCopyTxData(44U, &info, NULL, &avail);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, BUFREQ_E_BUSY);   // 下位層が再試行できるよう、BUSY を握りつぶさずに返す
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}


}  // namespace
