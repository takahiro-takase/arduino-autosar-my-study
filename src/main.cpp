#include <Arduino.h>
#include "Can.h"
#include "CanIf.h"
#include "PduR.h"
#include "Com.h"
#include <mcp_can_dfs.h>

// -------------------------------------------------------
// アプリケーション定義のシグナルID
// -------------------------------------------------------
#define COM_SIGNAL_ENGINE_SPEED   ((Com_SignalIdType)0)  // bit0-15,  16bit, big-endian
#define COM_SIGNAL_COOLANT_TEMP   ((Com_SignalIdType)1)  // bit16-23,  8bit
#define COM_SIGNAL_ENGINE_ON_FLAG ((Com_SignalIdType)2)  // bit24,     1bit

// -------------------------------------------------------
// COM I-PDU 設定
// -------------------------------------------------------
static const Com_IPduConfigType Com_RxIPdus[] = {
    { .IPduId = 0, .DLC = 8, .PduRId = 0 }  // PduR RxPath SrcPduId=0 と一致
};

static const Com_IPduConfigType Com_TxIPdus[] = {
    { .IPduId = 0, .DLC = 8, .PduRId = 0 }  // PduR TxPath SrcPduId=0 と一致
};

// -------------------------------------------------------
// COM シグナル設定テーブル
//   BitPosition: bit0 = byte[0]のMSB
//   big-endian : MSBのビット位置を指定
// -------------------------------------------------------
static const Com_SignalConfigType Com_Signals[] = {
    { .SignalId = COM_SIGNAL_ENGINE_SPEED,   .IPduId = 0, .BitPosition =  0, .BitSize = 16, .Endian = COM_BIG_ENDIAN },
    { .SignalId = COM_SIGNAL_COOLANT_TEMP,   .IPduId = 0, .BitPosition = 16, .BitSize =  8, .Endian = COM_BIG_ENDIAN },
    { .SignalId = COM_SIGNAL_ENGINE_ON_FLAG, .IPduId = 0, .BitPosition = 24, .BitSize =  1, .Endian = COM_BIG_ENDIAN }
};

static const Com_ConfigType ComConfig = {
    .RxIPdus      = Com_RxIPdus,
    .RxIPduCount  = 1,
    .TxIPdus      = Com_TxIPdus,
    .TxIPduCount  = 1,
    .Signals      = Com_Signals,
    .SignalCount   = 3
};

// -------------------------------------------------------
// DCM 層スタブ（診断ロガー）
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
// CanIf TX / RX PDU テーブル
// -------------------------------------------------------
static const CanIf_TxPduConfigType CanIf_TxPduConfig[] = {
    {
        .UpperLayerTxPduId = 0,
        .CanId             = 0x123,
        .Dlc               = 8,
        .Hth               = 0,
        .TxConfirmFct      = PduR_CanIfTxConfirmation
    }
};

static const CanIf_RxPduConfigType CanIf_RxPduConfig[] = {
    {
        .CanId             = 0x123,
        .Hrh               = 0,
        .UpperLayerRxPduId = 0,
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
// PduR RoutingPath 設定
// -------------------------------------------------------
static const PduR_RxDestType PduR_RxDests_Path0[] = {
    { .Module = PDUR_MODULE_COM, .DestPduId = 0, .RxIndFct = Com_RxIndication },
    { .Module = PDUR_MODULE_DCM, .DestPduId = 0, .RxIndFct = Diag_RxIndication }
};

static const PduR_RxRoutingPathType PduR_RxPaths[] = {
    { .SrcPduId = 0, .Dests = PduR_RxDests_Path0, .DestCount = 2 }
};

static const PduR_TxRoutingPathType PduR_TxPaths[] = {
    { .SrcPduId = 0, .CanIfTxPduId = 0, .ConfDestPduId = 0, .ConfFct = Com_TxConfirmation }
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
// シグナル単位で値をセットし、1フレームにまとめて送信する。
// -------------------------------------------------------
void loop()
{
    if (millis() - lastSendTime >= sendInterval)
    {
        lastSendTime = millis();

        // アプリはネイティブ型の変数ポインタを渡すだけ。
        // PDU内のバイト配置（big-endian）への変換は COM が担う。
        uint16 rpm      = 1500; // 0x05DC
        uint8  temp     = 85;   // 0x55
        uint8  engineOn = 1;

        (void)Com_SendSignal(COM_SIGNAL_ENGINE_SPEED,   (uint8*)&rpm);
        (void)Com_SendSignal(COM_SIGNAL_COOLANT_TEMP,   &temp);
        (void)Com_SendSignal(COM_SIGNAL_ENGINE_ON_FLAG, &engineOn);

        (void)Com_TriggerIPDUSend(0);
    }

    Can_Isr();
}
