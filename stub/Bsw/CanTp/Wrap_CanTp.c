/**
 * \file    Wrap_CanTp.c
 * \brief   `src/Bsw/CanTp/CanTp.c` 内の関数を対象とした wrap 実体
 *          （Wrap_CanTp.h 参照）。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Wrap_CanTp.h"
#include "Det.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "CanTp"

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * External Variables
 * ====================================================================== */

uint32 CallCount_CanTp_Init           = 0U;
uint32 CallCount_CanTp_GetVersionInfo = 0U;
uint32 CallCount_CanTp_Transmit       = 0U;
uint32 CallCount_CanTp_MainFunction   = 0U;
uint32 CallCount_CanTp_RxIndication   = 0U;
uint32 CallCount_CanTp_TxConfirmation = 0U;
uint32 CallCount_CanTp_IsTxBusy       = 0U;

uint32 FailFromCallCount_CanTp_Transmit = WRAP_CANTP_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_CanTp_IsTxBusy = WRAP_CANTP_FAIL_FROM_CALL_COUNT_DISABLED;

Std_ReturnType ForcedReturn_CanTp_Transmit = E_NOT_OK;
boolean        ForcedReturn_CanTp_IsTxBusy = TRUE;

uint8         LastData_CanTp_Transmit[CANTP_TX_BUFFER_SIZE] = { 0U };
PduLengthType LastLength_CanTp_Transmit                     = 0U;

/* ----------------------------------------------------------------------
 * WrapCanTp_Reset — 7関数すべての状態を一括で初期化する（Wrap_CanTp.h 参照）。
 * ---------------------------------------------------------------------- */

void WrapCanTp_Reset(void)
{
    CallCount_CanTp_Init           = 0U;
    CallCount_CanTp_GetVersionInfo = 0U;
    CallCount_CanTp_Transmit       = 0U;
    CallCount_CanTp_MainFunction   = 0U;
    CallCount_CanTp_RxIndication   = 0U;
    CallCount_CanTp_TxConfirmation = 0U;
    CallCount_CanTp_IsTxBusy       = 0U;

    FailFromCallCount_CanTp_Transmit = WRAP_CANTP_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_CanTp_IsTxBusy = WRAP_CANTP_FAIL_FROM_CALL_COUNT_DISABLED;

    ForcedReturn_CanTp_Transmit = E_NOT_OK;
    ForcedReturn_CanTp_IsTxBusy = TRUE;

    for (uint32 i = 0U; i < CANTP_TX_BUFFER_SIZE; i++)
    {
        LastData_CanTp_Transmit[i] = 0U;
    }
    LastLength_CanTp_Transmit = 0U;
}

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * CanTp_Init
 * ---------------------------------------------------------------------- */

extern
void __real_CanTp_Init(const CanTp_ConfigType* CfgPtr);
void __wrap_CanTp_Init(const CanTp_ConfigType* CfgPtr)
{
    CallCount_CanTp_Init++;
    Log_Write(LOG_T, TAG, "CanTp_Init", "called %u times", CallCount_CanTp_Init);

    __real_CanTp_Init(CfgPtr);
}

/* ----------------------------------------------------------------------
 * CanTp_GetVersionInfo
 * ---------------------------------------------------------------------- */

extern
void __real_CanTp_GetVersionInfo(Std_VersionInfoType* versioninfo);
void __wrap_CanTp_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    CallCount_CanTp_GetVersionInfo++;
    Log_Write(LOG_T, TAG, "CanTp_GetVersionInfo", "called %u times", CallCount_CanTp_GetVersionInfo);

    __real_CanTp_GetVersionInfo(versioninfo);
}

/* -----------------------------------------------------------------------
 * CanTp_Shutdown
 * ----------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * CanTp_Transmit
 * ---------------------------------------------------------------------- */

