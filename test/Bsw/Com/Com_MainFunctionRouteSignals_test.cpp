/**
 * \file    Com_MainFunctionRouteSignals_test.cpp
 * \brief   Com_MainFunctionRouteSignals() の単体テスト
 * \details Signal Gateway の転送が、Com_RxIndication() が立てた保留を、本関数が処理したときに行われること
 *          （[SWS_Com_00359]）、中継元の I-PDU 以外の受信では転送しないこと、保留が I-PDU ごとに 1 つ（最新値を
 *          1 回だけ転送）であること、停止した I-PDU の保留は捨てること、未初期化では何もしないこと
 *          （[SWS_Com_00666]）を確認する。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Com.h"
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

// 設定テーブルは Com が参照し続けるため、テスト本体のローカル変数ではなく静的な領域に置く。
// RX I-PDU 2 つ（IPduId と PduRId が 0 と 1。DLC 1）、TX I-PDU 1 つ（IPduId 0）、
// RX シグナル 2 つ（30 は RX I-PDU 0、31 は RX I-PDU 1）と TX シグナル 1 つ（32）、
// Signal Gateway は RX シグナル 30 → TX シグナル 32。
Com_IPduConfigType    gRxIPdus[2];
Com_IPduConfigType    gTxIPdu;
Com_SignalConfigType  gSignals[3];
Com_GwMappingType     gGwMapping;
Com_ConfigType        gConfig;

const Com_SignalIdType kRxSignalOnPdu0 = 30U;
const Com_SignalIdType kRxSignalOnPdu1 = 31U;
const Com_SignalIdType kTxSignal       = 32U;

Com_SignalConfigType MakeSignal(Com_SignalIdType id, Com_SignalDirectionType dir, uint8 ipduId)
{
    Com_SignalConfigType s = {};
    s.SignalId            = id;
    s.Direction           = dir;
    s.IPduId              = ipduId;
    s.BitPosition         = 0U;
    s.BitSize             = 8U;
    s.Endian              = COM_BIG_ENDIAN;
    s.FilterAlgorithm     = COM_FILTER_ALWAYS;
    s.Mask                = 0xFFU;
    s.TransferProperty    = COM_TRANSFER_PROPERTY_PENDING;
    s.RxDataTimeoutAction = COM_RX_TIMEOUT_ACTION_NONE;
    s.DataInvalidAction   = COM_DATA_INVALID_ACTION_NONE;
    return s;
}

// rxGroupId: RX I-PDU 0 の I-PDU Group（COM_IPDU_GROUP_NONE なら所属しない。常に起動済み）。
void InitCom(Com_IpduGroupIdType rxGroupId)
{
    for (uint8 i = 0U; i < 2U; i++)
    {
        gRxIPdus[i] = {};
        gRxIPdus[i].IPduId            = i;
        gRxIPdus[i].DLC               = 1U;
        gRxIPdus[i].PduRId            = i;
        gRxIPdus[i].UpdateBitPosition = 0xFFFFU;
        gRxIPdus[i].IpduGroupId       = COM_IPDU_GROUP_NONE;
    }
    gRxIPdus[0].IpduGroupId = rxGroupId;

    gTxIPdu = {};
    gTxIPdu.IPduId            = 0U;
    gTxIPdu.DLC               = 1U;
    gTxIPdu.PduRId            = 0U;
    gTxIPdu.TxModeMode        = COM_TX_MODE_DIRECT;
    gTxIPdu.TxModeModeTrue    = COM_TX_MODE_DIRECT;
    gTxIPdu.UpdateBitPosition = 0xFFFFU;
    gTxIPdu.IpduGroupId       = COM_IPDU_GROUP_NONE;

    gSignals[0] = MakeSignal(kRxSignalOnPdu0, COM_SIGNAL_DIRECTION_RX, 0U);
    gSignals[1] = MakeSignal(kRxSignalOnPdu1, COM_SIGNAL_DIRECTION_RX, 1U);
    gSignals[2] = MakeSignal(kTxSignal,       COM_SIGNAL_DIRECTION_TX, 0U);

    gGwMapping.SrcSignalId  = kRxSignalOnPdu0;
    gGwMapping.DestSignalId = kTxSignal;

    gConfig = {};
    gConfig.RxIPdus        = gRxIPdus;
    gConfig.RxIPduCount    = 2U;
    gConfig.TxIPdus        = &gTxIPdu;
    gConfig.TxIPduCount    = 1U;
    gConfig.Signals        = gSignals;
    gConfig.SignalCount    = 3U;
    gConfig.GwMappings     = &gGwMapping;
    gConfig.GwMappingCount = 1U;

    FakeDetHw_LogSuppressed = 1U;
    Com_DeInit();
    Com_Init(&gConfig);
    FakeDetHw_Reset();
    FakeDetHw_LogSuppressed = 0U;
}

// 1 バイトのデータを、RxPduId の I-PDU で受信したことにする。
void ReceiveOn(PduIdType rxPduId, uint8 value)
{
    uint8 frame[1] = { value };
    PduInfoType info = { frame, 1U };
    Com_RxIndication(rxPduId, &info);
}

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* SetUp(): どの I-PDU Group にも所属しない設定で Com_Init() を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): Com_DeInit() で未初期化へ戻す。 */
class Com_MainFunctionRouteSignals_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        InitCom(COM_IPDU_GROUP_NONE);
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Com_DeInit();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Com_MainFunctionRouteSignals_Test, Com_MainFunctionRouteSignals_OK_ForwardsReceivedSignalToTransmitSignal)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ReceiveOn(0U, 0x5AU);
    ASSERT_EQ(Com_Test_GetTxBuffer(0U)[0], 0U);   // 受信しただけでは、まだ転送しない

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_MainFunctionRouteSignals();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Com_Test_GetTxBuffer(0U)[0], 0x5AU);   // 受信した値が、宛先の TX シグナルへ書かれる（[SWS_Com_00359]）
}

