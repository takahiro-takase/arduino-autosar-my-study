/**
 * \file    Wrap_PduR.c
 * \brief   `src/Bsw/Can/Can.c` 内の関数を対象とした wrap 実体
 *          （Wrap_PduR.h 参照）。
 *
 * \details 変数を先頭の External Variables セクションへ集約し、その後に
 *          External Functions セクションで各 `__wrap_...` の実装をまとめる
 *          （2026-09-20、`src/Bsw/Can/Can.c` の External/Internal/Test
 *          Functions バナー方式に倣った構成。今後新規追加する `Wrap_XXX.c` は
 *          本ファイルと同じ構成へ統一する）。
 */
#include "Wrap_PduR.h"
#include "Det.h"

#define TAG "PduR"

/* ======================================================================
 * Global Variables
 * ====================================================================== */
uint32 CallCount_PduR_Init                = 0U;
uint32 CallCount_PduR_GetVersionInfo      = 0U;
uint32 CallCount_PduR_ComTransmit         = 0U;
uint32 CallCount_PduR_CanTpTransmit       = 0U;
uint32 CallCount_PduR_SecOCTransmit       = 0U;
uint32 CallCount_PduR_ComRxIndication     = 0U;
uint32 CallCount_PduR_CanIfTxConfirmation = 0U;
uint32 CallCount_PduR_SecOCTxConfirmation = 0U;

uint32 FailFromCallCount_PduR_ComTransmit   = Wrap_PduR_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_PduR_CanTpTransmit = Wrap_PduR_FAIL_FROM_CALL_COUNT_DISABLED;
uint32 FailFromCallCount_PduR_SecOCTransmit = Wrap_PduR_FAIL_FROM_CALL_COUNT_DISABLED;

Std_ReturnType ForcedReturn_PduR_ComTransmit   = E_NOT_OK;
Std_ReturnType ForcedReturn_PduR_CanTpTransmit = E_NOT_OK;
Std_ReturnType ForcedReturn_PduR_SecOcTransmit = E_NOT_OK;

/* ----------------------------------------------------------------------
 * WrapPduR_Reset — 11関数すべての状態を一括で初期化する（Wrap_PduR.h 参照）。
 * ---------------------------------------------------------------------- */

void WrapPduR_Reset(void)
{
    CallCount_PduR_Init                = 0U;
    CallCount_PduR_GetVersionInfo      = 0U;
    CallCount_PduR_ComTransmit         = 0U;
    CallCount_PduR_CanTpTransmit       = 0U;
    CallCount_PduR_SecOCTransmit       = 0U;
    CallCount_PduR_ComRxIndication     = 0U;
    CallCount_PduR_CanIfTxConfirmation = 0U;
    CallCount_PduR_SecOCTxConfirmation = 0U;

    FailFromCallCount_PduR_ComTransmit   = Wrap_PduR_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_PduR_CanTpTransmit = Wrap_PduR_FAIL_FROM_CALL_COUNT_DISABLED;
    FailFromCallCount_PduR_SecOCTransmit = Wrap_PduR_FAIL_FROM_CALL_COUNT_DISABLED;

    ForcedReturn_PduR_ComTransmit   = E_NOT_OK;
    ForcedReturn_PduR_CanTpTransmit = E_NOT_OK;
    ForcedReturn_PduR_SecOcTransmit = E_NOT_OK;
}

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * PduR_Init
 * ---------------------------------------------------------------------- */
extern
void __real_PduR_Init(const PduR_PBConfigType* Config);
void __wrap_PduR_Init(const PduR_PBConfigType* Config)
{
    CallCount_PduR_Init++;
    Log_Write(LOG_T, TAG, "PduR_Init", "called %u times", CallCount_PduR_Init);

    __real_PduR_Init(Config);
}

/* ----------------------------------------------------------------------
 * PduR_GetVersionInfo
 * ---------------------------------------------------------------------- */
extern
void __real_PduR_GetVersionInfo(Std_VersionInfoType* versioninfo);
void __wrap_PduR_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    CallCount_PduR_GetVersionInfo++;
    Log_Write(LOG_T, TAG, "PduR_GetVersionInfo", "called %u times", CallCount_PduR_GetVersionInfo);

    __real_PduR_GetVersionInfo(versioninfo);
}

/* ----------------------------------------------------------------------
 * PduR_GetConfigurationId
 * ---------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
 * PduR_EnableRouting
 * ---------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
 * PduR_DisableRouting
 * ---------------------------------------------------------------------- */


/* ======================================================================
 * Configurable interfaces definitions for interaction with upper layer module
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * PduR_ComTransmit
 * ---------------------------------------------------------------------- */