extern
Std_ReturnType __real_CanTp_Transmit(PduIdType TxSduId, const PduInfoType* PduInfoPtr);
Std_ReturnType __wrap_CanTp_Transmit(PduIdType TxSduId, const PduInfoType* PduInfoPtr)
{
    CallCount_CanTp_Transmit++;
    Log_Write(LOG_T, TAG, "CanTp_Transmit", "called %u times", CallCount_CanTp_Transmit);

    if (PduInfoPtr != NULL && PduInfoPtr->SduDataPtr != NULL)
    {
        PduLengthType copyLen = (PduInfoPtr->SduLength <= (PduLengthType)CANTP_TX_BUFFER_SIZE)
                                 ? PduInfoPtr->SduLength : (PduLengthType)CANTP_TX_BUFFER_SIZE;
        for (PduLengthType i = 0U; i < copyLen; i++)
        {
            LastData_CanTp_Transmit[i] = PduInfoPtr->SduDataPtr[i];
        }
        LastLength_CanTp_Transmit = PduInfoPtr->SduLength;
    }

    if (CallCount_CanTp_Transmit >= FailFromCallCount_CanTp_Transmit)
    {
        return ForcedReturn_CanTp_Transmit;
    }

    return __real_CanTp_Transmit(TxSduId, PduInfoPtr);
}

/* -----------------------------------------------------------------------
 * CanTp_CancelTransmit
 * ----------------------------------------------------------------------- */

/* 未実装 */

/* -----------------------------------------------------------------------
 * CanTp_CancelReceive
 * ----------------------------------------------------------------------- */

/* 未実装 */

/* -----------------------------------------------------------------------
 * CanTp_ChangeParameter
 * ----------------------------------------------------------------------- */

/* 未実装 */

/* -----------------------------------------------------------------------
 * CanTp_ReadParameter
 * ----------------------------------------------------------------------- */

/* 未実装 */


/* ----------------------------------------------------------------------
 * CanTp_MainFunction
 * ---------------------------------------------------------------------- */

extern
void __real_CanTp_MainFunction(void);
void __wrap_CanTp_MainFunction(void)
{
    CallCount_CanTp_MainFunction++;
    Log_Write(LOG_T, TAG, "CanTp_MainFunction", "called %u times", CallCount_CanTp_MainFunction);

    __real_CanTp_MainFunction();
}

/* ======================================================================
 * Call-back notifications
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * CanTp_RxIndication
 * ---------------------------------------------------------------------- */

extern
void __real_CanTp_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr);
void __wrap_CanTp_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)
{
    CallCount_CanTp_RxIndication++;
    Log_Write(LOG_T, TAG, "CanTp_RxIndication", "called %u times", CallCount_CanTp_RxIndication);

    __real_CanTp_RxIndication(RxPduId, PduInfoPtr);
}

/* ----------------------------------------------------------------------
 * CanTp_TxConfirmation
 * ---------------------------------------------------------------------- */

extern
void __real_CanTp_TxConfirmation(PduIdType TxPduId, Std_ReturnType result);
void __wrap_CanTp_TxConfirmation(PduIdType TxPduId, Std_ReturnType result)
{
    CallCount_CanTp_TxConfirmation++;
    Log_Write(LOG_T, TAG, "CanTp_TxConfirmation", "called %u times", CallCount_CanTp_TxConfirmation);

    __real_CanTp_TxConfirmation(TxPduId, result);
}

/* ======================================================================
 * Internal Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * CanTp_IsTxBusy
 * ---------------------------------------------------------------------- */

extern
boolean __real_CanTp_IsTxBusy(void);
boolean __wrap_CanTp_IsTxBusy(void)
{
    CallCount_CanTp_IsTxBusy++;
    Log_Write(LOG_T, TAG, "CanTp_IsTxBusy", "called %u times", CallCount_CanTp_IsTxBusy);

    if (CallCount_CanTp_IsTxBusy >= FailFromCallCount_CanTp_IsTxBusy)
    {
        return ForcedReturn_CanTp_IsTxBusy;
    }

    return __real_CanTp_IsTxBusy();
}

