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

// Signal Group と update-bit の転送の確認用の設定。
// RX I-PDU 0（DLC 3）: シグナル 40（byte0）、41（byte1）。update-bit は byte2 の bit0（位置 23）。
// TX I-PDU 0（DLC 3）: シグナル 50（byte0）、51（byte1）。update-bit は byte2 の bit0（位置 23）。
// TX I-PDU 1（DLC 1）: シグナル 52（byte0）。Signal Group でない単独のシグナル。
struct GwOptions
{
    uint8  rxIsGroup;     // RX I-PDU 0 を Signal Group にする
    uint16 rxUpdateBit;   // RX I-PDU 0 の update-bit の位置（0xFFFF で無し）
    uint8  txIsGroup;     // TX I-PDU 0 を Signal Group にする
    uint16 txUpdateBit;   // TX I-PDU 0 の update-bit の位置（0xFFFF で無し）
};

Com_IPduConfigType   gGwRxIPdu;
Com_IPduConfigType   gGwTxIPdus[2];
Com_SignalConfigType gGwSignals[5];
Com_GwMappingType    gGwMappings[2];
Com_ConfigType       gGwConfig;

const Com_SignalIdType kRxSig0 = 40U;   // RX byte0
const Com_SignalIdType kRxSig1 = 41U;   // RX byte1
const Com_SignalIdType kTxSig0 = 50U;   // TX I-PDU 0 の byte0
const Com_SignalIdType kTxSig1 = 51U;   // TX I-PDU 0 の byte1
const Com_SignalIdType kTxPlain = 52U;  // TX I-PDU 1 の byte0（単独）

// mappingCount 個のマッピング（gGwMappings[0]、[1]）で、Com を初期化し直す。
void InitGw(const GwOptions& o, uint8 mappingCount)
{
    gGwRxIPdu = {};
    gGwRxIPdu.IPduId            = 0U;
    gGwRxIPdu.DLC               = 3U;
    gGwRxIPdu.PduRId            = 0U;
    gGwRxIPdu.IsSignalGroup     = o.rxIsGroup;
    gGwRxIPdu.UpdateBitPosition = o.rxUpdateBit;
    gGwRxIPdu.IpduGroupId       = COM_IPDU_GROUP_NONE;

    for (uint8 i = 0U; i < 2U; i++)
    {
        gGwTxIPdus[i] = {};
        gGwTxIPdus[i].IPduId            = i;
        gGwTxIPdus[i].DLC               = (i == 0U) ? 3U : 1U;
        gGwTxIPdus[i].PduRId            = i;
        gGwTxIPdus[i].TxModeMode        = COM_TX_MODE_DIRECT;
        gGwTxIPdus[i].TxModeModeTrue    = COM_TX_MODE_DIRECT;
        gGwTxIPdus[i].UpdateBitPosition = 0xFFFFU;
        gGwTxIPdus[i].IpduGroupId       = COM_IPDU_GROUP_NONE;
    }
    gGwTxIPdus[0].IsSignalGroup     = o.txIsGroup;
    gGwTxIPdus[0].UpdateBitPosition = o.txUpdateBit;

    gGwSignals[0] = MakeSignal(kRxSig0,  COM_SIGNAL_DIRECTION_RX, 0U);
    gGwSignals[1] = MakeSignal(kRxSig1,  COM_SIGNAL_DIRECTION_RX, 0U);
    gGwSignals[1].BitPosition = 8U;
    gGwSignals[2] = MakeSignal(kTxSig0,  COM_SIGNAL_DIRECTION_TX, 0U);
    gGwSignals[3] = MakeSignal(kTxSig1,  COM_SIGNAL_DIRECTION_TX, 0U);
    gGwSignals[3].BitPosition = 8U;
    gGwSignals[4] = MakeSignal(kTxPlain, COM_SIGNAL_DIRECTION_TX, 1U);
    // 転送先のシグナルが、I-PDU の update-bit を立てる（[SWS_Com_00706]）。
    gGwSignals[2].UpdateBitContributor = 1U;
    gGwSignals[3].UpdateBitContributor = 1U;

    gGwMappings[0].SrcSignalId  = kRxSig0;
    gGwMappings[0].DestSignalId = kTxSig0;
    gGwMappings[1].SrcSignalId  = kRxSig1;
    gGwMappings[1].DestSignalId = kTxSig1;

    gGwConfig = {};
    gGwConfig.RxIPdus        = &gGwRxIPdu;
    gGwConfig.RxIPduCount    = 1U;
    gGwConfig.TxIPdus        = gGwTxIPdus;
    gGwConfig.TxIPduCount    = 2U;
    gGwConfig.Signals        = gGwSignals;
    gGwConfig.SignalCount    = 5U;
    gGwConfig.GwMappings     = gGwMappings;
    gGwConfig.GwMappingCount = mappingCount;

    FakeDetHw_LogSuppressed = 1U;
    Com_DeInit();
    Com_Init(&gGwConfig);
    FakeDetHw_Reset();
    FakeDetHw_LogSuppressed = 0U;
}

