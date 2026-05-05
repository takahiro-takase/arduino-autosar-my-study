#include <Arduino.h>
#include "Rte.h"
#include "Com.h"

// -------------------------------------------------------
// COM Signal ID との対応（RTE だけが知るマッピング）
//
// SW-C は「SpeedSensor ポートから EngineSpeed を読む」とだけ知る。
// 「COM_SIGNAL_ENGINE_SPEED = 0」というBSW内部の詳細は RTE が隠す。
// -------------------------------------------------------
#define RTE_SIGNAL_ENGINE_SPEED    ((Com_SignalIdType)0)
#define RTE_SIGNAL_COOLANT_TEMP    ((Com_SignalIdType)1)
#define RTE_SIGNAL_ENGINE_ON_FLAG  ((Com_SignalIdType)2)

#define RTE_TX_IPDU_ID             ((Com_IPduIdType)0)

// -------------------------------------------------------
// Runnable 宣言（SW-C が定義し、RTE が呼ぶ）
// AUTOSAR では ARXML で自動生成されるが、ここでは手書きで宣言する
// -------------------------------------------------------
extern void App_EngineMonitor(void);
extern void App_EngineControl(void);

// -------------------------------------------------------
// Rte_Read 実装
// COM の ReceiveSignal を呼ぶだけの薄いラッパー。
// 将来: データ一貫性保護（割り込み禁止区間）を追加できる。
// -------------------------------------------------------
Std_ReturnType Rte_Read_SpeedSensor_EngineSpeed(EngineSpeed_t* data)
{
    return Com_ReceiveSignal(RTE_SIGNAL_ENGINE_SPEED, (uint8*)data);
}

Std_ReturnType Rte_Read_TempSensor_CoolantTemp(CoolantTemp_t* data)
{
    return Com_ReceiveSignal(RTE_SIGNAL_COOLANT_TEMP, (uint8*)data);
}

Std_ReturnType Rte_Read_EngineStatus_EngineOnFlag(EngineOnFlag_t* data)
{
    return Com_ReceiveSignal(RTE_SIGNAL_ENGINE_ON_FLAG, (uint8*)data);
}

// -------------------------------------------------------
// Rte_Write 実装
// COM の SendSignal → TriggerIPDUSend を呼ぶ。
// 将来: 送信タイミング制御（イベント/周期）をここで管理できる。
// -------------------------------------------------------
Std_ReturnType Rte_Write_EngineCmd_EngineSpeed(EngineSpeed_t data)
{
    return Com_SendSignal(RTE_SIGNAL_ENGINE_SPEED, (uint8*)&data);
}

Std_ReturnType Rte_Write_EngineCmd_CoolantTemp(CoolantTemp_t data)
{
    return Com_SendSignal(RTE_SIGNAL_COOLANT_TEMP, (uint8*)&data);
}

Std_ReturnType Rte_Write_EngineCmd_EngineOnFlag(EngineOnFlag_t data)
{
    return Com_SendSignal(RTE_SIGNAL_ENGINE_ON_FLAG, (uint8*)&data);
}

// -------------------------------------------------------
// Rte_ScheduleRunnables
// millis() ベースで各 Runnable を周期呼び出しする。
// tick カウンタではなく実時間で管理するため、
// loop() の実行時間に依存しない。
//
// Arduino では loop() から呼ぶ。
// RTOS があれば周期タスクが各 Runnable を直接呼ぶ。
// -------------------------------------------------------
void Rte_ScheduleRunnables(void)
{
    static uint32 lastMonitorTime = 0;
    static uint32 lastControlTime = 0;

    const uint32 now = millis();

    if (now - lastMonitorTime >= 3000U)
    {
        lastMonitorTime = now;
        App_EngineMonitor();
    }

    if (now - lastControlTime >= 5000U)
    {
        lastControlTime = now;
        App_EngineControl();
    }
}
