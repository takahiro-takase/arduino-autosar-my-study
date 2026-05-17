/**
 * \file    Com.c
 * \brief   Communication Manager (AUTOSAR SWS_COM inspired)
 * \details Implements the AUTOSAR COM API layer for signal-based communication.
 *          Manages RX/TX I-PDU buffers and performs signal packing/unpacking
 *          with configurable bit endianness (Motorola/Intel).
 *          Conforms to the AUTOSAR 4.3.1 SWS_COM specification where noted,
 *          with simplifications for Arduino UNO hardware (fixed buffer counts,
 *          no deadline monitoring, no update bits).
 */

#include "Com.h"
#include "PduR.h"
#include "Det.h"

#define COM_IPDU_MAX_DLC  8U
#define COM_RX_IPDU_MAX   1U
#define COM_TX_IPDU_MAX   1U

static const Com_ConfigType* Com_ConfigPtr = NULL;
static uint8 Com_RxBuffer[COM_RX_IPDU_MAX][COM_IPDU_MAX_DLC];
static uint8 Com_TxBuffer[COM_TX_IPDU_MAX][COM_IPDU_MAX_DLC];

/**
 * \brief   Initializes the COM module and clears all I-PDU buffers.
 *
 * \details Stores the configuration pointer, zero-initializes all RX and TX
 *          I-PDU buffers, and logs the signal configuration table
 *          (AUTOSAR SWS_Com_00864). Initialization is aborted if the
 *          RX or TX I-PDU count exceeds the compiled-in maximum.
 *
 * \param[in]  config  Pointer to the COM configuration structure.
 *                     Must not be NULL.
 *
 * \pre        PduR_Init() must have been called successfully.
 * \note       COM_RX_IPDU_MAX and COM_TX_IPDU_MAX are compile-time constants
 *             fixed at 1. Configurations with more I-PDUs are rejected.
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Com_Init(const Com_ConfigType* config)
{
    if (config == NULL)
    {
        Det_LogP(PSTR("[Com_Init] E: config NULL"));
        return;
    }
    if (config->RxIPduCount > COM_RX_IPDU_MAX)
    {
        Det_LogP(PSTR("[Com_Init] E: RxIPduCount>max"));
        return;
    }
    if (config->TxIPduCount > COM_TX_IPDU_MAX)
    {
        Det_LogP(PSTR("[Com_Init] E: TxIPduCount>max"));
        return;
    }

    Com_ConfigPtr = config;

    for (uint8 i = 0; i < COM_RX_IPDU_MAX; i++)
        for (uint8 j = 0; j < COM_IPDU_MAX_DLC; j++)
            Com_RxBuffer[i][j] = 0U;

    for (uint8 i = 0; i < COM_TX_IPDU_MAX; i++)
        for (uint8 j = 0; j < COM_IPDU_MAX_DLC; j++)
            Com_TxBuffer[i][j] = 0U;

    Det_LogP(PSTR("[Com_Init] OK"));

    Det_PrintP(PSTR("  RxIPdus="));
    Det_PrintDec(config->RxIPduCount);
    Det_PrintP(PSTR(" TxIPdus="));
    Det_PrintDec(config->TxIPduCount);
    Det_PrintP(PSTR(" Signals="));
    Det_PrintDec(config->SignalCount);
    Det_Newline();

    for (uint8 s = 0; s < config->SignalCount; s++)
    {
        const Com_SignalConfigType* sig = &config->Signals[s];
        Det_PrintP(PSTR("  Sig["));
        Det_PrintDec(sig->SignalId);
        Det_PrintP(PSTR("] IPdu="));
        Det_PrintDec(sig->IPduId);
        Det_PrintP(PSTR(" Bit="));
        Det_PrintDec(sig->BitPosition);
        Det_PrintP(PSTR("/"));
        Det_PrintDec(sig->BitSize);
        Det_PrintP(PSTR(" "));
        Det_LogP(sig->Endian == COM_BIG_ENDIAN ? PSTR("BIG") : PSTR("LITTLE"));
    }
}

/**
 * \brief   Copies a received I-PDU payload into the internal RX buffer.
 *
 * \details Called by PduR when a CAN frame is received. Searches the RX I-PDU
 *          table for an entry matching RxPduId (PduR namespace), copies the
 *          payload bytes into the corresponding RX buffer slot, and logs the
 *          raw data (AUTOSAR SWS_Com_00442).
 *          After this call, signal values are accessible via Com_ReceiveSignal().
 *
 * \param[in]  RxPduId     PduR-layer PDU ID of the received I-PDU. Used to
 *                         look up the matching Com_IPduConfigType entry.
 * \param[in]  PduInfoPtr  Pointer to the received PDU data and length.
 *                         Must not be NULL.
 *
 * \pre        Com_Init() must have been called successfully.
 *
 * \ServiceID      {0x10}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Com_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)
{
    if (Com_ConfigPtr == NULL || PduInfoPtr == NULL)
        return;

    for (uint8 i = 0; i < Com_ConfigPtr->RxIPduCount; i++)
    {
        const Com_IPduConfigType* ipdu = &Com_ConfigPtr->RxIPdus[i];
        if (ipdu->PduRId != RxPduId)
            continue;

        const uint8 copyLen = (PduInfoPtr->SduLength < ipdu->DLC)
                              ? (uint8)PduInfoPtr->SduLength : ipdu->DLC;

        for (uint8 b = 0; b < copyLen; b++)
            Com_RxBuffer[ipdu->IPduId][b] = PduInfoPtr->SduDataPtr[b];

        Det_PrintP(PSTR("[Com_RxInd] IPdu="));
        Det_PrintDec(ipdu->IPduId);
        Det_PrintP(PSTR(" raw=["));
        for (uint8 b = 0; b < copyLen; b++)
        {
            if (b > 0U) Det_Print(" ");
            if (Com_RxBuffer[ipdu->IPduId][b] < 0x10U) Det_Print("0");
            Det_PrintHex(Com_RxBuffer[ipdu->IPduId][b]);
        }
        Det_LogP(PSTR("]"));
        return;
    }

    Det_PrintP(PSTR("[Com_RxInd] no IPdu PduRId="));
    Det_PrintDec(RxPduId);
    Det_Newline();
}

/* -----------------------------------------------------------------------
 * Internal helpers — not part of the public AUTOSAR COM API
 * ----------------------------------------------------------------------- */