extern 
Std_ReturnType __real_PduR_ComTransmit(PduIdType SrcPduId, const PduInfoType* PduInfoPtr);
Std_ReturnType __wrap_PduR_ComTransmit(PduIdType SrcPduId, const PduInfoType* PduInfoPtr)
{
    CallCount_PduR_ComTransmit++;
    Log_Write(LOG_T, TAG, "PduR_ComTransmit", "called %u times", CallCount_PduR_ComTransmit);

    if (CallCount_PduR_ComTransmit >= FailFromCallCount_PduR_ComTransmit)
    {
        return ForcedReturn_PduR_ComTransmit;
    }
    return __real_PduR_ComTransmit(SrcPduId, PduInfoPtr);
}

/* ----------------------------------------------------------------------
 * PduR_CanTpTransmit
 * ---------------------------------------------------------------------- */
extern 
Std_ReturnType __real_PduR_CanTpTransmit(PduIdType SrcPduId, const PduInfoType* PduInfoPtr);
Std_ReturnType __wrap_PduR_CanTpTransmit(PduIdType SrcPduId, const PduInfoType* PduInfoPtr)
{
    CallCount_PduR_CanTpTransmit++;
    Log_Write(LOG_T, TAG, "PduR_CanTpTransmit", "called %u times", CallCount_PduR_CanTpTransmit);

    if (CallCount_PduR_CanTpTransmit >= FailFromCallCount_PduR_CanTpTransmit)
    {
        return ForcedReturn_PduR_CanTpTransmit;
    }
    return __real_PduR_CanTpTransmit(SrcPduId, PduInfoPtr);
}

/* ----------------------------------------------------------------------
 * PduR_SecOCTransmit
 * ---------------------------------------------------------------------- */
extern 
Std_ReturnType __real_PduR_SecOCTransmit(PduIdType SrcPduId, const PduInfoType* PduInfoPtr);
Std_ReturnType __wrap_PduR_SecOCTransmit(PduIdType SrcPduId, const PduInfoType* PduInfoPtr)
{
    CallCount_PduR_SecOCTransmit++;
    Log_Write(LOG_T, TAG, "PduR_SecOCTransmit", "called %u times", CallCount_PduR_SecOCTransmit);

    if (CallCount_PduR_SecOCTransmit >= FailFromCallCount_PduR_SecOCTransmit)
    {
        return ForcedReturn_PduR_SecOcTransmit;
    }
    return __real_PduR_SecOCTransmit(SrcPduId, PduInfoPtr);
}

/* ----------------------------------------------------------------------
 * PduR_<User:Up>Transmit
 * ---------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
 * PduR_<User:Up>CancelTransmit
 * ---------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
 * PduR_<User:Up>ChangeParameter (obsolete)
 * ---------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
 * PduR_<User:Up>CancelReceive
 * ---------------------------------------------------------------------- */

/* ======================================================================
 * Configurable interfaces definitions for lower layer communication interface module interaction
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * PduR_ComRxIndication
 * ---------------------------------------------------------------------- */
extern 
void __real_PduR_ComRxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr);
void __wrap_PduR_ComRxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)
{
    CallCount_PduR_ComRxIndication++;
    Log_Write(LOG_T, TAG, "PduR_ComRxIndication", "called %u times", CallCount_PduR_ComRxIndication);

    __real_PduR_ComRxIndication(RxPduId, PduInfoPtr);
}

/* ----------------------------------------------------------------------
 * PduR_<User:Lo>RxIndication
 * ---------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
 * PduR_CanIfTxConfirmation
 * ---------------------------------------------------------------------- */
extern 
void __real_PduR_CanIfTxConfirmation(PduIdType TxPduId, Std_ReturnType result);
void __wrap_PduR_CanIfTxConfirmation(PduIdType TxPduId, Std_ReturnType result)
{
    CallCount_PduR_CanIfTxConfirmation++;
    Log_Write(LOG_T, TAG, "PduR_CanIfTxConfirmation", "called %u times", CallCount_PduR_CanIfTxConfirmation);

    __real_PduR_CanIfTxConfirmation(TxPduId, result);
}

/* ----------------------------------------------------------------------
 * PduR_SecOCTxConfirmation
 * ---------------------------------------------------------------------- */
extern 
void __real_PduR_SecOCTxConfirmation(PduIdType SrcPduId, Std_ReturnType result);
void __wrap_PduR_SecOCTxConfirmation(PduIdType SrcPduId, Std_ReturnType result)
{
    CallCount_PduR_SecOCTxConfirmation++;
    Log_Write(LOG_T, TAG, "PduR_SecOCTxConfirmation", "called %u times", CallCount_PduR_SecOCTxConfirmation);

    __real_PduR_SecOCTxConfirmation(SrcPduId, result);
}

/* ----------------------------------------------------------------------
 * PduR_<User:Lo>TxConfirmation
 * ---------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
 * PduR_<User:Lo>TriggerTransmit
 * ---------------------------------------------------------------------- */


/* ======================================================================
 * Scheduled functions
 * ====================================================================== */

/* ======================================================================
 * Internal Functions
 * ====================================================================== */
