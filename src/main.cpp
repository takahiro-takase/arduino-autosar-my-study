#include <Arduino.h>
#include "Can.h"
#include "CanIf.h"
#include "PduR.h"
#include <mcp_can_dfs.h>

// -------------------------------------------------------
// COM 層スタブ
// 実際の AUTOSAR では COM モジュールが担う。
// -------------------------------------------------------
static void Com_TxConfirmation(PduIdType PduId)
{
    Serial.print("[Com_TxConfirmation] ComPduId=");
    Serial.print(PduId);
    Serial.println(" : TX complete");
}

static void Com_RxIndication(PduIdType PduId, const PduInfoType* PduInfoPtr)
{
    Serial.print("[Com_RxIndication] ComPduId=");
    Serial.print(PduId);
    Serial.print(" Len=");
    Serial.print(PduInfoPtr->SduLength);
    Serial.print(" Data=");
    for (PduLengthType i = 0; i < PduInfoPtr->SduLength; i++)
    {
        Serial.print(PduInfoPtr->SduDataPtr[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
}

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
        .UpperLayerRxPduId = 0,                   // PduR RxPath SrcPduId=0 と一致
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
        .DestPduId = 0,                // COM の名前空間での PduId（ComPduId）
        .RxIndFct  = Com_RxIndication
    },
    {
        .Module    = PDUR_MODULE_DCM,
        .DestPduId = 0,                // DCM の名前空間での PduId（DcmPduId）
        .RxIndFct  = Diag_RxIndication
    }
};

// -------------------------------------------------------
// PduR RX RoutingPath テーブル
//   SrcPduId=0（CanIf の名前空間）→ COM ComPduId=0
// -------------------------------------------------------
static const PduR_RxRoutingPathType PduR_RxPaths[] = {
    {
        .SrcPduId  = 0,                 // CanIf RxPduId=0
        .Dests     = PduR_RxDests_Path0,
        .DestCount = 2                  // COM + DCM にマルチキャスト
    }
};

// -------------------------------------------------------
// PduR TX RoutingPath テーブル
//   SrcPduId=0（上位層の名前空間）→ CanIf TxPduId=0
//   TxConfirmation は Com_TxConfirmation(ComPduId=0) へ
// -------------------------------------------------------
static const PduR_TxRoutingPathType PduR_TxPaths[] = {
    {
        .SrcPduId      = 0,                  // 上位層が PduR_Transmit に渡す ID
        .CanIfTxPduId  = 0,                  // CanIf の名前空間での TX PduId
        .ConfDestPduId = 0,                  // COM の名前空間での TxConfirmation PduId
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
}

// -------------------------------------------------------
// Arduino loop()
// -------------------------------------------------------
void loop()
{
    // PduR_Transmit は Step 6 で有効化
    if (millis() - lastSendTime >= sendInterval)
    {
        lastSendTime = millis();
        uint8_t     data[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
        PduInfoType pduInfo = {.SduDataPtr = data, .SduLength = 8};
        (void)PduR_Transmit(0, &pduInfo);
    }

    Can_Isr();
}