/**
 * \brief   Extracts a bit field from a byte buffer using network bit ordering.
 *
 * \details Bit numbering: bit 0 = MSB of byte[0], bit 7 = LSB of byte[0],
 *          bit 8 = MSB of byte[1], ... (network/Motorola convention).
 *          For COM_BIG_ENDIAN the first bit read becomes the MSB of the
 *          result; for COM_LITTLE_ENDIAN the first bit read is the LSB.
 *
 * \param[in]  buf      Byte buffer to read from.
 * \param[in]  bitPos   Start bit position (network bit order).
 * \param[in]  bitSize  Number of bits to extract (1–32).
 * \param[in]  endian   Bit significance order (COM_BIG_ENDIAN / COM_LITTLE_ENDIAN).
 *
 * \return  Unpacked signal value as uint32.
 *
 * \ServiceID      {0xF0}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
static uint32 Com_UnpackSignal(const uint8* buf,
                                uint8 bitPos,
                                uint8 bitSize,
                                Com_SignalEndianType endian)
{
    uint32 value = 0U;
    for (uint8 i = 0; i < bitSize; i++)
    {
        const uint8 pos = bitPos + i;
        const uint8 bit = (buf[pos / 8U] >> (7U - (pos % 8U))) & 1U;
        if (endian == COM_BIG_ENDIAN)
            value = (value << 1U) | bit;
        else
            value |= ((uint32)bit << i);
    }
    return value;
}

/**
 * \brief   Packs a value into a bit field of a byte buffer using network bit ordering.
 *
 * \details Writes bitSize bits of value into buf starting at bitPos using the
 *          same network bit numbering as Com_UnpackSignal(). Only the target
 *          bits are modified; all other bits in buf are preserved.
 *
 * \param[in,out] buf      Byte buffer to write into.
 * \param[in]     bitPos   Start bit position (network bit order).
 * \param[in]     bitSize  Number of bits to write (1–32).
 * \param[in]     endian   Bit significance order (COM_BIG_ENDIAN / COM_LITTLE_ENDIAN).
 * \param[in]     value    Signal value to pack; only the lower bitSize bits are used.
 *
 * \ServiceID      {0xF1}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
static void Com_PackSignal(uint8* buf,
                            uint8 bitPos,
                            uint8 bitSize,
                            Com_SignalEndianType endian,
                            uint32 value)
{
    for (uint8 i = 0; i < bitSize; i++)
    {
        const uint8 bit   = (endian == COM_BIG_ENDIAN)
                            ? (uint8)((value >> (bitSize - 1U - i)) & 1U)
                            : (uint8)((value >> i) & 1U);
        const uint8 pos   = bitPos + i;
        const uint8 shift = 7U - (pos % 8U);
        if (bit)
            buf[pos / 8U] |=  (uint8)(1U << shift);
        else
            buf[pos / 8U] &= (uint8)~(1U << shift);
    }
}

/**
 * \brief   Extracts a signal value from the RX I-PDU buffer.
 *
 * \details Unpacks the signal identified by SignalId from the internal RX
 *          buffer using the bit position, size, and endianness defined in the
 *          signal configuration table (AUTOSAR SWS_Com_00194).
 *          The unpacked value is always written as a 4-byte little-endian
 *          integer into SignalDataPtr regardless of BitSize.
 *
 * \param[in]  SignalId      ID of the signal to read. Must match an entry
 *                           in the signal configuration table.
 * \param[out] SignalDataPtr Pointer to the output buffer. Must be at least
 *                           4 bytes. Written as uint32 in little-endian order.
 *                           Must not be NULL.
 *
 * \retval  E_OK      Signal was found and value was written to SignalDataPtr.
 * \retval  E_NOT_OK  COM not initialized, SignalDataPtr is NULL, or SignalId
 *                    not found in the configuration table.
 *
 * \pre        Com_Init() must have been called successfully.
 * \pre        Com_RxIndication() must have been called at least once for the
 *             I-PDU containing this signal.
 * \note       Return type is uint8 per SWS_Com_00194. E_OK / E_NOT_OK values
 *             (0x00 / 0x01) are compatible with Std_ReturnType used by the RTE.
 *
 * \ServiceID      {0x0B}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
uint8 Com_ReceiveSignal(Com_SignalIdType SignalId, void* SignalDataPtr)
{
    if (Com_ConfigPtr == NULL || SignalDataPtr == NULL)
        return E_NOT_OK;

    uint8* dataPtr = (uint8*)SignalDataPtr;

    for (uint8 s = 0; s < Com_ConfigPtr->SignalCount; s++)
    {
        const Com_SignalConfigType* sig = &Com_ConfigPtr->Signals[s];
        if (sig->SignalId != SignalId)
            continue;

        const uint32 value = Com_UnpackSignal(
            Com_RxBuffer[sig->IPduId],
            sig->BitPosition, sig->BitSize, sig->Endian);

        dataPtr[0] = (uint8)(value);
        dataPtr[1] = (uint8)(value >>  8U);
        dataPtr[2] = (uint8)(value >> 16U);
        dataPtr[3] = (uint8)(value >> 24U);
        return E_OK;
    }
    return E_NOT_OK;
}

/**
 * \brief   Packs a signal value into the TX I-PDU buffer.
 *
 * \details Writes the signal value from SignalDataPtr into the internal TX
 *          buffer at the bit position and with the endianness defined in the
 *          signal configuration table (AUTOSAR SWS_Com_00171).
 *          The value is read as a 4-byte little-endian integer from
 *          SignalDataPtr regardless of BitSize; only the relevant bits are
 *          packed into the TX buffer.
 *          The I-PDU is not transmitted immediately; call Com_TriggerIPDUSend()
 *          to trigger transmission.
 *
 * \param[in]  SignalId      ID of the signal to write. Must match an entry
 *                           in the signal configuration table.
 * \param[in]  SignalDataPtr Pointer to the signal value. Must be at least
 *                           4 bytes in little-endian order. Must not be NULL.
 *
 * \retval  E_OK      Signal was found and value was packed into the TX buffer.
 * \retval  E_NOT_OK  COM not initialized, SignalDataPtr is NULL, or SignalId
 *                    not found in the configuration table.
 *
 * \pre        Com_Init() must have been called successfully.
 * \note       Return type is uint8 per SWS_Com_00171. E_OK / E_NOT_OK values
 *             (0x00 / 0x01) are compatible with Std_ReturnType used by the RTE.
 *
 * \ServiceID      {0x0A}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
uint8 Com_SendSignal(Com_SignalIdType SignalId, const void* SignalDataPtr)
{
    if (Com_ConfigPtr == NULL || SignalDataPtr == NULL)
        return E_NOT_OK;

    const uint8* dataPtr = (const uint8*)SignalDataPtr;

    for (uint8 s = 0; s < Com_ConfigPtr->SignalCount; s++)
    {
        const Com_SignalConfigType* sig = &Com_ConfigPtr->Signals[s];
        if (sig->SignalId != SignalId)
            continue;

        const uint32 value = (uint32)dataPtr[0]
                           | ((uint32)dataPtr[1] <<  8U)
                           | ((uint32)dataPtr[2] << 16U)
                           | ((uint32)dataPtr[3] << 24U);

        Com_PackSignal(Com_TxBuffer[sig->IPduId],
                       sig->BitPosition, sig->BitSize, sig->Endian, value);
        return E_OK;
    }
    return E_NOT_OK;
}

/**
 * \brief   Triggers immediate transmission of a TX I-PDU via PduR.
 *
 * \details Looks up the TX I-PDU configuration by PduId, builds a PduInfoType
 *          pointing to the internal TX buffer, and calls PduR_Transmit()
 *          (AUTOSAR SWS_Com_00725). Logs the TX buffer contents before
 *          forwarding to PduR.
 *
 * \param[in]  PduId  COM I-PDU handle of the I-PDU to send. Must match an
 *                    IPduId in the TX I-PDU configuration table.
 *
 * \retval  E_OK      I-PDU was forwarded to PduR_Transmit() successfully.
 * \retval  E_NOT_OK  COM not initialized, PduId not found, or
 *                    PduR_Transmit() returned E_NOT_OK.
 *
 * \pre        Com_Init() must have been called successfully.
 * \pre        Signal values must have been set via Com_SendSignal() before
 *             calling this function.
 *
 * \ServiceID      {0x17}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Com_TriggerIPDUSend(PduIdType PduId)
{
    if (Com_ConfigPtr == NULL)
        return E_NOT_OK;

    for (uint8 i = 0; i < Com_ConfigPtr->TxIPduCount; i++)
    {
        const Com_IPduConfigType* ipdu = &Com_ConfigPtr->TxIPdus[i];
        if ((PduIdType)ipdu->IPduId != PduId)
            continue;

        Det_PrintP(PSTR("[Com_TxSend] IPdu="));
        Det_PrintDec(PduId);
        Det_PrintP(PSTR(" data=["));
        for (uint8 b = 0; b < ipdu->DLC; b++)
        {
            if (b > 0U) Det_Print(" ");
            if (Com_TxBuffer[PduId][b] < 0x10U) Det_Print("0");
            Det_PrintHex(Com_TxBuffer[PduId][b]);
        }
        Det_LogP(PSTR("]"));

        PduInfoType pduInfo = {
            .SduDataPtr = Com_TxBuffer[PduId],
            .SduLength  = ipdu->DLC
        };
        return PduR_Transmit(ipdu->PduRId, &pduInfo);
    }

    Det_PrintP(PSTR("[Com_TxSend] no TX IPdu="));
    Det_PrintDec(PduId);
    Det_Newline();
    return E_NOT_OK;
}

/**
 * \brief   Notifies COM that a TX I-PDU was successfully transmitted.
 *
 * \details Called by PduR after CanIf confirms a successful CAN frame
 *          transmission (AUTOSAR SWS_Com_00695). In this implementation
 *          the notification is logged only; no retry or deadline logic is
 *          performed.
 *
 * \param[in]  TxPduId  PduR-layer PDU ID of the confirmed TX I-PDU.
 * \param[in]  result   Transmission result forwarded from CanIf.
 *                      E_OK = success, E_NOT_OK = failure.
 *                      Currently unused; reserved for future error handling.
 *
 * \pre        Com_Init() must have been called successfully.
 * \note       result is accepted but not acted upon because this implementation
 *             does not support TX retry or error counters.
 *
 * \ServiceID      {0x11}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Com_TxConfirmation(PduIdType TxPduId, Std_ReturnType result)
{
    (void)result;
    Det_PrintP(PSTR("[Com_TxConf] TxPduId="));
    Det_PrintDec(TxPduId);
    Det_Newline();
}
