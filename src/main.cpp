/**
 * \file    main.cpp
 * \brief   Arduino エントリポイント・BSW スタック初期化
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include <Arduino.h>
#include "Can.h"
#include "Can_PBCfg.h"
#include "CanIf.h"
#include "CanIf_PBCfg.h"
#include "PduR.h"
#include "PduR_PBCfg.h"
#include "PduR_CanIf.h"
#include "Com.h"
#include "Rte.h"
#include "App_EngineManager.h"

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
// Arduino setup()
// -------------------------------------------------------
void setup()
{
    Serial.begin(115200);

    Can_Init(&Can_Config);
    Can_SetControllerMode(0U, CAN_T_START);
    CanIf_Init(&CanIf_Config);
    PduR_Init(&PduR_Config);
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
