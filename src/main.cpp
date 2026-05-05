#include <Arduino.h>
#include "Can.h"
#include "CanIf.h"
#include "PduR.h"
#include "Com.h"
#include <mcp_can_dfs.h>

// -------------------------------------------------------
// アプリケーション定義のシグナルID
//   実際の AUTOSAR では ARXML から自動生成される。
// -------------------------------------------------------
#define COM_SIGNAL_ENGINE_RPM    ((Com_SignalIdType)0)  // PDU0 bytes[0-1] uint16
#define COM_SIGNAL_COOLANT_TEMP  ((Com_SignalIdType)1)  // PDU0 byte[2]    uint8

// -------------------------------------------------------
// COM シグナル設定テーブル
//   SignalId / 属するIPduId / PDU内バイトオフセット / バイト長
// -------------------------------------------------------
static const Com_SignalConfigType Com_Signals[] = {
    { .SignalId = COM_SIGNAL_ENGINE_RPM,   .IPduId = 0, .ByteOffset = 0, .ByteLength = 2 },
    { .SignalId = COM_SIGNAL_COOLANT_TEMP, .IPduId = 0, .ByteOffset = 2, .ByteLength = 1 }
};

static const Com_ConfigType ComConfig = {
    .Signals     = Com_Signals,
    .SignalCount  = 2,
    .TxIPduId    = 0,   // PduR TX RoutingPath の SrcPduId=0 と一致
    .TxDlc       = 8
};

// -------------------------------------------------------
// DCM 層スタブ（診断ロガー）
// マルチキャストの受信先として COM と同時に呼ばれる。
// 実際の AUTOSAR では DCM モジュールが診断フレームを解析する。
// -------------------------------------------------------
static void Diag_RxIndication(PduIdType PduId, const PduInfoType* PduInfoPtr)
{
    Serial.print("[Diag_RxIndication] DcmPduId=");
    Serial.print(PduId);
    Serial.print(" first_byte=0x");
    Serial.println(PduInfoPtr->SduDataPtr[0], HEX);
}

// -------------------------------------------------------
// CanDrv 設定
// -------------------------------------------------------
static const Can_ConfigType CanConfig = {
    .filter   = {0x123, 0x7FF},
    .csPin    = 10,
    .intPin   = 2,
    .baudrate = CAN_500KBPS
};

// -------------------------------------------------------
// CanIf TX PDU テーブル
//   UpperLayerTxPduId = PduR TX RoutingPath の SrcPduId と一致させる
//   TxConfirmFct      = PduR_CanIfTxConfirmation（PduR に転送）
// -------------------------------------------------------
static const CanIf_TxPduConfigType CanIf_TxPduConfig[] = {
    {
        .UpperLayerTxPduId = 0,                    // PduR TxPath SrcPduId=0 と一致
        .CanId             = 0x123,
        .Dlc               = 8,
        .Hth               = 0,
        .TxConfirmFct      = PduR_CanIfTxConfirmation
    }
};

// -------------------------------------------------------
// CanIf RX PDU テーブル
//   UpperLayerRxPduId = PduR RX RoutingPath の SrcPduId と一致させる
//   RxIndicationFct   = PduR_CanIfRxIndication（PduR に転送）
// -------------------------------------------------------
static const CanIf_RxPduConfigType CanIf_RxPduConfig[] = {
    {
        .CanId             = 0x123,
        .Hrh               = 0,
        .UpperLayerRxPduId = 0,                    // PduR RxPath SrcPduId=0 と一致
        .RxIndicationFct   = PduR_CanIfRxIndication
    }
};

static const CanIf_ConfigType CanIfConfig = {
    .TxPduConfig = CanIf_TxPduConfig,
    .TxPduCount  = 1,
    .RxPduConfig = CanIf_RxPduConfig,
    .RxPduCount  = 1
};

// -------------------------------------------------------
// PduR RX RoutingPath 転送先（マルチキャスト：COM と DCM に同時配信）
// -------------------------------------------------------
static const PduR_RxDestType PduR_RxDests_Path0[] = {
    {
        .Module    = PDUR_MODULE_COM,
        .DestPduId = 0,                // COM の名前空間での PduId
        .RxIndFct  = Com_RxIndication
    },
    {
        .Module    = PDUR_MODULE_DCM,
        .DestPduId = 0,                // DCM の名前空間での PduId
        .RxIndFct  = Diag_RxIndication
    }
};

// -------------------------------------------------------
// PduR RX RoutingPath テーブル
//   SrcPduId=0（CanIf の名前空間）→ COM と DCM にマルチキャスト
// -------------------------------------------------------
static const PduR_RxRoutingPathType PduR_RxPaths[] = {
    {
        .SrcPduId  = 0,
        .Dests     = PduR_RxDests_Path0,
        .DestCount = 2
    }
};

// -------------------------------------------------------
// PduR TX RoutingPath テーブル
//   SrcPduId=0（上位層の名前空間）→ CanIf TxPduId=0
//   TxConfirmation は Com_TxConfirmation(ComPduId=0) へ
// -------------------------------------------------------
static const PduR_TxRoutingPathType PduR_TxPaths[] = {
    {
        .SrcPduId      = 0,
        .CanIfTxPduId  = 0,
        .ConfDestPduId = 0,
        .ConfFct       = Com_TxConfirmation
    }
};

static const PduR_ConfigType PduRConfig = {
    .RxPaths     = PduR_RxPaths,
    .RxPathCount = 1,
    .TxPaths     = PduR_TxPaths,
    .TxPathCount = 1
};

// -------------------------------------------------------
// 送信周期
// -------------------------------------------------------
static unsigned long       lastSendTime = 0;
static const unsigned long sendInterval = 5000;

// -------------------------------------------------------
// Arduino setup()
// -------------------------------------------------------
void setup()
{
    Serial.begin(115200);

    Can_Init(&CanConfig);
    Can_SetControllerMode(CAN_CS_STARTED);
    CanIf_Init(&CanIfConfig);
    PduR_Init(&PduRConfig);
    Com_Init(&ComConfig);
}

// -------------------------------------------------------
// Arduino loop()
// アプリ層は PDU を直接組み立てず、シグナル単位で値を渡す。
// COM がシグナルをバイト列に詰め、PduR 経由で送信する。
// -------------------------------------------------------
void loop()
{
    if (millis() - lastSendTime >= sendInterval)
    {
        lastSendTime = millis();

        // シグナル値を COM バッファに書き込む（big-endian）
        uint16 rpm  = 1500;  // 1500 rpm
        uint8  temp = 85;    // 85 ℃

        uint8 rpmBytes[2] = { (uint8)(rpm >> 8), (uint8)(rpm & 0xFF) };
        (void)Com_SendSignal(COM_SIGNAL_ENGINE_RPM,   rpmBytes);
        (void)Com_SendSignal(COM_SIGNAL_COOLANT_TEMP, &temp);

        // まとめて 1 フレームで送信
        (void)Com_TriggerIPDUSend();
    }

    Can_Isr();
}
