/**
 * \file    Com_TriggerTransmit_test.cpp
 * \brief   Com_TriggerTransmit() の単体テスト
 * \details 下位層の要求に応じて TX I-PDU の内容を複製する経路（[SWS_Com_00647]/[SWS_Com_00869]）、
 *          ComIPduTriggerTransmitCallout（[SWS_Com_00766]/[SWS_Com_00395]）、
 *          update-bit のクリア（ClearUpdateBit=TRIGGER_TRANSMIT、[SWS_Com_00578]）を確認する。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Com.h"
#include "Fake_Det_Hw.h"
#include "Fake_Millis.h"
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

// 設定テーブルは Com が参照し続けるため、テスト本体のローカル変数ではなく静的な領域に置く。
// TX I-PDU（IPduId 0、DLC 2、update-bit は byte1 の bit0）と、その I-PDU の TX シグナル（SignalId 90、byte0）。
Com_IPduConfigType    gIPdu;
Com_SignalConfigType  gSignal;
Com_ConfigType        gConfig;

const Com_SignalIdType kSignalId = 90U;

// コールバックの記録。
uint32 gCalloutCount;
uint8  gCalloutLength;
uint8  gCalloutData0;
boolean gCalloutReturn;

boolean OnTriggerTransmitCallout(const uint8* SduDataPtr, uint8 SduLength)
{
    gCalloutCount++;
    gCalloutLength = SduLength;
    gCalloutData0  = SduDataPtr[0];
    return gCalloutReturn;
}

void Prepare()
{
    gIPdu = {};
    gIPdu.IPduId            = 0U;
    gIPdu.DLC               = 2U;
    gIPdu.PduRId            = 0U;
    gIPdu.TxModeMode        = COM_TX_MODE_DIRECT;
    gIPdu.TxModeModeTrue    = COM_TX_MODE_DIRECT;
    gIPdu.UpdateBitPosition = 0xFFFFU;
    gIPdu.IpduGroupId       = COM_IPDU_GROUP_NONE;

    gSignal = {};
    gSignal.SignalId         = kSignalId;
    gSignal.Direction        = COM_SIGNAL_DIRECTION_TX;
    gSignal.IPduId           = 0U;
    gSignal.BitPosition      = 0U;
    gSignal.BitSize          = 8U;
    gSignal.Endian           = COM_BIG_ENDIAN;
    gSignal.FilterAlgorithm  = COM_FILTER_ALWAYS;
    gSignal.Mask             = 0xFFU;
    gSignal.TransferProperty = COM_TRANSFER_PROPERTY_TRIGGERED_ON_CHANGE;
    gSignal.RxDataTimeoutAction = COM_RX_TIMEOUT_ACTION_NONE;
    gSignal.DataInvalidAction   = COM_DATA_INVALID_ACTION_NONE;

    gCalloutCount  = 0U;
    gCalloutLength = 0U;
    gCalloutData0  = 0U;
    gCalloutReturn = TRUE;
}

void InitCom()
{
    gConfig.RxIPdus        = NULL;
    gConfig.RxIPduCount    = 0U;
    gConfig.TxIPdus        = &gIPdu;
    gConfig.TxIPduCount    = 1U;
    gConfig.Signals        = &gSignal;
    gConfig.SignalCount    = 1U;
    gConfig.GwMappings     = NULL;
    gConfig.GwMappingCount = 0U;

    FakeDetHw_LogSuppressed = 1U;
    Com_DeInit();
    Com_Init(&gConfig);
    FakeDetHw_Reset();
    FakeDetHw_LogSuppressed = 0U;
}

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* Com_TriggerTransmit() の単体テスト用フィクスチャ。
 * SetUp(): 設定を既定へ戻し、PduR への送信を成功させる（Com_MainFunctionTx() を使うテストのため）。
 * TearDown(): Com_DeInit() で未初期化へ戻す。 */
