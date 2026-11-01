/**
 * \file    PduR_CanIfTriggerTransmit_test.cpp
 * \brief   PduR_CanIfTriggerTransmit() の単体テスト
 * \details 下位層（CanIf）の送信データの要求を、上位層の TriggerTransmit へ転送する経路
 *          （[SWS_PduR_00430]/[SWS_PduR_00661]）と、無効なルーティングパスグループの扱い
 *          （[SWS_PDUR_00805]）を確認する。
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

// 上位層の TriggerTransmit（偽物）の記録。
uint32         gUpperCallCount;
PduIdType      gUpperPduId;
PduInfoType*   gUpperPduInfo;
Std_ReturnType gUpperReturn;

Std_ReturnType FakeUpperTriggerTransmit(PduIdType DestPduId, PduInfoType* PduInfoPtr)
{
    gUpperCallCount++;
    gUpperPduId   = DestPduId;
    gUpperPduInfo = PduInfoPtr;
    return gUpperReturn;
}

// SecOC を経由する経路の、TransmitOverrideFct（偽物。トリガ送信の確認では呼ばれない）。
Std_ReturnType FakeOverrideTransmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)
{
    (void)TxPduId;
    (void)PduInfoPtr;
    return E_OK;
}

// TX パス: SrcPduId 10（トリガ送信に対応。上位層の PDU ID は 20）、SrcPduId 11（対応しない）。
PduR_TxRoutingPathType gTxPaths[2];
const PduR_RxDestType kRxDest = { PDUR_MODULE_COM, 30U, NULL };
const PduR_RxRoutingPathType kRxPath = { 40U, &kRxDest, 1U };

// グループ 0: TX パス 10 が所属する。
const PduIdType kGroupTxMembers[] = { 10U };
PduR_RoutingPathGroupType gGroup;
PduR_PBConfigType gConfig;

void InitPduR(boolean groupEnabledAtInit, boolean useGroup)
{
    gTxPaths[0] = {};
    gTxPaths[0].SrcPduId           = 10U;
    gTxPaths[0].CanIfTxPduId       = 0U;
    gTxPaths[0].ConfDestPduId      = 20U;
    gTxPaths[0].TriggerTransmitFct = FakeUpperTriggerTransmit;
    gTxPaths[1] = {};
    gTxPaths[1].SrcPduId           = 11U;
    gTxPaths[1].CanIfTxPduId       = 1U;
    gTxPaths[1].ConfDestPduId      = 21U;
    gTxPaths[1].TriggerTransmitFct = NULL;

    gGroup = { groupEnabledAtInit, NULL, 0U, kGroupTxMembers, 1U };

    gConfig = {};
    gConfig.RxPaths     = &kRxPath;
    gConfig.RxPathCount = 1U;
    gConfig.TxPaths     = gTxPaths;
    gConfig.TxPathCount = 2U;
    if (useGroup != FALSE)
    {
        gConfig.RoutingPathGroups     = &gGroup;
        gConfig.RoutingPathGroupCount = 1U;
    }

    FakeDetHw_LogSuppressed = 1U;
    PduR_Test_ResetInitState();
    PduR_Init(&gConfig);
    FakeDetHw_Reset();
    FakeDetHw_LogSuppressed = 0U;
}

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* PduR_CanIfTriggerTransmit() の単体テスト用フィクスチャ。
 * SetUp(): 上位層の記録を消し、グループなしの設定で PduR_Init() を呼ぶ。 */
