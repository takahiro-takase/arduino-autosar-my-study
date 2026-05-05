#include "Rte.h"
#include "Com.h"

/* millis() は Arduino の wiring.c（C リンケージ）で定義されている */
extern unsigned long millis(void);

#define RTE_SIGNAL_ENGINE_SPEED    ((Com_SignalIdType)0)
#define RTE_SIGNAL_COOLANT_TEMP    ((Com_SignalIdType)1)
#define RTE_SIGNAL_ENGINE_ON_FLAG  ((Com_SignalIdType)2)

/* SW-C の Runnable 宣言（App_EngineManager.c が定義） */
extern void App_EngineManager_Run(void);

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

Std_ReturnType Rte_TriggerTransmit(Com_IPduIdType IPduId)
{
    return Com_TriggerIPDUSend(IPduId);
}

void Rte_ScheduleRunnables(void)
{
    static unsigned long lastRunTime = 0UL;
    const unsigned long now = millis();

    if (now - lastRunTime >= 3000UL)
    {
        lastRunTime = now;
        App_EngineManager_Run();
    }
}
