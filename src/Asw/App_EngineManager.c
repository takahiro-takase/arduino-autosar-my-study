/**
 * \file    App_EngineManager.c
 * \brief   Engine Manager Application SW-Component
 * \details Implements the Engine Manager SW-C as an AUTOSAR-style Application
 *          Software Component (ASW). Contains one periodic Runnable Entity
 *          (App_EngineManager_Run) that reads sensor signals via RTE port
 *          accessors, evaluates the engine state machine, and writes the
 *          resulting state back to the CAN bus every 3 seconds.
 *
 *          State machine transitions:
 *            OFF      --[flag=1]-->          STARTING
 *            OFF      --[speed>0, flag=0]--> FAULT
 *            STARTING --[flag=0]-->          OFF
 *            STARTING --[speed>=500]-->      RUNNING
 *            STARTING --[timeout 5s]-->      FAULT
 *            RUNNING  --[flag=0]-->          OFF
 *            RUNNING  --[temp>=100]-->       FAULT
 *            RUNNING  --[speed<100]-->       FAULT
 *            FAULT    --[flag=0]-->          OFF
 */

#include "App_EngineManager.h"
#include "Rte.h"
#include "Det.h"

/* millis() is declared in Arduino wiring.c with C linkage. */
extern unsigned long millis(void);

#define ENGINE_SPEED_RUNNING_THRESHOLD  ((EngineSpeed_t)500U)
#define ENGINE_SPEED_STALL_THRESHOLD    ((EngineSpeed_t)100U)
#define COOLANT_OVERHEAT_THRESHOLD      ((CoolantTemp_t)100U)
#define STARTING_TIMEOUT_MS             (5000UL)

static EngineState_t  s_state           = ENGINE_STATE_OFF;
static unsigned long  s_startingEnterMs = 0UL;

static void State_Off(EngineSpeed_t speed, CoolantTemp_t temp, EngineOnFlag_t flag);
static void State_Starting(EngineSpeed_t speed, CoolantTemp_t temp, EngineOnFlag_t flag);
static void State_Running(EngineSpeed_t speed, CoolantTemp_t temp, EngineOnFlag_t flag);
static void State_Fault(EngineSpeed_t speed, CoolantTemp_t temp, EngineOnFlag_t flag);

/**
 * \brief   Initializes the Engine Manager SW-Component.
 *
 * \details Resets the engine state machine to ENGINE_STATE_OFF and clears the
 *          STARTING-state timeout reference. Must be called once during system
 *          initialization before the RTE scheduler starts invoking
 *          App_EngineManager_Run().
 *
 * \pre        RTE and all BSW modules must be initialized before this call.
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void App_EngineManager_Init(void)
{
    s_state           = ENGINE_STATE_OFF;
    s_startingEnterMs = 0UL;
    Det_LogP(PSTR("[EngineManager] Init->OFF"));
}

/**
 * \brief   Executes the Engine Manager periodic Runnable Entity.
 *
 * \details Reads the three RX signals (EngineSpeed, CoolantTemp, EngineOnFlag)
 *          from the RTE, delegates to the current state handler, then writes
 *          the updated EngineState signal and triggers CAN frame transmission
 *          (CAN ID 0x200, DLC 1). Invoked every 3000 ms by
 *          Rte_ScheduleRunnables().
 *
 *          This function is the sole Runnable Entity of the Engine Manager
 *          SW-C. In a full AUTOSAR OS environment it would be mapped to a
 *          periodic OsTask; here the period is enforced by the RTE scheduler.
 *
 * \pre        App_EngineManager_Init() must have been called successfully.
 * \note       RTE Read return values are discarded: if a signal is unavailable
 *             the previous buffer value (0 at startup) is used, which causes
 *             the state machine to remain in OFF.
 *
 * \ServiceID      {0xF0}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
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

    (void)Rte_Write_EngineStatus_EngineState(s_state);
    (void)Rte_TriggerTransmit(0U);
}

/**
 * \brief   Returns the current engine state.
 *
 * \details Provides read-only access to the internal engine state variable
 *          for external query (e.g., diagnostic or test purposes) without
 *          triggering a state transition.
 *
 * \return  Current EngineState_t value
 *          (ENGINE_STATE_OFF / STARTING / RUNNING / FAULT).
 *
 * \pre        App_EngineManager_Init() must have been called successfully.
 *
 * \ServiceID      {0xF1}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
EngineState_t App_EngineManager_GetState(void)
{
    return s_state;
}

/* -----------------------------------------------------------------------
 * Internal state handlers — called exclusively from App_EngineManager_Run
 * ----------------------------------------------------------------------- */