class PduR_CanIfTriggerTransmit_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        gUpperCallCount = 0U;
        gUpperPduId     = 0xFFFFU;
        gUpperPduInfo   = NULL;
        gUpperReturn    = E_OK;
        InitPduR(TRUE, FALSE);
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(PduR_CanIfTriggerTransmit_Test, PduR_CanIfTriggerTransmit_OK_ForwardsToUpperLayerWithConfDestPduId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 sdu[8] = { 0 };
    PduInfoType info = { sdu, 8U };
    gUpperReturn = E_OK;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = PduR_CanIfTriggerTransmit(10U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(gUpperCallCount, 1U);
    EXPECT_EQ(gUpperPduId, 20U);        // パスの ConfDestPduId（上位層の PDU ID）へ読み替える
    EXPECT_EQ(gUpperPduInfo, &info);    // PduInfoPtr は、そのまま渡す
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(PduR_CanIfTriggerTransmit_Test, PduR_CanIfTriggerTransmit_OK_PathViaSecOCPassesOverrideId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    gTxPaths[0].TransmitOverrideFct = FakeOverrideTransmit;   // SecOC を経由する経路にする
    gTxPaths[0].TransmitOverrideId  = 77U;                    // SecOC の TX PDU の ID（ConfDestPduId=20 とは別）
    uint8 sdu[8] = { 0 };
    PduInfoType info = { sdu, 8U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = PduR_CanIfTriggerTransmit(10U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(gUpperCallCount, 1U);
    EXPECT_EQ(gUpperPduId, 77U);   // SecOC の TX PDU の ID を渡す（SecOC が Secured I-PDU を保持する。[SWS_SecOC_00068]）
}

TEST_F(PduR_CanIfTriggerTransmit_Test, PduR_CanIfTriggerTransmit_OK_ReturnsUpperLayerReturnValue)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 sdu[8] = { 0 };
    PduInfoType info = { sdu, 8U };
    gUpperReturn = E_NOT_OK;   // 上位層が、データを渡せない

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = PduR_CanIfTriggerTransmit(10U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);           // [SWS_PduR_00661]: 上位層の戻り値を、そのまま返す
    EXPECT_EQ(gUpperCallCount, 1U);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(PduR_CanIfTriggerTransmit_Test, PduR_CanIfTriggerTransmit_NG_Uninitialized)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 sdu[8] = { 0 };
    PduInfoType info = { sdu, 8U };
    PduR_Test_ResetInitState();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = PduR_CanIfTriggerTransmit(10U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, PDUR_E_UNINIT);
    EXPECT_EQ(gUpperCallCount, 0U);
}

TEST_F(PduR_CanIfTriggerTransmit_Test, PduR_CanIfTriggerTransmit_NG_NullPduInfoPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = PduR_CanIfTriggerTransmit(10U, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, PDUR_E_PARAM_POINTER);
    EXPECT_EQ(gUpperCallCount, 0U);
}

TEST_F(PduR_CanIfTriggerTransmit_Test, PduR_CanIfTriggerTransmit_NG_NullSduDataPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PduInfoType info = { NULL, 8U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = PduR_CanIfTriggerTransmit(10U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, PDUR_E_PARAM_POINTER);
    EXPECT_EQ(gUpperCallCount, 0U);
}

TEST_F(PduR_CanIfTriggerTransmit_Test, PduR_CanIfTriggerTransmit_NG_NoMatchingRoute)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 sdu[8] = { 0 };
    PduInfoType info = { sdu, 8U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = PduR_CanIfTriggerTransmit(99U, &info);   // 登録されていない

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, PDUR_E_PDU_ID_INVALID);
    EXPECT_EQ(gUpperCallCount, 0U);
}

TEST_F(PduR_CanIfTriggerTransmit_Test, PduR_CanIfTriggerTransmit_NG_PathWithoutTriggerTransmitSupport)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 sdu[8] = { 0 };
    PduInfoType info = { sdu, 8U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = PduR_CanIfTriggerTransmit(11U, &info);   // TriggerTransmitFct が NULL のパス

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(gUpperCallCount, 0U);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);   // 設定どおりの動作なので、DET へは報告しない
}

TEST_F(PduR_CanIfTriggerTransmit_Test, PduR_CanIfTriggerTransmit_NG_RoutingPathGroupDisabled)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 sdu[8] = { 0 };
    PduInfoType info = { sdu, 8U };
    InitPduR(TRUE, TRUE);
    PduR_DisableRouting(0U, FALSE);   // パス 10 の所属するグループを無効にする
    FakeDetHw_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = PduR_CanIfTriggerTransmit(10U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);               // [SWS_PDUR_00805]
    EXPECT_EQ(gUpperCallCount, 0U);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);   // DET へは報告しない
}

TEST_F(PduR_CanIfTriggerTransmit_Test, PduR_CanIfTriggerTransmit_OK_RoutingPathGroupEnabledAgain)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 sdu[8] = { 0 };
    PduInfoType info = { sdu, 8U };
    InitPduR(FALSE, TRUE);   // 起動時は無効
    PduR_EnableRouting(0U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = PduR_CanIfTriggerTransmit(10U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(gUpperCallCount, 1U);
}

}  // namespace
