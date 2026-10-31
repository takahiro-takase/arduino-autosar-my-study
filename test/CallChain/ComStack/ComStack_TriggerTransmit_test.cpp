/**
 * \file    ComStack_TriggerTransmit_test.cpp
 * \brief   トリガ送信（下位層が送信データを取りに来る経路）のコールチェーンの単体テスト
 *          （GoogleTest / CMake native_chain_tests）。
 *
 * \details CanIf_TriggerTransmit() → PduR_CanIfTriggerTransmit() → Com_TriggerTransmit() の順に、
 *          実体のモジュールを結んで、Com の TX バッファの内容が呼び出し元のバッファへ届くことを確認する。
 *          PDU ID は、層ごとに翻訳される（CanIf の TxPduId 0 → PduR の SrcPduId 5 → Com の IPduId 2）。
 *          Com_MainFunctionTx() が PduR_ComTransmit() へ送る通常の経路（ComStack_Signal_Tx_test.cpp）とは
 *          逆向きで、Can_Write() は呼ばれない。
 *
 *          本番の設定は、どの層もトリガ送信に対応しない（CanIf_TxPduConfigType.TriggerTransmitFct、
 *          PduR_TxRoutingPathType.TriggerTransmitFct とも NULL）ので、この経路は本テスト専用の設定で結ぶ。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Com.h"
#include "PduR.h"
#include "CanIf.h"
#include "CanSM.h"
#include "Can.h"
#include "Fake_Det_Hw.h"
#include "Fake_Millis.h"
#include "Wrap_Can.h"
#include "Wrap_CanIf.h"
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

const PduIdType kCanIfTxPduId = 0U;   // CanIf の TX PDU
const PduIdType kPduRSrcPduId = 5U;   // PduR の経路の SrcPduId（CanIf の UpperLayerTxPduId）
const uint8     kComIPduId    = 2U;   // Com の TX I-PDU（PduR の ConfDestPduId）

// Com: TX I-PDU（IPduId 2、DLC 2）と、16 ビットの TX シグナル（SignalId 0）。
Com_IPduConfigType   gComIPdu;
Com_SignalConfigType gComSignal;
Com_ConfigType       gComConfig;

// PduR: SrcPduId 5 → 上位層 Com_TriggerTransmit（Com の IPduId 2）。
PduR_TxRoutingPathType gPduRTxPath;
PduR_PBConfigType      gPduRConfig;

// CanIf: TX PDU 0（上位層の PDU ID 5）が、PduR_CanIfTriggerTransmit へ転送する。
CanIf_TxPduConfigType gCanIfTxPdu;
CanIf_RxPduConfigType gCanIfRxPdu;
CanIf_ConfigType      gCanIfConfig;

void InitStack()
{
    gComIPdu = {};
    gComIPdu.IPduId            = kComIPduId;
    gComIPdu.DLC               = 2U;
    gComIPdu.PduRId            = kPduRSrcPduId;
    gComIPdu.TxModeMode        = COM_TX_MODE_DIRECT;
    gComIPdu.TxModeModeTrue    = COM_TX_MODE_DIRECT;
    gComIPdu.UpdateBitPosition = 0xFFFFU;
    gComIPdu.IpduGroupId       = COM_IPDU_GROUP_NONE;

    gComSignal = {};
    gComSignal.SignalId         = 0U;
    gComSignal.Direction        = COM_SIGNAL_DIRECTION_TX;
    gComSignal.IPduId           = kComIPduId;
    gComSignal.BitPosition      = 0U;
    gComSignal.BitSize          = 16U;
    gComSignal.Endian           = COM_BIG_ENDIAN;
    gComSignal.FilterAlgorithm  = COM_FILTER_ALWAYS;
    gComSignal.TransferProperty = COM_TRANSFER_PROPERTY_PENDING;
    gComSignal.RxDataTimeoutAction = COM_RX_TIMEOUT_ACTION_NONE;
    gComSignal.DataInvalidAction   = COM_DATA_INVALID_ACTION_NONE;

    gComConfig = {};
    gComConfig.TxIPdus     = &gComIPdu;
    gComConfig.TxIPduCount = 1U;
    gComConfig.Signals     = &gComSignal;
    gComConfig.SignalCount = 1U;

    gPduRTxPath = {};
    gPduRTxPath.SrcPduId           = kPduRSrcPduId;
    gPduRTxPath.CanIfTxPduId       = kCanIfTxPduId;
    gPduRTxPath.ConfDestPduId      = kComIPduId;
    gPduRTxPath.TriggerTransmitFct = Com_TriggerTransmit;

    gPduRConfig = {};
    gPduRConfig.TxPaths     = &gPduRTxPath;
    gPduRConfig.TxPathCount = 1U;

    gCanIfTxPdu = {};
    gCanIfTxPdu.UpperLayerTxPduId   = kPduRSrcPduId;
    gCanIfTxPdu.CanId               = 0x100U;
    gCanIfTxPdu.Dlc                 = 2U;
    gCanIfTxPdu.Hth                 = 0U;
    gCanIfTxPdu.TriggerTransmitFct  = PduR_CanIfTriggerTransmit;
    gCanIfRxPdu = {};
    gCanIfRxPdu.CanId = 0x200U;
    gCanIfRxPdu.Dlc   = 1U;

    gCanIfConfig = {};
    gCanIfConfig.TxPduConfig = &gCanIfTxPdu;
    gCanIfConfig.TxPduCount  = 1U;
    gCanIfConfig.RxPduConfig = &gCanIfRxPdu;
    gCanIfConfig.RxPduCount  = 1U;

    FakeDetHw_LogSuppressed = 1U;
    FakeMillis_Reset();   // Com_Init() が millis() を取り込むため、Init より前にリセットする
    WrapCan_Reset();
    WrapCanIf_Reset();
    WrapPduR_Reset();
    CanSM_Init(NULL);
    CanIf_Init(&gCanIfConfig);
    PduR_Test_ResetInitState();
    PduR_Init(&gPduRConfig);
    Com_DeInit();
    Com_Init(&gComConfig);
    FakeDetHw_Reset();
    FakeDetHw_LogSuppressed = 0U;
}

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* SetUp(): Com → PduR → CanIf の順に、本テスト専用の設定で初期化する（Init 中の DET ログは抑制する）。
 * TearDown(): Com_DeInit()、CanIf_DeInit()、CanSM_DeInit() で未初期化へ戻し、PduR も未初期化へ戻す。 */
