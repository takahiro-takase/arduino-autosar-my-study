/**
 * \file    PduR.c
 * \brief   PDU Router (AUTOSAR SWS_PDURouter inspired)
 * \details Implements the AUTOSAR PduR routing layer that sits between CanIf
 *          and the upper communication modules (COM, DCM). Routes received PDUs
 *          to one or more upper-layer modules (multicast) and forwards transmit
 *          requests and confirmations between COM and CanIf.
 *          Conforms to the AUTOSAR 4.3.1 SWS_PDURouter specification where
 *          noted, with simplifications for Arduino UNO hardware.
 */

#include "PduR.h"
#include "CanIf.h"
#include "Det.h"

static const PduR_PBConfigType* PduR_ConfigPtr = NULL;

/**
 * \brief   Initializes the PDU Router module.
 *
 * \details Validates all RX routing paths and stores the post-build
 *          configuration pointer (AUTOSAR SWS_PduR_00119). Logs the configured
 *          RX/TX routing table entries for diagnostic purposes.
 *          Initialization is aborted if any RX path has no destinations.
 *
 * \param[in]  ConfigPtr  Pointer to the post-build PduR configuration.
 *                        Must not be NULL.
 *
 * \pre        CanIf_Init() must have been called successfully.
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void PduR_Init(const PduR_PBConfigType* ConfigPtr)
{
    if (ConfigPtr == NULL)
    {
        Det_LogP(PSTR("[PduR_Init] E: ConfigPtr NULL"));
        return;
    }

    for (uint8 i = 0; i < ConfigPtr->RxPathCount; i++)
    {
        if (ConfigPtr->RxPaths[i].Dests == NULL || ConfigPtr->RxPaths[i].DestCount == 0)
        {
            Det_PrintP(PSTR("[PduR_Init] E: RxPath["));
            Det_PrintDec(i);
            Det_LogP(PSTR("] no dests"));
            return;
        }
    }

    PduR_ConfigPtr = ConfigPtr;
    Det_LogP(PSTR("[PduR_Init] initialized"));

    Det_PrintP(PSTR("  RX paths: "));
    Det_PrintDec(ConfigPtr->RxPathCount);
    Det_Newline();
    for (uint8 i = 0; i < ConfigPtr->RxPathCount; i++)
    {
        const PduR_RxRoutingPathType* path = &ConfigPtr->RxPaths[i];
        Det_PrintP(PSTR("    SrcPduId="));
        Det_PrintDec(path->SrcPduId);
        Det_PrintP(PSTR(" dests="));
        Det_PrintDec(path->DestCount);
        Det_Newline();
        for (uint8 d = 0; d < path->DestCount; d++)
        {
            Det_PrintP(PSTR("      Module="));
            Det_PrintDec(path->Dests[d].Module);
            Det_PrintP(PSTR(" DestPduId="));
            Det_PrintDec(path->Dests[d].DestPduId);
            Det_Newline();
        }
    }

    Det_PrintP(PSTR("  TX paths: "));
    Det_PrintDec(ConfigPtr->TxPathCount);
    Det_Newline();
    for (uint8 i = 0; i < ConfigPtr->TxPathCount; i++)
    {
        const PduR_TxRoutingPathType* path = &ConfigPtr->TxPaths[i];
        Det_PrintP(PSTR("    SrcPduId="));
        Det_PrintDec(path->SrcPduId);
        Det_PrintP(PSTR(" -> CanIfTxPduId="));
        Det_PrintDec(path->CanIfTxPduId);
        Det_Newline();
    }
}

/**
 * \brief   Routes a received PDU from CanIf to all configured upper layers.
 *
 * \details Called indirectly by CanIf via the macro
 *          `#define PduR_CanIfRxIndication PduR_ComRxIndication`
 *          defined in PduR_CanIf.h (AUTOSAR SWS_PduR_00369).
 *          Searches the RX routing table for a path matching RxPduId and
 *          invokes every configured destination's RxIndFct callback
 *          (multicast). If no matching path is found, the PDU is discarded.
 *
 * \param[in]  RxPduId     Source PDU ID assigned by CanIf. Used to look up
 *                         the RX routing path.
 * \param[in]  PduInfoPtr  Pointer to the received PDU data and length.
 *                         Must not be NULL.
 *
 * \pre        PduR_Init() must have been called successfully.
 * \note       Invoked via #define alias PduR_CanIfRxIndication; do not call
 *             PduR_ComRxIndication directly from application code.
 *
 * \ServiceID      {0x10}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void PduR_ComRxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)
{
    if (PduR_ConfigPtr == NULL || PduInfoPtr == NULL)
        return;

    for (uint8 i = 0; i < PduR_ConfigPtr->RxPathCount; i++)
    {
        const PduR_RxRoutingPathType* path = &PduR_ConfigPtr->RxPaths[i];

        if (path->SrcPduId != RxPduId)
            continue;

        for (uint8 d = 0; d < path->DestCount; d++)
        {
            const PduR_RxDestType* dest = &path->Dests[d];

            Det_PrintP(PSTR("[PduR_RxInd] Src="));
            Det_PrintDec(RxPduId);
            Det_PrintP(PSTR(" Mod="));
            Det_PrintDec(dest->Module);
            Det_PrintP(PSTR(" Dst="));
            Det_PrintDec(dest->DestPduId);
            Det_Newline();

            if (dest->RxIndFct != NULL)
                dest->RxIndFct(dest->DestPduId, PduInfoPtr);
        }
        return;
    }

    Det_PrintP(PSTR("[PduR_RxInd] no route Src="));
    Det_PrintDec(RxPduId);
    Det_Newline();
}

/**
 * \brief   Forwards a TX confirmation from CanIf to the upper layer.
 *
 * \details Called by CanIf after a CAN frame has been successfully transmitted.
 *          Searches the TX routing table for a path matching TxPduId and
 *          invokes the configured upper-layer confirmation callback
 *          (AUTOSAR SWS_PduR_00365).
 *
 * \param[in]  TxPduId  PDU ID of the confirmed transmission, as assigned by
 *                      CanIf. Used to look up the TX routing path.
 * \param[in]  result   Transmission result from CanIf.
 *                      E_OK = success, E_NOT_OK = failure.
 *                      Currently unused; reserved for future TP support.
 *
 * \pre        PduR_Init() must have been called successfully.
 * \note       The result parameter is accepted but not forwarded to the upper
 *             layer because Com_TxConfirmation() does not take a result
 *             argument per SWS_COM specification.
 *
 * \ServiceID      {0x11}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void PduR_CanIfTxConfirmation(PduIdType TxPduId, Std_ReturnType result)
{
    (void)result;

    if (PduR_ConfigPtr == NULL)
        return;

    for (uint8 i = 0; i < PduR_ConfigPtr->TxPathCount; i++)
    {
        const PduR_TxRoutingPathType* path = &PduR_ConfigPtr->TxPaths[i];

        if (path->SrcPduId != TxPduId)
            continue;

        Det_PrintP(PSTR("[PduR_TxConf] Src="));
        Det_PrintDec(TxPduId);
        Det_PrintP(PSTR(" ConfDst="));
        Det_PrintDec(path->ConfDestPduId);
        Det_Newline();

        if (path->ConfFct != NULL)
            path->ConfFct(path->ConfDestPduId);

        return;
    }

    Det_PrintP(PSTR("[PduR_TxConf] no route Src="));
    Det_PrintDec(TxPduId);
    Det_Newline();
}

/**
 * \brief   Requests transmission of a PDU from an upper layer via CanIf.
 *
 * \details Called by COM or other upper layers to transmit a PDU. Searches
 *          the TX routing table for a path matching SrcPduId and forwards
 *          the request to CanIf_Transmit() (AUTOSAR SWS_PduR_00109).
 *          Returns E_NOT_OK immediately if no matching path exists.
 *
 * \param[in]  SrcPduId    PDU ID of the upper-layer PDU to transmit. Used to
 *                         look up the TX routing path.
 * \param[in]  PduInfoPtr  Pointer to the PDU data and length to transmit.
 *                         Must not be NULL.
 *
 * \retval  E_OK      PDU was forwarded to CanIf_Transmit() successfully.
 * \retval  E_NOT_OK  PduR not initialized, PduInfoPtr is NULL, no matching
 *                    TX routing path found, or CanIf_Transmit() failed.
 *
 * \pre        PduR_Init() must have been called successfully.
 * \pre        The CAN controller must be in CAN_CS_STARTED state.
 *
 * \ServiceID      {0x49}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType PduR_Transmit(PduIdType SrcPduId, const PduInfoType* PduInfoPtr)
{
    if (PduR_ConfigPtr == NULL || PduInfoPtr == NULL)
        return E_NOT_OK;

    for (uint8 i = 0; i < PduR_ConfigPtr->TxPathCount; i++)
    {
        const PduR_TxRoutingPathType* path = &PduR_ConfigPtr->TxPaths[i];

        if (path->SrcPduId != SrcPduId)
            continue;

        Det_PrintP(PSTR("[PduR_Transmit] Src="));
        Det_PrintDec(SrcPduId);
        Det_PrintP(PSTR(" -> CanIf="));
        Det_PrintDec(path->CanIfTxPduId);
        Det_Newline();

        return CanIf_Transmit(path->CanIfTxPduId, PduInfoPtr);
    }

    Det_PrintP(PSTR("[PduR_Transmit] no route Src="));
    Det_PrintDec(SrcPduId);
    Det_Newline();
    return E_NOT_OK;
}