/**
 * \brief   Handles the ENGINE_STATE_OFF state.
 *
 * \details Transitions out of OFF when the EngineOnFlag rises:
 *          - flag=1            → STARTING (records entry timestamp)
 *          - speed>0 & flag=0  → FAULT    (speed without flag is abnormal)
 *
 * \param[in]  speed  Current engine speed (RPM).
 * \param[in]  temp   Current coolant temperature (°C). Unused in this state.
 * \param[in]  flag   EngineOnFlag: 1 = start requested, 0 = no request.
 *
 * \ServiceID      {0xF2}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
static void State_Off(EngineSpeed_t speed, CoolantTemp_t temp, EngineOnFlag_t flag)
{
    (void)temp;

    if (flag == 1U)
    {
        s_state           = ENGINE_STATE_STARTING;
        s_startingEnterMs = millis();
        Det_LogP(PSTR("[EngineManager] OFF->STARTING"));
    }
    else if (speed > 0U)
    {
        s_state = ENGINE_STATE_FAULT;
        Det_LogP(PSTR("[EngineManager] OFF->FAULT(spd w/o flag)"));
    }
}

/**
 * \brief   Handles the ENGINE_STATE_STARTING state.
 *
 * \details Monitors the cranking phase with a 5-second timeout:
 *          - flag=0                              → OFF    (start cancelled)
 *          - speed >= ENGINE_SPEED_RUNNING_THRESHOLD (500) → RUNNING
 *          - elapsed >= STARTING_TIMEOUT_MS (5000 ms)      → FAULT
 *
 * \param[in]  speed  Current engine speed (RPM).
 * \param[in]  temp   Current coolant temperature (°C). Unused in this state.
 * \param[in]  flag   EngineOnFlag: must remain 1 to stay in STARTING.
 *
 * \ServiceID      {0xF3}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
static void State_Starting(EngineSpeed_t speed, CoolantTemp_t temp, EngineOnFlag_t flag)
{
    (void)temp;

    if (flag == 0U)
    {
        s_state = ENGINE_STATE_OFF;
        Det_LogP(PSTR("[EngineManager] STARTING->OFF"));
        return;
    }
    if (speed >= ENGINE_SPEED_RUNNING_THRESHOLD)
    {
        s_state = ENGINE_STATE_RUNNING;
        Det_LogP(PSTR("[EngineManager] STARTING->RUNNING"));
        return;
    }
    if (millis() - s_startingEnterMs >= STARTING_TIMEOUT_MS)
    {
        s_state = ENGINE_STATE_FAULT;
        Det_LogP(PSTR("[EngineManager] STARTING->FAULT(timeout)"));
    }
}

/**
 * \brief   Handles the ENGINE_STATE_RUNNING state.
 *
 * \details Monitors for fault conditions while the engine runs normally:
 *          - flag=0                                     → OFF
 *          - temp >= COOLANT_OVERHEAT_THRESHOLD (100°C) → FAULT (overheat)
 *          - speed < ENGINE_SPEED_STALL_THRESHOLD (100) → FAULT (stall)
 *          Otherwise logs the current speed and temperature each cycle.
 *
 * \param[in]  speed  Current engine speed (RPM).
 * \param[in]  temp   Current coolant temperature (°C).
 * \param[in]  flag   EngineOnFlag: must remain 1 to stay in RUNNING.
 *
 * \ServiceID      {0xF4}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
static void State_Running(EngineSpeed_t speed, CoolantTemp_t temp, EngineOnFlag_t flag)
{
    if (flag == 0U)
    {
        s_state = ENGINE_STATE_OFF;
        Det_LogP(PSTR("[EngineManager] RUNNING->OFF"));
        return;
    }
    if (temp >= COOLANT_OVERHEAT_THRESHOLD)
    {
        s_state = ENGINE_STATE_FAULT;
        Det_PrintP(PSTR("[EngineManager] RUNNING->FAULT(overheat="));
        Det_PrintDec(temp);
        Det_LogP(PSTR(")"));
        return;
    }
    if (speed < ENGINE_SPEED_STALL_THRESHOLD)
    {
        s_state = ENGINE_STATE_FAULT;
        Det_PrintP(PSTR("[EngineManager] RUNNING->FAULT(stall="));
        Det_PrintDec(speed);
        Det_LogP(PSTR(")"));
        return;
    }

    Det_PrintP(PSTR("[EngineManager] RUNNING spd="));
    Det_PrintDec(speed);
    Det_PrintP(PSTR(" tmp="));
    Det_PrintDec(temp);
    Det_Newline();
}

/**
 * \brief   Handles the ENGINE_STATE_FAULT state.
 *
 * \details Holds the FAULT state until the operator clears the start request:
 *          - flag=0 → OFF (fault acknowledged, system reset)
 *          - flag=1 → remains FAULT (logs a waiting message each cycle)
 *          Speed and temperature are not evaluated in this state.
 *
 * \param[in]  speed  Current engine speed (RPM). Unused in this state.
 * \param[in]  temp   Current coolant temperature (°C). Unused in this state.
 * \param[in]  flag   EngineOnFlag: 0 clears the fault and returns to OFF.
 *
 * \ServiceID      {0xF5}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
static void State_Fault(EngineSpeed_t speed, CoolantTemp_t temp, EngineOnFlag_t flag)
{
    (void)speed;
    (void)temp;

    if (flag == 0U)
    {
        s_state = ENGINE_STATE_OFF;
        Det_LogP(PSTR("[EngineManager] FAULT->OFF"));
    }
    else
    {
        Det_LogP(PSTR("[EngineManager] FAULT(wait flag=0)"));
    }
}