class ComStack_TriggerTransmit_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        InitStack();
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Com_DeInit();
        CanIf_DeInit();
        CanSM_DeInit();
        PduR_Test_ResetInitState();
        WrapCan_Reset();
        WrapCanIf_Reset();
        WrapPduR_Reset();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(ComStack_TriggerTransmit_Test, ComStack_TriggerTransmit_OK_ComTxBufferReachesCaller)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint16 value = 0x1234U;
    ASSERT_EQ(Com_SendSignal(0U, &value), E_OK);   // Com の TX バッファへ書く
    uint8 buf[8];
    for (uint8 i = 0U; i < 8U; i++) { buf[i] = 0xEEU; }
    PduInfoType info = { buf, 8U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    // CanIf → PduR → Com（CanIf が上位層の PDU ID 5 へ、PduR が Com の IPduId 2 へ翻訳する）
    const Std_ReturnType ret = CanIf_TriggerTransmit(kCanIfTxPduId, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(info.SduLength, 2U);
    EXPECT_EQ(buf[0], 0x12U);
    EXPECT_EQ(buf[1], 0x34U);
    EXPECT_EQ(buf[2], 0xEEU);                  // DLC を超える部分には触れない
    EXPECT_EQ(CallCount_Can_Write, 0U);        // 通常の送信（Can_Write）は、行わない
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(ComStack_TriggerTransmit_Test, ComStack_TriggerTransmit_NG_SmallBufferRejectedByComAndReportedToCaller)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint16 value = 0x1234U;
    ASSERT_EQ(Com_SendSignal(0U, &value), E_OK);
    uint8 buf[8];
    for (uint8 i = 0U; i < 8U; i++) { buf[i] = 0xEEU; }
    PduInfoType info = { buf, 1U };   // I-PDU の DLC（2）より小さい

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanIf_TriggerTransmit(kCanIfTxPduId, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);        // Com の E_NOT_OK が、PduR と CanIf を通って、そのまま届く
    EXPECT_EQ(buf[0], 0xEEU);        // 何もコピーされない
    EXPECT_EQ(info.SduLength, 1U);
}

}  // namespace
