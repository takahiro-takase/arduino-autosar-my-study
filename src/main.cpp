#include <Arduino.h>
#include "Can.h"
#include "CanIf.h"
#include "PduR.h"
#include "Com.h"
#include "Rte.h"       // SW-C は RTE ヘッダだけをインクルードする
#include <mcp_can_dfs.h>

// -------------------------------------------------------
// COM I-PDU 設定（BSW 設定はここで集中管理）
// -------------------------------------------------------
static const Com_IPduConfigType Com_RxIPdus[] = {
    { .IPduId = 0, .DLC = 8, .PduRId = 0 }
};
static const Com_IPduConfigType Com_TxIPdus[] = {
    { .IPduId = 0, .DLC = 8, .PduRId = 0 }
};
static const Com_SignalConfigType Com_Signals[] = {
    { .SignalId = 0, .IPduId = 0, .BitPosition =  0, .BitSize = 16, .Endian = COM_BIG_ENDIAN },
    { .SignalId = 1, .IPduId = 0, .BitPosition = 16, .BitSize =  8, .Endian = COM_BIG_ENDIAN },
    { .SignalId = 2, .IPduId = 0, .BitPosition = 24, .BitSize =  1, .Endian = COM_BIG_ENDIAN }
};
static const Com_ConfigType ComConfig = {
    .RxIPdus     = Com_RxIPdus,  .RxIPduCount = 1,
    .TxIPdus     = Com_TxIPdus,  .TxIPduCount = 1,
    .Signals     = Com_Signals,  .SignalCount  = 3
};

// -------------------------------------------------------
// DCM 層スタブ
// -------------------------------------------------------
static void Diag_RxIndication(PduIdType PduId, const PduInfoType* PduInfoPtr)
{
    Serial.print("[Diag_RxIndication] first_byte=0x");
    Serial.println(PduInfoPtr->SduDataPtr[0], HEX);
    (void)PduId;
}

// -------------------------------------------------------
// CanDrv / CanIf / PduR 設定
// -------------------------------------------------------
static const Can_ConfigType CanConfig = {
    .filter      = {0x123, 0x7FF},
    .csPin       = 10,
    .intPin      = 2,
    .baudrate    = CAN_500KBPS,
    .crystalFreq = CAN_CRYSTAL_8MHZ   // 使用するMCP2515モジュールのクリスタル周波数に合わせる
};
static const CanIf_TxPduConfigType CanIf_TxPduConfig[] = {
    { .UpperLayerTxPduId = 0, .CanId = 0x123, .Dlc = 8, .Hth = 0, .TxConfirmFct = PduR_CanIfTxConfirmation }
};
static const CanIf_RxPduConfigType CanIf_RxPduConfig[] = {
    { .CanId = 0x123, .Hrh = 0, .UpperLayerRxPduId = 0, .RxIndicationFct = PduR_CanIfRxIndication }
};
static const CanIf_ConfigType CanIfConfig = {
    .TxPduConfig = CanIf_TxPduConfig, .TxPduCount = 1,
    .RxPduConfig = CanIf_RxPduConfig, .RxPduCount = 1
};
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
    .RxPaths = PduR_RxPaths, .RxPathCount = 1,
    .TxPaths = PduR_TxPaths, .TxPathCount = 1
};

// -------------------------------------------------------
// SW-C: EngineMonitor（10ms 周期 Runnable）
//
// RTE API だけを使う。COM / PduR / CanIf は知らない。
// 「SpeedSensor ポートから EngineSpeed を読む」という概念だけを持つ。
// -------------------------------------------------------
void App_EngineMonitor(void)
{
    EngineSpeed_t  speed   = 0;
    CoolantTemp_t  temp    = 0;
    EngineOnFlag_t flag    = 0;

    // RTE Read：ポート名でアクセス（COM Signal ID は知らない）
    (void)Rte_Read_SpeedSensor_EngineSpeed(&speed);
    (void)Rte_Read_TempSensor_CoolantTemp(&temp);
    (void)Rte_Read_EngineStatus_EngineOnFlag(&flag);

    Serial.println("[App_EngineMonitor]");
    Serial.print("  EngineSpeed  = "); Serial.print(speed);  Serial.println(" rpm");
    Serial.print("  CoolantTemp  = "); Serial.print(temp);   Serial.println(" C");
    Serial.print("  EngineOnFlag = "); Serial.println(flag);

    if (temp >= 100)
    {
        Serial.println("  [WARN] Coolant overheating!");
    }
    if (flag == 0 && speed > 0)
    {
        Serial.println("  [WARN] Speed detected while engine is OFF");
    }
}

// -------------------------------------------------------
// SW-C: EngineControl（5ms 周期 Runnable）
//
// Rte_Write でシグナルをセットし、TX を要求する。
// CanIf / PduR / COM の実装詳細は知らない。
// -------------------------------------------------------
void App_EngineControl(void)
{

    EngineSpeed_t  rpm  = 1500;
    CoolantTemp_t  temp = 85;
    EngineOnFlag_t flag = 1;

    // RTE Write：シグナルを TX バッファへ積む
    (void)Rte_Write_EngineCmd_EngineSpeed(rpm);
    (void)Rte_Write_EngineCmd_CoolantTemp(temp);
    (void)Rte_Write_EngineCmd_EngineOnFlag(flag);

    // まとめて 1 フレームで送信
    (void)Com_TriggerIPDUSend(0);
}

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
//
// BSW ポーリング（Can_Isr）と RTE スケジューリングだけを行う。
// アプリロジックは一切書かない。RTE が Runnable を呼ぶ。
// -------------------------------------------------------
void loop()
{
    Can_Isr();
    Rte_ScheduleRunnables();
}
