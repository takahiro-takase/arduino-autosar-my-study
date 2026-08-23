/**
 * \file    CanTp_fake.c
 * \brief   CanTp_fake.h 冒頭のコメント参照。
 */
#include "CanTp_fake.h"
#include "CanTp.h"
#include <string.h>

uint8  FakeCanTp_TxBuf[CANTP_FAKE_TX_BUF_SIZE];
uint8  FakeCanTp_TxLength      = 0U;
uint32 FakeCanTp_TransmitCount = 0U;

void FakeCanTp_Reset(void)
{
    memset(FakeCanTp_TxBuf, 0, sizeof(FakeCanTp_TxBuf));
    FakeCanTp_TxLength      = 0U;
    FakeCanTp_TransmitCount = 0U;
}

Std_ReturnType CanTp_Transmit(PduIdType TxSduId, const PduInfoType* PduInfoPtr)
{
    (void)TxSduId;
    FakeCanTp_TransmitCount++;

    if (PduInfoPtr == NULL || PduInfoPtr->SduDataPtr == NULL)
        return E_NOT_OK;

    uint8 len = (uint8)PduInfoPtr->SduLength;
    if (len > CANTP_FAKE_TX_BUF_SIZE)
        len = CANTP_FAKE_TX_BUF_SIZE;

    memcpy(FakeCanTp_TxBuf, PduInfoPtr->SduDataPtr, len);
    FakeCanTp_TxLength = len;

    return E_OK;
}
