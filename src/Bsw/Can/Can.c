/**
 * \file    Can.c
 * \brief   CAN Driver (AUTOSAR SWS_Can inspired)
 * \details Implements the AUTOSAR CanDrv API layer using MCP2515 via
 *          Mcp2515_Wrapper. Conforms to the AUTOSAR 4.x SWS_Can specification
 *          where noted, with simplifications for Arduino UNO hardware.
 */

#include "Can.h"
#include "Mcp2515_Wrapper.h"
#include "CanIf.h"
#include "Det.h"

/* Arduino wiring.c (C linkage) */
extern int digitalRead(uint8 pin);

static const Can_ConfigType*   Can_ConfigPtr = NULL;
static Can_ControllerStateType CanState      = CAN_CS_UNINIT;

/**
 * \brief   Initializes the CAN driver.
 *
 * \details Initializes the MCP2515 hardware, configures all hardware
 *          acceptance filters/masks, and places the controller in
 *          CAN_CS_STOPPED state (AUTOSAR SWS_Can_00246).
 *          Halts execution on initialization failure.
 *
 * \param[in]  Config  Pointer to the CAN driver configuration structure.
 *                     Must not be NULL.
 *
 * \pre        SPI peripheral must be initialized before this call.
 * \note       Must be called once at system startup before any other
 *             Can_* API.
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Can_Init(const Can_ConfigType* Config)
{
    Det_LogP(PSTR("[Can_Init] Initializing CAN..."));

    Can_ConfigPtr = Config;

    if (Mcp2515_Init(Config->csPin, Config->baudrate, Config->crystalFreq) != MCP2515_WRAPPER_OK)
    {
        Det_LogP(PSTR("[Can_Init] FAIL"));
        while (1)
            ;
    }

    Det_LogP(PSTR("[Can_Init] CAN Initialized successfully"));

    Mcp2515_InitMask(0, 0, Config->filter.mask << 16);
    Mcp2515_InitFilter(0, 0, Config->filter.filterId << 16);
    Mcp2515_InitFilter(1, 0, Config->filter.filterId << 16);
    Mcp2515_InitMask(1, 0, Config->filter.mask << 16);
    Mcp2515_InitFilter(2, 0, Config->filter.filterId << 16);
    Mcp2515_InitFilter(3, 0, Config->filter.filterId << 16);
    Mcp2515_InitFilter(4, 0, Config->filter.filterId << 16);
    Mcp2515_InitFilter(5, 0, Config->filter.filterId << 16);

    Mcp2515_SetMode(MCP2515_MODE_LISTEN_ONLY);
    CanState = CAN_CS_STOPPED;
}

/**
 * \brief   Performs a CAN controller state transition.
 *
 * \details Maps the AUTOSAR state transition to the corresponding MCP2515
 *          operating mode (AUTOSAR SWS_Can_00017, SWS_Can_00230):
 *          - CAN_T_START  : CAN_CS_STOPPED -> CAN_CS_STARTED (MCP_NORMAL)
 *          - CAN_T_STOP   : CAN_CS_STARTED -> CAN_CS_STOPPED (MCP_LISTENONLY)
 *          - CAN_T_SLEEP  : CAN_CS_STOPPED -> CAN_CS_SLEEP   (MCP_SLEEP)
 *          - CAN_T_WAKEUP : CAN_CS_SLEEP   -> CAN_CS_STOPPED (MCP_LISTENONLY)
 *
 * \param[in]  Controller  CAN controller index. Only controller 0 is present;
 *                         other values return CAN_NOT_OK.
 * \param[in]  Transition  Requested state transition (Can_StateTransitionType).
 *
 * \return  CAN_OK      Transition applied successfully.
 * \return  CAN_NOT_OK  Invalid Controller index or unsupported Transition value.
 *
 * \ServiceID      {0x03}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Can_ReturnType Can_SetControllerMode(uint8 Controller, Can_StateTransitionType Transition)
{
    if (Controller != 0U)
        return CAN_NOT_OK;

    switch (Transition)
    {
    case CAN_T_START:
        Mcp2515_SetMode(MCP2515_MODE_NORMAL);
        CanState = CAN_CS_STARTED;
        break;
    case CAN_T_STOP:
        Mcp2515_SetMode(MCP2515_MODE_LISTEN_ONLY);
        CanState = CAN_CS_STOPPED;
        break;
    case CAN_T_SLEEP:
        Mcp2515_SetMode(MCP2515_MODE_SLEEP);
        CanState = CAN_CS_SLEEP;
        break;
    case CAN_T_WAKEUP:
        Mcp2515_SetMode(MCP2515_MODE_LISTEN_ONLY);
        CanState = CAN_CS_STOPPED;
        break;
    default:
        return CAN_NOT_OK;
    }

    return CAN_OK;
}

/**
 * \brief   Requests transmission of a CAN frame.
 *
 * \details Passes the PDU to the MCP2515 transmit buffer and notifies
 *          CanIf via CanIf_TxConfirmation on success (AUTOSAR SWS_Can_00016).
 *          Returns CAN_NOT_OK immediately if the controller is not in
 *          CAN_CS_STARTED state.
 *
 * \param[in]  Hth      Hardware transmit handle. Ignored in this implementation
 *                      because MCP2515 selects the TX buffer automatically.
 * \param[in]  PduInfo  Pointer to the PDU to transmit. Must not be NULL.
 *                      Members used: id, length, sdu, swPduHandle.
 *
 * \return  CAN_OK      Frame was accepted and transmitted successfully.
 * \return  CAN_NOT_OK  Controller not started, or MCP2515 transmission failed.
 * \return  CAN_BUSY    Not returned by this implementation (MCP2515 auto-retry).
 *
 * \ServiceID      {0x06}
 * \Reentrancy     {Reentrant (different Hth)}
 * \Synchronicity  {Synchronous}
 */