// 3 バイトのフレームを、RX I-PDU 0 で受信したことにする。
void ReceiveFrame3(uint8 b0, uint8 b1, uint8 b2)
{
    uint8 frame[3] = { b0, b1, b2 };
    PduInfoType info = { frame, 3U };
    Com_RxIndication(0U, &info);
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

TEST_F(Com_MainFunctionRouteSignals_Test, Com_MainFunctionRouteSignals_OK_RoutesWhenSourceUpdateBitIsSet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    InitGw({ 0U, 23U, 0U, 0xFFFFU }, 1U);   // 転送元だけが update-bit を持つ
    ReceiveFrame3(0x11U, 0x22U, 0x01U);      // update-bit が立っている

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_MainFunctionRouteSignals();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Com_Test_GetTxBuffer(0U)[0], 0x11U);   // 転送する（[SWS_Com_00704]。転送先に update-bit は無い）
    EXPECT_EQ(Com_Test_GetTxBuffer(0U)[2], 0U);      // 転送先に update-bit が無いので、立てない
}

TEST_F(Com_MainFunctionRouteSignals_Test, Com_MainFunctionRouteSignals_OK_DoesNotRouteWhenSourceUpdateBitIsClear)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    InitGw({ 0U, 23U, 0U, 0xFFFFU }, 2U);   // 転送元だけが update-bit を持つ。2 つのマッピングとも
    ReceiveFrame3(0x11U, 0x22U, 0x00U);      // update-bit が立っていない

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_MainFunctionRouteSignals();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Com_Test_GetTxBuffer(0U)[0], 0U);   // 転送しない（[SWS_Com_00705]）
    EXPECT_EQ(Com_Test_GetTxBuffer(0U)[1], 0U);   // I-PDU の update-bit なので、I-PDU のどのシグナルも転送しない
}

TEST_F(Com_MainFunctionRouteSignals_Test, Com_MainFunctionRouteSignals_OK_DoesNotRouteWhenSourceUpdateBitIsClearAndDestinationHasUpdateBit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    InitGw({ 0U, 23U, 0U, 23U }, 1U);   // 転送元・転送先とも update-bit を持つ
    ReceiveFrame3(0x11U, 0x22U, 0x00U);   // 転送元の update-bit が立っていない

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_MainFunctionRouteSignals();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Com_Test_GetTxBuffer(0U)[0], 0U);          // 転送しない（[SWS_Com_00703]）
    EXPECT_EQ(Com_Test_GetTxBuffer(0U)[2] & 0x01U, 0U);  // 転送先の update-bit も立てない
}

TEST_F(Com_MainFunctionRouteSignals_Test, Com_MainFunctionRouteSignals_OK_RoutesWithDestinationUpdateBitWhenSourceUpdateBitIsSet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    InitGw({ 0U, 23U, 0U, 23U }, 1U);   // 転送元・転送先とも update-bit を持つ
    ReceiveFrame3(0x11U, 0x22U, 0x01U);   // 転送元の update-bit が立っている

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_MainFunctionRouteSignals();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Com_Test_GetTxBuffer(0U)[0], 0x11U);
    EXPECT_EQ(Com_Test_GetTxBuffer(0U)[2] & 0x01U, 1U);  // 転送先の update-bit が立つ（[SWS_Com_00702]）
}

TEST_F(Com_MainFunctionRouteSignals_Test, Com_MainFunctionRouteSignals_OK_SetsDestinationUpdateBitWhenSourceHasNoUpdateBit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    InitGw({ 0U, 0xFFFFU, 0U, 23U }, 1U);   // 転送先だけが update-bit を持つ
    ReceiveFrame3(0x11U, 0x22U, 0x00U);      // 新しいデータを受信した

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_MainFunctionRouteSignals();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Com_Test_GetTxBuffer(0U)[0], 0x11U);
    EXPECT_EQ(Com_Test_GetTxBuffer(0U)[2] & 0x01U, 1U);  // 受信のたびに、転送先の update-bit を立てる（[SWS_Com_00706]）
}

