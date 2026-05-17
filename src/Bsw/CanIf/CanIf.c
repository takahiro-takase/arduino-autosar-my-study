/**
 * \file    CanIf.c
 * \brief   CAN Interface (AUTOSAR SWS_CANInterface inspired)
 * \details Implements the AUTOSAR CanIf API layer that sits between the CAN
 *          Driver (Can.c) and the upper communication layers (PduR, DCM).
 *          Conforms to the AUTOSAR 4.3.1 SWS_CANInterface specification where
 *          noted, with simplifications for Arduino UNO hardware.
 */

#include "CanIf.h"
#include "Can.h"
#include "Det.h"

static const CanIf_ConfigType* CanIf_ConfigPtr = NULL;

/**
 * \brief   Initializes the CAN Interface module.
 *
 * \details Stores the configuration pointer and logs the number of TX/RX PDUs
 *          configured (AUTOSAR SWS_CANIF_00001). Must be called once after
 *          Can_Init() and before any other CanIf_* API.
 *
 * \param[in]  ConfigPtr  Pointer to the CanIf configuration structure.
 *                        Must not be NULL.
 *
 * \pre        Can_Init() must have been called successfully.
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanIf_Init(const CanIf_ConfigType* ConfigPtr)
{
    CanIf_ConfigPtr = ConfigPtr;
    Det_LogP(PSTR("[CanIf_Init] initialized"));
    Det_PrintP(PSTR("  TX PDU count: "));
    Det_PrintDec(ConfigPtr->TxPduCount);
    Det_Newline();
    Det_PrintP(PSTR("  RX PDU count: "));
    Det_PrintDec(ConfigPtr->RxPduCount);
    Det_Newline();
}

/**
 * \brief   Requests transmission of a PDU via the CAN Driver.
 *
 * \details Looks up the TX PDU configuration by TxPduId, validates the PDU
 *          length against the configured DLC, builds a Can_PduType, and
 *          calls Can_Write() (AUTOSAR SWS_CANIF_00005).
 *
 * \param[in]  TxPduId     ID of the TX PDU to transmit. Must be less than
 *                         the configured TxPduCount.
 * \param[in]  PduInfoPtr  Pointer to the PDU data and length to transmit.
 *                         Must not be NULL; SduDataPtr must not be NULL.
 *
 * \retval  E_OK      PDU was accepted and passed to Can_Write() successfully.
 * \retval  E_NOT_OK  CanIf not initialized, invalid TxPduId, NULL pointer,
 *                    SduLength exceeds configured DLC, or Can_Write() failed.
 *
 * \pre        CanIf_Init() must have been called successfully.
 * \pre        The CAN controller must be in CAN_CS_STARTED state.
 *
 * \ServiceID      {0x49}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)
{
    if (CanIf_ConfigPtr == NULL)
        return E_NOT_OK;

    if (TxPduId >= CanIf_ConfigPtr->TxPduCount)
    {
        Det_LogP(PSTR("[CanIf_Transmit] E: invalid TxPduId"));
        return E_NOT_OK;
    }

    if (PduInfoPtr == NULL || PduInfoPtr->SduDataPtr == NULL)
    {
        Det_LogP(PSTR("[CanIf_Transmit] E: PduInfoPtr NULL"));
        return E_NOT_OK;
    }

    const CanIf_TxPduConfigType* txCfg = &CanIf_ConfigPtr->TxPduConfig[TxPduId];

    if (PduInfoPtr->SduLength > txCfg->Dlc)
    {
        Det_LogP(PSTR("[CanIf_Transmit] E: SduLength>DLC"));
        return E_NOT_OK;
    }

    Can_PduType canPdu = {
        .swPduHandle = TxPduId,
        .id          = txCfg->CanId,
        .length      = (uint8)PduInfoPtr->SduLength,
        .sdu         = PduInfoPtr->SduDataPtr
    };

    Det_PrintP(PSTR("[CanIf_Transmit] TxPduId="));
    Det_PrintDec(TxPduId);
    Det_PrintP(PSTR(" CanId=0x"));
    Det_PrintHex(txCfg->CanId);
    Det_Newline();

    Can_ReturnType ret = Can_Write(txCfg->Hth, &canPdu);

    if (ret == CAN_BUSY)
        Det_LogP(PSTR("[CanIf_Transmit] BUSY"));

    return (ret == CAN_OK) ? E_OK : E_NOT_OK;
}

/**
 * \brief   Indicates a received CAN frame from the CAN Driver to upper layers.
 *
 * \details Called by the CAN Driver when a frame is received. Searches the RX
 *          PDU table for a matching HOH and CAN ID, then dispatches the PDU to
 *          the configured upper-layer RxIndication callback
 *          (AUTOSAR SWS_CANIF_00415, SWS_CANInterface_00451).
 *
 * \param[in]  Mailbox     Pointer to the hardware mailbox descriptor containing
 *                         the received CAN ID, HOH, and controller ID.
 *                         Must not be NULL.
 * \param[in]  PduInfoPtr  Pointer to the received PDU data and length.
 *                         Must not be NULL.
 *
 * \pre        CanIf_Init() must have been called successfully.
 * \note       If no RX PDU configuration matches the received CAN ID and HOH,
 *             the frame is silently discarded and a log message is emitted.
 *
 * \ServiceID      {0x10}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanIf_RxIndication(const Can_HwType* Mailbox, const PduInfoType* PduInfoPtr)
{
    if (CanIf_ConfigPtr == NULL || Mailbox == NULL || PduInfoPtr == NULL)
        return;

    for (uint8 i = 0; i < CanIf_ConfigPtr->RxPduCount; i++)
    {
        const CanIf_RxPduConfigType* rxCfg = &CanIf_ConfigPtr->RxPduConfig[i];

        if (rxCfg->Hrh == Mailbox->Hoh && rxCfg->CanId == Mailbox->CanId)
        {
            Det_PrintP(PSTR("[CanIf_RxInd] CanId=0x"));
            Det_PrintHex(Mailbox->CanId);
            Det_PrintP(PSTR(" -> RxPduId="));
            Det_PrintDec(rxCfg->UpperLayerRxPduId);
            Det_Newline();

            if (rxCfg->RxIndicationFct != NULL)
                rxCfg->RxIndicationFct(rxCfg->UpperLayerRxPduId, PduInfoPtr);

            return;
        }
    }

    Det_PrintP(PSTR("[CanIf_RxInd] no match CanId=0x"));
    Det_PrintHex(Mailbox->CanId);
    Det_Newline();
}

/**
 * \brief   Confirms successful transmission of a CAN frame from the CAN Driver.
 *
 * \details Called by the CAN Driver after a frame has been successfully
 *          transmitted. Looks up the TX PDU configuration by CanTxPduId and
 *          invokes the configured upper-layer TxConfirmation callback
 *          (AUTOSAR SWS_CANIF_00011).
 *
 * \param[in]  CanTxPduId  ID of the successfully transmitted TX PDU.
 *                         Must be less than the configured TxPduCount.
 *
 * \pre        CanIf_Init() must have been called successfully.
 * \note       If CanTxPduId is out of range, the call is silently ignored.
 *
 * \ServiceID      {0x11}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanIf_TxConfirmation(PduIdType CanTxPduId)
{
    if (CanIf_ConfigPtr == NULL)
        return;

    if (CanTxPduId >= CanIf_ConfigPtr->TxPduCount)
        return;

    const CanIf_TxPduConfigType* txCfg = &CanIf_ConfigPtr->TxPduConfig[CanTxPduId];

    Det_PrintP(PSTR("[CanIf_TxConf] TxPduId="));
    Det_PrintDec(CanTxPduId);
    Det_Newline();

    if (txCfg->TxConfirmFct != NULL)
        txCfg->TxConfirmFct(txCfg->UpperLayerTxPduId, E_OK);
}
