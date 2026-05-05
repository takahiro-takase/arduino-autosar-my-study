#include <Arduino.h>
#include "App_EngineManager.h"
#include "Rte.h"

#define ENGINE_SPEED_RUNNING_THRESHOLD  ((EngineSpeed_t)500U)
#define ENGINE_SPEED_STALL_THRESHOLD    ((EngineSpeed_t)100U)
#define COOLANT_OVERHEAT_THRESHOLD      ((CoolantTemp_t)100U)
#define STARTING_TIMEOUT_MS             (5000UL)

static EngineState_t s_state           = ENGINE_STATE_OFF;
static uint32        s_startingEnterMs = 0U;

static void State_Off(EngineSpeed_t speed, CoolantTemp_t temp, EngineOnFlag_t flag);
static void State_Starting(EngineSpeed_t speed, CoolantTemp_t temp, EngineOnFlag_t flag);
static void State_Running(EngineSpeed_t speed, CoolantTemp_t temp, EngineOnFlag_t flag);
static void State_Fault(EngineSpeed_t speed, CoolantTemp_t temp, EngineOnFlag_t flag);

void App_EngineManager_Init(void)
{
    s_state           = ENGINE_STATE_OFF;
    s_startingEnterMs = 0U;
    Serial.println(F("[EngineManager] Init -> OFF"));
}

void App_EngineManager_Run(void)
{
    EngineSpeed_t  speed = 0U;
    CoolantTemp_t  temp  = 0U;
    EngineOnFlag_t flag  = 0U;

    (void)Rte_Read_SpeedSensor_EngineSpeed(&speed);
    (void)Rte_Read_TempSensor_CoolantTemp(&temp);
    (void)Rte_Read_EngineStatus_EngineOnFlag(&flag);

    switch (s_state)
    {
        case ENGINE_STATE_OFF:      State_Off(speed, temp, flag);      break;
        case ENGINE_STATE_STARTING: State_Starting(speed, temp, flag); break;
        case ENGINE_STATE_RUNNING:  State_Running(speed, temp, flag);  break;
        case ENGINE_STATE_FAULT:    State_Fault(speed, temp, flag);    break;
        default:                    s_state = ENGINE_STATE_FAULT;      break;
    }
}

EngineState_t App_EngineManager_GetState(void)
{
    return s_state;
}

static void State_Off(EngineSpeed_t speed, CoolantTemp_t temp, EngineOnFlag_t flag)
{
    (void)temp;

    if (flag == 1U)
    {
        s_state           = ENGINE_STATE_STARTING;
        s_startingEnterMs = (uint32)millis();
        Serial.println(F("[EngineManager] OFF->STARTING"));

        (void)Rte_Write_EngineCmd_EngineSpeed((EngineSpeed_t)0U);
        (void)Rte_Write_EngineCmd_CoolantTemp(temp);
        (void)Rte_Write_EngineCmd_EngineOnFlag(1U);
        (void)Rte_TriggerTransmit(0U);
    }
    else if (speed > 0U)
    {
        s_state = ENGINE_STATE_FAULT;
        Serial.println(F("[EngineManager] OFF->FAULT(speed w/o flag)"));
    }
}

static void State_Starting(EngineSpeed_t speed, CoolantTemp_t temp, EngineOnFlag_t flag)
{
    if (flag == 0U)
    {
        s_state = ENGINE_STATE_OFF;
        Serial.println(F("[EngineManager] STARTING->OFF"));
        return;
    }
    if (speed >= ENGINE_SPEED_RUNNING_THRESHOLD)
    {
        s_state = ENGINE_STATE_RUNNING;
        Serial.println(F("[EngineManager] STARTING->RUNNING"));
        return;
    }
    if ((uint32)millis() - s_startingEnterMs >= STARTING_TIMEOUT_MS)
    {
        s_state = ENGINE_STATE_FAULT;
        Serial.println(F("[EngineManager] STARTING->FAULT(timeout)"));
        return;
    }

    (void)Rte_Write_EngineCmd_EngineSpeed(speed);
    (void)Rte_Write_EngineCmd_CoolantTemp(temp);
    (void)Rte_Write_EngineCmd_EngineOnFlag(1U);
    (void)Rte_TriggerTransmit(0U);
}

static void State_Running(EngineSpeed_t speed, CoolantTemp_t temp, EngineOnFlag_t flag)
{
    if (flag == 0U)
    {
        s_state = ENGINE_STATE_OFF;
        Serial.println(F("[EngineManager] RUNNING->OFF"));
        return;
    }
    if (temp >= COOLANT_OVERHEAT_THRESHOLD)
    {
        s_state = ENGINE_STATE_FAULT;
        Serial.print(F("[EngineManager] RUNNING->FAULT(overheat="));
        Serial.print(temp);
        Serial.println(')');
        return;
    }
    if (speed < ENGINE_SPEED_STALL_THRESHOLD)
    {
        s_state = ENGINE_STATE_FAULT;
        Serial.print(F("[EngineManager] RUNNING->FAULT(stall="));
        Serial.print(speed);
        Serial.println(')');
        return;
    }

    (void)Rte_Write_EngineCmd_EngineSpeed(speed);
    (void)Rte_Write_EngineCmd_CoolantTemp(temp);
    (void)Rte_Write_EngineCmd_EngineOnFlag(1U);
    (void)Rte_TriggerTransmit(0U);

    Serial.print(F("[EngineManager] RUNNING spd="));
    Serial.print(speed);
    Serial.print(F(" tmp="));
    Serial.println(temp);
}

static void State_Fault(EngineSpeed_t speed, CoolantTemp_t temp, EngineOnFlag_t flag)
{
    (void)speed;
    (void)temp;

    if (flag == 0U)
    {
        s_state = ENGINE_STATE_OFF;
        Serial.println(F("[EngineManager] FAULT->OFF"));
    }
    else
    {
        Serial.println(F("[EngineManager] FAULT(wait flag=0)"));
    }
}