TEST_F(Com_MainFunctionRouteSignals_Test, Com_MainFunctionRouteSignals_OK_KeepsUpdatedFrameWhenLaterFrameHasClearUpdateBit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    InitGw({ 0U, 23U, 0U, 0xFFFFU }, 1U);   // 転送元だけが update-bit を持つ
    ReceiveFrame3(0x11U, 0x22U, 0x01U);      // 更新ありのフレーム（保留が立つ）
    ReceiveFrame3(0x11U, 0x22U, 0x00U);      // 転送の前に、更新なしのフレームが続けて届く

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_MainFunctionRouteSignals();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Com_Test_GetTxBuffer(0U)[0], 0x11U);   // 先に届いた「更新あり」の転送は、失われない
}

TEST_F(Com_MainFunctionRouteSignals_Test, Com_MainFunctionRouteSignals_NG_DoesNotCommitGroupWhenSendSignalFails)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    InitGw({ 0U, 0xFFFFU, 1U, 0xFFFFU }, 1U);   // 転送先 TX I-PDU 0 は Signal Group
    gGwSignals[2].Direction = COM_SIGNAL_DIRECTION_RX;   // 転送先のシグナル 50 を、TX シグナルでなくして、Com_SendSignal() を失敗させる
    const uint8 stale = 0x77U;
    ASSERT_EQ(Com_SendSignal(kTxSig1, &stale), E_OK);    // シャドウバッファに、コミットされていない古い値を置く
    ReceiveFrame3(0x11U, 0x22U, 0x00U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_MainFunctionRouteSignals();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Com_Test_GetTxBuffer(0U)[1], 0U);   // 書けなかったので、古いシャドウバッファの値は、コミットされない
}

TEST_F(Com_MainFunctionRouteSignals_Test, Com_MainFunctionRouteSignals_OK_RoutesGroupMembersAsOneConsistentSet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    InitGw({ 0U, 0xFFFFU, 1U, 0xFFFFU }, 2U);   // 転送先 TX I-PDU 0 は Signal Group（メンバー 50、51）
    ReceiveFrame3(0x11U, 0x22U, 0x00U);
    ASSERT_EQ(Com_Test_GetTxBuffer(0U)[0], 0U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_MainFunctionRouteSignals();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Com_Test_GetTxBuffer(0U)[0], 0x11U);   // 2 つのメンバーが、1 回の転送で、そろって実バッファへコミットされる
    EXPECT_EQ(Com_Test_GetTxBuffer(0U)[1], 0x22U);   // （[SWS_Com_00383]）
}

TEST_F(Com_MainFunctionRouteSignals_Test, Com_MainFunctionRouteSignals_OK_RoutesSubsetOfGroupMembersIntoReducedGroup)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    InitGw({ 0U, 0xFFFFU, 1U, 0xFFFFU }, 1U);   // 転送先の Signal Group のうち、メンバー 50 だけを転送する
    ReceiveFrame3(0x11U, 0x22U, 0x00U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_MainFunctionRouteSignals();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Com_Test_GetTxBuffer(0U)[0], 0x11U);   // 転送するメンバーは、コミットされる
    EXPECT_EQ(Com_Test_GetTxBuffer(0U)[1], 0U);      // 転送しないメンバーは、変わらない（[SWS_Com_00735]）
}

TEST_F(Com_MainFunctionRouteSignals_Test, Com_MainFunctionRouteSignals_OK_RoutesGroupMemberToPlainSignal)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    InitGw({ 1U, 0xFFFFU, 0U, 0xFFFFU }, 1U);   // 転送元 RX I-PDU 0 は Signal Group
    gGwMappings[0].SrcSignalId  = kRxSig1;      // グループのメンバー 41 を、
    gGwMappings[0].DestSignalId = kTxPlain;     // グループでない単独のシグナル 52 へ転送する
    ReceiveFrame3(0x11U, 0x22U, 0x00U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_MainFunctionRouteSignals();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Com_Test_GetTxBuffer(1U)[0], 0x22U);   // 単独のシグナルへ転送できる（[SWS_Com_00833]）
}

TEST_F(Com_MainFunctionRouteSignals_Test, Com_MainFunctionRouteSignals_OK_DoesNotRouteGroupWhenFrameIsShorterThanGroup)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    InitGw({ 1U, 0xFFFFU, 0U, 0xFFFFU }, 1U);   // 転送元 RX I-PDU 0 は Signal Group（DLC 3）
    uint8 frame[2] = { 0x11U, 0x22U };
    PduInfoType info = { frame, 2U };           // グループの長さに満たない
    Com_RxIndication(0U, &info);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_MainFunctionRouteSignals();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Com_Test_GetTxBuffer(0U)[0], 0U);   // I-PDU 全体が棄却されるので、転送しない（[SWS_Com_00575]）
}

}  // namespace
