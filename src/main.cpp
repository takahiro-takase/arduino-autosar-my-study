#include <Arduino.h>
#include "Can.h"
#include "CanIf.h"
#include "PduR.h"
#include "Com.h"
#include "Rte.h"
#include "App_EngineManager.h"
#include <mcp_can_dfs.h>

// -------------------------------------------------------
// COM I-PDU 設定（BSW 設定はここで集中管理）
//
// RX I-PDU (IPduId=0): CAN ID 0x100  EngineSpeed(16bit) + CoolantTemp(8bit) + EngineOnFlag(1bit)
// TX I-PDU (IPduId=0): CAN ID 0x200  EngineState(8bit)  0=OFF/1=STARTING/2=RUNNING/3=FAULT
// -------------------------------------------------------
static const Com_IPduConfigType Com_RxIPdus[] = {
    { .IPduId = 0, .DLC = 4, .PduRId = 0 }
};
static const Com_IPduConfigType Com_TxIPdus[] = {
    { .IPduId = 0, .DLC = 1, .PduRId = 0 }
};
static const Com_SignalConfigType Com_Signals[] = {
    /* RX signals (CAN ID 0x100) */
    { .SignalId = 0, .IPduId = 0, .BitPosition =  0, .BitSize = 16, .Endian = COM_BIG_ENDIAN }, /* EngineSpeed  */
    { .SignalId = 1, .IPduId = 0, .BitPosition = 16, .BitSize =  8, .Endian = COM_BIG_ENDIAN }, /* CoolantTemp  */
    { .SignalId = 2, .IPduId = 0, .BitPosition = 24, .BitSize =  1, .Endian = COM_BIG_ENDIAN }, /* EngineOnFlag */
    /* TX signals (CAN ID 0x200) */
    { .SignalId = 3, .IPduId = 0, .BitPosition =  0, .BitSize =  8, .Endian = COM_BIG_ENDIAN }  /* EngineState  */
};
static const Com_ConfigType ComConfig = {
    .RxIPdus     = Com_RxIPdus,  .RxIPduCount = 1,
    .TxIPdus     = Com_TxIPdus,  .TxIPduCount = 1,
    .Signals     = Com_Signals,  .SignalCount  = 4
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
    .filter      = {0x100, 0x7FF},   /* RX: CAN ID 0x100 のみ受信 */
    .csPin       = 10,
    .intPin      = 2,
    .baudrate    = CAN_500KBPS,
    .crystalFreq = CAN_CRYSTAL_8MHZ
};
static const CanIf_TxPduConfigType CanIf_TxPduConfig[] = {
    { .UpperLayerTxPduId = 0, .CanId = 0x200, .Dlc = 1, .Hth = 0, .TxConfirmFct = PduR_CanIfTxConfirmation }
};
static const CanIf_RxPduConfigType CanIf_RxPduConfig[] = {
    { .CanId = 0x100, .Hrh = 0, .UpperLayerRxPduId = 0, .RxIndicationFct = PduR_CanIfRxIndication }
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
// Arduino setup()
// -------------------------------------------------------
void setup()
{
    Serial.begin(115200);

    Can_Init(&CanConfig);
    Can_SetControllerMode(0U, CAN_T_START);
    CanIf_Init(&CanIfConfig);
    PduR_Init(&PduRConfig);
    Com_Init(&ComConfig);
    App_EngineManager_Init();
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
