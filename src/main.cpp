#include <Arduino.h>
#include "Can.h"
#include "CanIf.h"
#include "PduR.h"
#include <mcp_can_dfs.h>

// -------------------------------------------------------
// COM 層スタブ
// 実際の AUTOSAR では COM モジュールが担う。
// ここではシリアル出力で受け取りを確認するだけ。
// -------------------------------------------------------
static void Com_TxConfirmation(PduIdType PduId)
{
    Serial.print("[Com_TxConfirmation] PduId=");
    Serial.print(PduId);
    Serial.println(" : TX complete");
}

static void Com_RxIndication(PduIdType PduId, const PduInfoType* PduInfoPtr)
{
    Serial.print("[Com_RxIndication] PduId=");
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
// TxConfirmFct = PduR_CanIfTxConfirmation（PduR に転送）
// -------------------------------------------------------
static const CanIf_TxPduConfigType CanIf_TxPduConfig[] = {
    {
        .UpperLayerTxPduId = 0,                   // PduR 側の TxPduId
        .CanId             = 0x123,
        .Dlc               = 8,
        .Hth               = 0,
        .TxConfirmFct      = PduR_CanIfTxConfirmation // CanIf → PduR
    }
};

// -------------------------------------------------------
// CanIf RX PDU テーブル
// RxIndicationFct = PduR_CanIfRxIndication（PduR に転送）
// -------------------------------------------------------
static const CanIf_RxPduConfigType CanIf_RxPduConfig[] = {
    {
        .CanId              = 0x123,
        .Hrh                = 0,
        .UpperLayerRxPduId  = 0,                    // PduR 側の RxPduId
        .RxIndicationFct    = PduR_CanIfRxIndication // CanIf → PduR
    }
};

static const CanIf_ConfigType CanIfConfig = {
    .TxPduConfig = CanIf_TxPduConfig,
    .TxPduCount  = 1,
    .RxPduConfig = CanIf_RxPduConfig,
    .RxPduCount  = 1
};

// -------------------------------------------------------
// PduR TX ルーティングテーブル
// PduR TxPduId=0 → CanIf TxPduId=0 → Com_TxConfirmation
// -------------------------------------------------------
static const PduR_TxRouteType PduR_TxRoutes[] = {
    {
        .CanIfTxPduId           = 0,                // 転送先 CanIf TX PDU
        .UpperLayerTxConfirmFct = Com_TxConfirmation, // PduR → COM
        .UpperLayerTxPduId      = 0                 // COM 側の PDU ID
    }
};

// -------------------------------------------------------
// PduR RX ルーティングテーブル
// CanIf RxPduId=0 → Com_RxIndication(ComPduId=0)
// -------------------------------------------------------
static const PduR_RxRouteType PduR_RxRoutes[] = {
    {
        .UpperLayerRxFct = Com_RxIndication, // PduR → COM
        .UpperLayerPduId = 0                 // COM 側の PDU ID
    }
};

static const PduR_ConfigType PduRConfig = {
    .TxRoutes      = PduR_TxRoutes,
    .TxRouteCount  = 1,
    .RxRoutes      = PduR_RxRoutes,
    .RxRouteCount  = 1
};

// -------------------------------------------------------
// 送信周期
// -------------------------------------------------------
static unsigned long       lastSendTime = 0;
static const unsigned long sendInterval = 5000;

// -------------------------------------------------------
// Arduino setup()：AUTOSAR の起動シーケンスに相当
// 下位層から順に初期化する
// -------------------------------------------------------
void setup()
{
    Serial.begin(115200);

    // 1. CanDrv 初期化
    Can_Init(&CanConfig);

    // 2. コントローラ開始
    Can_SetControllerMode(CAN_CS_STARTED);

    // 3. CanIf 初期化（TX/RX PDU テーブル登録）
    CanIf_Init(&CanIfConfig);

    // 4. PduR 初期化（TX/RX ルーティングテーブル登録）
    PduR_Init(&PduRConfig);
}

// -------------------------------------------------------
// Arduino loop()
// アプリは PduR_Transmit を呼ぶだけ。
// CanIf や CanDrv の詳細は一切知らない。
// -------------------------------------------------------
void loop()
{
    // 5 秒周期で送信（PduR 経由）
    if (millis() - lastSendTime >= sendInterval)
    {
        lastSendTime = millis();
        uint8_t     data[8]  = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
        PduInfoType pduInfo  = {.SduDataPtr = data, .SduLength = 8};
        (void)PduR_Transmit(0, &pduInfo); // PduR TxPduId=0
    }

    // 受信（CanDrv → CanIf → PduR → COM の順に自動転送）
    Can_Isr();
}
