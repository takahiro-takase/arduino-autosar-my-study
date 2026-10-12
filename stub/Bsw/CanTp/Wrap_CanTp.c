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
#include "Dcm_Cbk.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "CanTp"

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * Global Variables
 * ====================================================================== */

uint32 CallCount_CanTp_Init           = 0U;
uint32 CallCount_CanTp_GetVersionInfo = 0U;
uint32 CallCount_CanTp_Transmit       = 0U;
uint32 CallCount_CanTp_MainFunction   = 0U;
uint32 CallCount_CanTp_RxIndication   = 0U;
uint32 CallCount_CanTp_TxConfirmation = 0U;

uint32 FailFromCallCount_CanTp_Transmit = WRAP_CANTP_FAIL_FROM_CALL_COUNT_DISABLED;

Std_ReturnType ForcedReturn_CanTp_Transmit = E_NOT_OK;

uint8         CaptureEnabled_CanTp_Transmit               = 0U;
uint8         SuppressConfirmation_CanTp_Transmit         = 0U;
uint8         LastData_CanTp_Transmit[WRAP_CANTP_CAPTURE_SIZE] = { 0U };
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

    FailFromCallCount_CanTp_Transmit = WRAP_CANTP_FAIL_FROM_CALL_COUNT_DISABLED;

    ForcedReturn_CanTp_Transmit = E_NOT_OK;
    CaptureEnabled_CanTp_Transmit       = 0U;
    SuppressConfirmation_CanTp_Transmit = 0U;

    for (uint32 i = 0U; i < WRAP_CANTP_CAPTURE_SIZE; i++)
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

    if (PduInfoPtr != NULL)
    {
        LastLength_CanTp_Transmit = PduInfoPtr->SduLength;

        if ((CaptureEnabled_CanTp_Transmit != 0U) && (PduInfoPtr->SduLength > 0U))
        {
            /* 下位層の代わりに、Dcm の応答を読み出して複製する。読み出した分は、TP_DATARETRY で巻き戻す
             * （本物の CanTp が、後から先頭から読めるようにするため）。 */
            PduInfoType   cap;
            PduLengthType avail = 0U;
            cap.SduDataPtr = LastData_CanTp_Transmit;
            cap.SduLength  = (PduInfoPtr->SduLength <= (PduLengthType)WRAP_CANTP_CAPTURE_SIZE)
                              ? PduInfoPtr->SduLength : (PduLengthType)WRAP_CANTP_CAPTURE_SIZE;
            if (Dcm_CopyTxData(CANTP_DCM_TX_PDU_ID, &cap, NULL, &avail) == BUFREQ_OK)
            {
                RetryInfoType rewind;
                PduInfoType   none;
                rewind.TpDataState = TP_DATARETRY;
                rewind.TxTpDataCnt = cap.SduLength;
                none.SduDataPtr    = NULL;
                none.SduLength     = 0U;
                (void)Dcm_CopyTxData(CANTP_DCM_TX_PDU_ID, &none, &rewind, &avail);
            }
        }
    }

    if (CallCount_CanTp_Transmit >= FailFromCallCount_CanTp_Transmit)
    {
        if ((ForcedReturn_CanTp_Transmit == E_OK) && (SuppressConfirmation_CanTp_Transmit == 0U))
        {
            Dcm_TpTxConfirmation(CANTP_DCM_TX_PDU_ID, E_OK);   /* 本物の CanTp が送り終えた体にする */
        }
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