TEST_F(Com_MainFunctionRouteSignals_Test, Com_MainFunctionRouteSignals_OK_IgnoresFramesOfOtherReceiveIPdu)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ReceiveOn(1U, 0x5AU);   // 中継元のシグナルを含まない I-PDU のフレーム

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_MainFunctionRouteSignals();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Com_Test_GetTxBuffer(0U)[0], 0U);   // 中継しない
}

TEST_F(Com_MainFunctionRouteSignals_Test, Com_MainFunctionRouteSignals_OK_IgnoresUnknownSourceSignal)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    gGwMapping.SrcSignalId = 99U;   // 中継元に、存在しないシグナルを指定している
    ReceiveOn(0U, 0x5AU);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_MainFunctionRouteSignals();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Com_Test_GetTxBuffer(0U)[0], 0U);   // 中継しない
}

TEST_F(Com_MainFunctionRouteSignals_Test, Com_MainFunctionRouteSignals_OK_ForwardsLatestValueOnlyOnce)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ReceiveOn(0U, 0x11U);
    ReceiveOn(0U, 0x22U);   // 呼ばれる前に、同じ I-PDU をもう一度受信する

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_MainFunctionRouteSignals();
    const uint8 afterFirst = Com_Test_GetTxBuffer(0U)[0];
    const uint8 manual = 0x77U;
    (void)Com_SendSignal(kTxSignal, &manual);   // 宛先を、別の値へ書き換える
    Com_MainFunctionRouteSignals();             // 新しい受信が無いので、転送しない

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(afterFirst, 0x22U);                   // 保留は I-PDU ごとに 1 つで、最新の値を転送する
    EXPECT_EQ(Com_Test_GetTxBuffer(0U)[0], 0x77U);  // 2 回目の呼び出しでは、転送しない（保留を下ろしている）
}

TEST_F(Com_MainFunctionRouteSignals_Test, Com_MainFunctionRouteSignals_OK_DropsPendingOfStoppedIpdu)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    InitCom(1U);                           // RX I-PDU 0 を、I-PDU Group 1 に所属させる（既定で停止）
    Com_IpduGroupStart(1U, FALSE);
    ReceiveOn(0U, 0x5AU);                  // 起動中に受信して、保留を立てる
    Com_IpduGroupStop(1U);                 // 転送の前に停止する
    Com_IpduGroupStart(1U, FALSE);         // 再び起動する（新しい受信は無い）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_MainFunctionRouteSignals();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Com_Test_GetTxBuffer(0U)[0], 0U);   // 停止中に捨てられた保留は、起動し直しても転送しない
}

TEST_F(Com_MainFunctionRouteSignals_Test, Com_MainFunctionRouteSignals_NG_UninitDoesNothing)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ReceiveOn(0U, 0x5AU);
    Com_DeInit();   // 未初期化にする
    FakeDetHw_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_MainFunctionRouteSignals();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);   // 未初期化では、何もせずに戻る。DET へも報告しない（[SWS_Com_00666]）
}

}  // namespace