class Com_TriggerTransmit_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        WrapPduR_Reset();
        FailFromCallCount_PduR_ComTransmit = 0U;
        ForcedReturn_PduR_ComTransmit      = E_OK;
        Prepare();
        InitCom();
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Com_DeInit();
        WrapPduR_Reset();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Com_TriggerTransmit_Test, Com_TriggerTransmit_OK_CopiesTxBufferAndSetsLength)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    const uint8 value = 0x55U;
    ASSERT_EQ(Com_SendSignal(kSignalId, &value), E_OK);
    uint8 buf[8];
    for (uint8 i = 0U; i < 8U; i++) { buf[i] = 0xEEU; }
    PduInfoType info = { buf, 8U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Com_TriggerTransmit(0U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(info.SduLength, 2U);   // 複製した長さ（I-PDU の DLC）
    EXPECT_EQ(buf[0], 0x55U);        // TX バッファの内容
    EXPECT_EQ(buf[2], 0xEEU);        // DLC を超える部分には触れない
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(Com_TriggerTransmit_Test, Com_TriggerTransmit_NG_BufferTooSmallCopiesNothing)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    const uint8 value = 0x55U;
    ASSERT_EQ(Com_SendSignal(kSignalId, &value), E_OK);
    uint8 buf[8];
    for (uint8 i = 0U; i < 8U; i++) { buf[i] = 0xEEU; }
    PduInfoType info = { buf, 1U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Com_TriggerTransmit(0U, &info);   // DLC=2 に対して、バッファは 1 バイト

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);        // [SWS_Com_00869]
    EXPECT_EQ(buf[0], 0xEEU);        // 何もコピーしない
    EXPECT_EQ(info.SduLength, 1U);   // PduInfoPtr を変えない
}

TEST_F(Com_TriggerTransmit_Test, Com_TriggerTransmit_NG_UnregisteredTxPduId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 buf[8];
    for (uint8 i = 0U; i < 8U; i++) { buf[i] = 0xEEU; }
    PduInfoType info = { buf, 8U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Com_TriggerTransmit(1U, &info);   // 配列の範囲内だが、登録されていない

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, COM_E_PARAM);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);
    EXPECT_EQ(buf[0], 0xEEU);
}

TEST_F(Com_TriggerTransmit_Test, Com_TriggerTransmit_NG_TxPduIdOutOfRange)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 buf[8];
    for (uint8 i = 0U; i < 8U; i++) { buf[i] = 0xEEU; }
    PduInfoType info = { buf, 8U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Com_TriggerTransmit(200U, &info);   // COM_TX_IPDU_MAX を超える

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, COM_E_PARAM);
    EXPECT_EQ(buf[0], 0xEEU);
}

TEST_F(Com_TriggerTransmit_Test, Com_TriggerTransmit_NG_NullPduInfoPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Com_TriggerTransmit(0U, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, COM_E_PARAM_POINTER);
}

