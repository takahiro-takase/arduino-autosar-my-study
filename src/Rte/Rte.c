/**
 * \file    Rte.c
 * \brief   Runtime Environment (AUTOSAR SWS_RTE inspired)
 * \details Implements the AUTOSAR RTE API layer that mediates data exchange
 *          between SW-Components (SW-C) and the Basic Software (BSW) COM module.
 *          Provides typed Read/Write port accessors and a simple round-robin
 *          Runnable scheduler driven by the Arduino millis() timer.
 *          Conforms to the AUTOSAR 4.3.1 SWS_RTE API naming convention
 *          (Rte_Read_<p>_<o> / Rte_Write_<p>_<o>). The optional Rte_Instance
 *          and Rte_TransformerError parameters are omitted because only a
 *          single-instance SW-C without transformer chains is used.
 */

#include "Rte.h"
#include "Com.h"

extern unsigned long millis(void);

#define RTE_SIGNAL_ENGINE_SPEED    ((Com_SignalIdType)0)
#define RTE_SIGNAL_COOLANT_TEMP    ((Com_SignalIdType)1)
#define RTE_SIGNAL_ENGINE_ON_FLAG  ((Com_SignalIdType)2)
#define RTE_SIGNAL_ENGINE_STATE    ((Com_SignalIdType)3)

extern void App_EngineManager_Run(void);

/**
 * \brief   Reads the EngineSpeed signal from the SpeedSensor required port.
 *
 * \details Delegates to Com_ReceiveSignal() to unpack the EngineSpeed signal
 *          from the COM RX I-PDU buffer into the caller's variable
 *          (AUTOSAR SWS_RTE Rte_Read_<p>_<o> pattern).
 *
 * \param[out] data  Pointer to the variable that receives the engine speed
 *                   value. Must not be NULL. Populated only when E_OK is
 *                   returned.
 *
 * \retval  E_OK      Signal was successfully read from the COM buffer.
 * \retval  E_NOT_OK  COM not initialized or SignalId not found.
 *
 * \pre        Com_Init() and Com_RxIndication() must have been called.
 *
 * \ServiceID      {0xF2}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_Read_SpeedSensor_EngineSpeed(EngineSpeed_t* data)
{
    return Com_ReceiveSignal(RTE_SIGNAL_ENGINE_SPEED, data);
}

/**
 * \brief   Reads the CoolantTemp signal from the TempSensor required port.
 *
 * \details Delegates to Com_ReceiveSignal() to unpack the CoolantTemp signal
 *          from the COM RX I-PDU buffer into the caller's variable
 *          (AUTOSAR SWS_RTE Rte_Read_<p>_<o> pattern).
 *
 * \param[out] data  Pointer to the variable that receives the coolant
 *                   temperature value. Must not be NULL.
 *
 * \retval  E_OK      Signal was successfully read from the COM buffer.
 * \retval  E_NOT_OK  COM not initialized or SignalId not found.
 *
 * \pre        Com_Init() and Com_RxIndication() must have been called.
 *
 * \ServiceID      {0xF3}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_Read_TempSensor_CoolantTemp(CoolantTemp_t* data)
{
    return Com_ReceiveSignal(RTE_SIGNAL_COOLANT_TEMP, data);
}

/**
 * \brief   Reads the EngineOnFlag signal from the EngineStatus required port.
 *
 * \details Delegates to Com_ReceiveSignal() to unpack the EngineOnFlag signal
 *          from the COM RX I-PDU buffer into the caller's variable
 *          (AUTOSAR SWS_RTE Rte_Read_<p>_<o> pattern).
 *
 * \param[out] data  Pointer to the variable that receives the engine-on flag
 *                   value (0 = off, 1 = on). Must not be NULL.
 *
 * \retval  E_OK      Signal was successfully read from the COM buffer.
 * \retval  E_NOT_OK  COM not initialized or SignalId not found.
 *
 * \pre        Com_Init() and Com_RxIndication() must have been called.
 *
 * \ServiceID      {0xF4}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_Read_EngineStatus_EngineOnFlag(EngineOnFlag_t* data)
{
    return Com_ReceiveSignal(RTE_SIGNAL_ENGINE_ON_FLAG, data);
}

/**
 * \brief   Writes the EngineState signal to the EngineStatus provided port.
 *
 * \details Packs the EngineState value into the COM TX I-PDU buffer via
 *          Com_SendSignal() (AUTOSAR SWS_RTE Rte_Write_<p>_<o> pattern).
 *          The I-PDU is not transmitted immediately; call
 *          Rte_TriggerTransmit() afterward to trigger CAN frame transmission.
 *
 * \param[in]  state  Engine state to write (OFF / STARTING / RUNNING / FAULT).
 *
 * \retval  E_OK      Signal was successfully packed into the COM TX buffer.
 * \retval  E_NOT_OK  COM not initialized or SignalId not found.
 *
 * \pre        Com_Init() must have been called successfully.
 *
 * \ServiceID      {0xF5}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_Write_EngineStatus_EngineState(EngineState_t state)
{
    uint8 val = (uint8)state;
    return Com_SendSignal(RTE_SIGNAL_ENGINE_STATE, &val);
}

/**
 * \brief   Triggers immediate transmission of a COM TX I-PDU.
 *
 * \details Wraps Com_TriggerIPDUSend() to allow SW-Cs to request CAN frame
 *          transmission without a direct dependency on the COM module.
 *          This function is a project-specific RTE extension; it does not
 *          correspond to the AUTOSAR standard Rte_Trigger_<p>_<o> API
 *          (which targets internal SW-C event triggers).
 *
 * \param[in]  IPduId  COM I-PDU handle of the I-PDU to transmit.
 *
 * \retval  E_OK      I-PDU was successfully forwarded to PduR and CanIf.
 * \retval  E_NOT_OK  COM not initialized, I-PDU not found, or lower-layer
 *                    transmission failed.
 *
 * \pre        Com_Init() must have been called successfully.
 * \pre        Rte_Write_EngineStatus_EngineState() must have been called to
 *             populate the TX buffer before triggering transmission.
 * \note       Non-standard AUTOSAR API. Added to decouple App_EngineManager
 *             from direct COM calls.
 *
 * \ServiceID      {0xF6}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Rte_TriggerTransmit(Com_IPduIdType IPduId)
{
    return Com_TriggerIPDUSend(IPduId);
}

/**
 * \brief   Invokes all scheduled SW-C Runnables based on elapsed time.
 *
 * \details Checks the Arduino millis() counter and calls
 *          App_EngineManager_Run() every 3000 ms. This replaces the AUTOSAR
 *          OS periodic task scheduling mechanism with a simple polling loop
 *          suitable for the Arduino UNO bare-metal environment.
 *          Must be called from the Arduino loop() function.
 *
 * \pre        Arduino runtime must be initialized (setup() must have returned).
 * \note       Non-standard AUTOSAR API. In a full AUTOSAR OS environment,
 *             Runnables would be triggered by OsTask activations at configured
 *             periods; here a single 3-second period is hard-coded.
 *
 * \ServiceID      {0xF7}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
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