Can_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType* PduInfo)
{
    (void)Hth;

    if (CanState != CAN_CS_STARTED)
        return CAN_NOT_OK;

    if (Mcp2515_Send(PduInfo->id, PduInfo->length, PduInfo->sdu) != MCP2515_WRAPPER_OK)
        return CAN_NOT_OK;

    Det_PrintP(PSTR("[Can_Write] Sent ID=0x"));
    Det_PrintHex(PduInfo->id);
    Det_PrintP(PSTR(" Data="));
    for (int i = 0; i < PduInfo->length; i++)
    {
        Det_PrintHex(PduInfo->sdu[i]);
        Det_PrintP(PSTR(" "));
    }
    Det_Newline();

    CanIf_TxConfirmation(PduInfo->swPduHandle);

    return CAN_OK;
}

/**
 * \brief   Polling-based receive processing function.
 *
 * \details Checks the MCP2515 receive status register and, if a frame is
 *          available, reads it and notifies CanIf via CanIf_RxIndication
 *          (AUTOSAR SWS_Can_00108). Intended to be called periodically
 *          from the main loop or a task scheduler.
 *
 * \pre        Can_Init() must have been called successfully.
 * \pre        Controller must be in CAN_CS_STARTED state.
 *
 * \ServiceID      {0x08}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Can_MainFunction_Read(void)
{
    if (CanState != CAN_CS_STARTED)
        return;

    if (Mcp2515_CheckReceive() == MCP2515_WRAPPER_OK)
    {
        uint32 rxId;
        uint8  len;
        uint8  buf[8];

        if (Mcp2515_Read(&rxId, &len, buf) == MCP2515_WRAPPER_OK)
        {
            CanIf_RxIndication(0, rxId, len, buf);
        }
    }
}

/**
 * \brief   Simulated interrupt service routine for MCP2515 INT pin.
 *
 * \details Polls the MCP2515 INT pin (active LOW). When asserted, reads the
 *          received frame and notifies CanIf via CanIf_RxIndication
 *          (AUTOSAR SWS_Can_00396). Called from the Arduino main loop,
 *          replacing a hardware ISR attachment.
 *
 * \note       Non-standard AUTOSAR API. In a full AUTOSAR OS environment this
 *             would be an ISR category 2 handler registered via the OS.
 *             The INT pin number is taken from Can_ConfigType::intPin.
 *
 * \pre        Can_Init() must have been called successfully.
 *
 * \ServiceID      {0xF0}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Can_Isr(void)
{
    if (Can_ConfigPtr == NULL)
        return;

    if (!digitalRead(Can_ConfigPtr->intPin))
    {
        uint32 rxId;
        uint8  len;
        uint8  buf[8];

        if (Mcp2515_Read(&rxId, &len, buf) == MCP2515_WRAPPER_OK)
        {
            CanIf_RxIndication(0, rxId, len, buf);
        }
    }
}