TEST_F(Com_TriggerTransmit_Test, Com_TriggerTransmit_NG_NullSduDataPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PduInfoType info = { NULL, 8U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Com_TriggerTransmit(0U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, COM_E_PARAM_POINTER);
}

TEST_F(Com_TriggerTransmit_Test, Com_TriggerTransmit_NG_Uninitialized)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Com_DeInit();   // 未初期化にする
    uint8 buf[8];
    for (uint8 i = 0U; i < 8U; i++) { buf[i] = 0xEEU; }
    PduInfoType info = { buf, 8U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Com_TriggerTransmit(0U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, COM_E_UNINIT);
    EXPECT_EQ(buf[0], 0xEEU);
}

TEST_F(Com_TriggerTransmit_Test, Com_TriggerTransmit_OK_DoesNotConsumeTransmitRequest)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    const uint8 value = 0x55U;
    ASSERT_EQ(Com_SendSignal(kSignalId, &value), E_OK);
    ASSERT_EQ(Com_Test_GetTxPending(0U), 1U);   // 送信が予約されている
    uint8 buf[8];
    for (uint8 i = 0U; i < 8U; i++) { buf[i] = 0xEEU; }
    PduInfoType info = { buf, 8U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Com_TriggerTransmit(0U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(Com_Test_GetTxPending(0U), 1U);   // 送信の予約は、そのまま（送信の規則には干渉しない。[SWS_Com_00475]）
    EXPECT_EQ(CallCount_PduR_ComTransmit, 0U);  // Com 自身は、PduR へ送らない
}

TEST_F(Com_TriggerTransmit_Test, Com_TriggerTransmit_OK_ClearsUpdateBitWhenConfiguredTriggerTransmit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    gIPdu.UpdateBitPosition = 15U;     // byte1 の bit0
    gSignal.UpdateBitContributor = 1U;
    gIPdu.ClearUpdateBit = COM_CLEAR_UPDATE_BIT_TRIGGER_TRANSMIT;
    InitCom();
    const uint8 value = 0x55U;
    ASSERT_EQ(Com_SendSignal(kSignalId, &value), E_OK);
    ASSERT_EQ(Com_Test_GetTxBuffer(0U)[1] & 0x01U, 1U);   // update-bit が立っている
    uint8 buf[8];
    for (uint8 i = 0U; i < 8U; i++) { buf[i] = 0xEEU; }
    PduInfoType info = { buf, 8U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Com_TriggerTransmit(0U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(buf[1] & 0x01U, 1U);                        // 複製したフレームには、update-bit が載る
    EXPECT_EQ(Com_Test_GetTxBuffer(0U)[1] & 0x01U, 0U);   // 渡した後に、クリアする（[SWS_Com_00578]）
}

TEST_F(Com_TriggerTransmit_Test, Com_TriggerTransmit_OK_KeepsUpdateBitWhenConfiguredTransmit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    gIPdu.UpdateBitPosition = 15U;     // byte1 の bit0
    gSignal.UpdateBitContributor = 1U;
    InitCom();   // ClearUpdateBit は既定（TRANSMIT）
    const uint8 value = 0x55U;
    ASSERT_EQ(Com_SendSignal(kSignalId, &value), E_OK);
    uint8 buf[8];
    for (uint8 i = 0U; i < 8U; i++) { buf[i] = 0xEEU; }
    PduInfoType info = { buf, 8U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Com_TriggerTransmit(0U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(Com_Test_GetTxBuffer(0U)[1] & 0x01U, 1U);   // TRANSMIT 設定は、TriggerTransmit ではクリアしない
}

TEST_F(Com_TriggerTransmit_Test, Com_TriggerTransmit_OK_TransmitDoesNotClearUpdateBitWhenConfiguredTriggerTransmit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    gIPdu.UpdateBitPosition = 15U;     // byte1 の bit0
    gSignal.UpdateBitContributor = 1U;
    gIPdu.ClearUpdateBit = COM_CLEAR_UPDATE_BIT_TRIGGER_TRANSMIT;
    InitCom();
    const uint8 value = 0x55U;
    ASSERT_EQ(Com_SendSignal(kSignalId, &value), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_MainFunctionTx();   // PduR_ComTransmit() が E_OK を返す

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_EQ(CallCount_PduR_ComTransmit, 1U);
    EXPECT_EQ(Com_Test_GetTxBuffer(0U)[1] & 0x01U, 1U);   // Transmit 直後には、クリアしない
}

TEST_F(Com_TriggerTransmit_Test, Com_TriggerTransmit_OK_CalloutCalledAndReturnValueIgnored)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    gIPdu.TriggerTransmitCalloutCbk = OnTriggerTransmitCallout;
    gCalloutReturn = FALSE;   // false を返しても、複製は取り消されない（[SWS_Com_00395]）
    InitCom();
    const uint8 value = 0x55U;
    ASSERT_EQ(Com_SendSignal(kSignalId, &value), E_OK);
    uint8 buf[8];
    for (uint8 i = 0U; i < 8U; i++) { buf[i] = 0xEEU; }
    PduInfoType info = { buf, 8U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Com_TriggerTransmit(0U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(gCalloutCount, 1U);
    EXPECT_EQ(gCalloutLength, 2U);    // 複製した長さ
    EXPECT_EQ(gCalloutData0, 0x55U);  // 複製したバイト列
}

TEST_F(Com_TriggerTransmit_Test, Com_TriggerTransmit_OK_CalloutNotCalledWhenIpduGroupStopped)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    gIPdu.TriggerTransmitCalloutCbk = OnTriggerTransmitCallout;
    gIPdu.IpduGroupId = 0U;
    InitCom();
    Com_IpduGroupStart(0U, FALSE);
    Com_IpduGroupStop(0U);   // I-PDU Group を停止する
    uint8 buf[8];
    for (uint8 i = 0U; i < 8U; i++) { buf[i] = 0xEEU; }
    PduInfoType info = { buf, 8U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Com_TriggerTransmit(0U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);             // 複製は行う
    EXPECT_EQ(gCalloutCount, 0U);     // 停止中は callout を呼ばない（[SWS_Com_00766]）
}

}  // namespace
