/**
 * \file    Wrap_Com.c
 * \brief   `src/Bsw/Can/Can.c` 内の関数を対象とした wrap 実体
 *          （Wrap_Com.h 参照）。
 *
 * \details 変数を先頭の External Variables セクションへ集約し、その後に
 *          External Functions セクションで各 `__wrap_...` の実装をまとめる
 *          （2026-09-20、`src/Bsw/Can/Can.c` の External/Internal/Test
 *          Functions バナー方式に倣った構成。今後新規追加する `Wrap_XXX.c` は
 *          本ファイルと同じ構成へ統一する）。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Wrap_Com.h"
#include "Det.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "Com"

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * Global Variables
 * ====================================================================== */

uint32 CallCount_Com_Init                    = 0U;
uint32 CallCount_Com_DeInit                  = 0U;
uint32 CallCount_Com_IpduGroupStart          = 0U;
uint32 CallCount_Com_IpduGroupStop           = 0U;
uint32 CallCount_Com_EnableReceptionDM       = 0U;
uint32 CallCount_Com_DisableReceptionDM      = 0U;
uint32 CallCount_Com_GetStatus               = 0U;
uint32 CallCount_Com_GetVersionInfo          = 0U;
uint32 CallCount_Com_SendSignal              = 0U;
uint32 CallCount_Com_ReceiveSignal           = 0U;
uint32 CallCount_Com_SendSignalGroup         = 0U;
uint32 CallCount_Com_ReceiveSignalGroup      = 0U;
uint32 CallCount_Com_SendSignalGroupArray    = 0U;
uint32 CallCount_Com_ReceiveSignalGroupArray = 0U;
uint32 CallCount_Com_InvalidateSignal        = 0U;
uint32 CallCount_Com_InvalidateSignalGroup   = 0U;
uint32 CallCount_Com_TriggerIPDUSend         = 0U;
uint32 CallCount_Com_SwitchIpduTxMode        = 0U;
uint32 CallCount_Com_RxIndication            = 0U;
uint32 CallCount_Com_TxConfirmation          = 0U;
uint32 CallCount_Com_MainFunctionRx          = 0U;
uint32 CallCount_Com_MainFunctionTx          = 0U;

uint32 FailFromCallCount_Com_TriggerIPDUSend = Wrap_Com_FAIL_FROM_CALL_COUNT_DISABLED;

Std_ReturnType ForcedReturn_Com_TriggerIPDUSend = E_NOT_OK;

/* ----------------------------------------------------------------------
 * WrapCom_Reset — 11関数すべての状態を一括で初期化する（Wrap_Com.h 参照）。
 * ---------------------------------------------------------------------- */

void WrapCom_Reset(void)
{
    CallCount_Com_Init                    = 0U;
    CallCount_Com_DeInit                  = 0U;
    CallCount_Com_IpduGroupStart          = 0U;
    CallCount_Com_IpduGroupStop           = 0U;
    CallCount_Com_EnableReceptionDM       = 0U;
    CallCount_Com_DisableReceptionDM      = 0U;
    CallCount_Com_GetStatus               = 0U;
    CallCount_Com_GetVersionInfo          = 0U;
    CallCount_Com_SendSignal              = 0U;
    CallCount_Com_ReceiveSignal           = 0U;
    CallCount_Com_SendSignalGroup         = 0U;
    CallCount_Com_ReceiveSignalGroup      = 0U;
    CallCount_Com_SendSignalGroupArray    = 0U;
    CallCount_Com_ReceiveSignalGroupArray = 0U;
    CallCount_Com_InvalidateSignal        = 0U;
    CallCount_Com_InvalidateSignalGroup   = 0U;
    CallCount_Com_TriggerIPDUSend         = 0U;
    CallCount_Com_SwitchIpduTxMode        = 0U;
    CallCount_Com_RxIndication            = 0U;
    CallCount_Com_TxConfirmation          = 0U;
    CallCount_Com_MainFunctionRx          = 0U;
    CallCount_Com_MainFunctionTx          = 0U;

    FailFromCallCount_Com_TriggerIPDUSend = Wrap_Com_FAIL_FROM_CALL_COUNT_DISABLED;

    ForcedReturn_Com_TriggerIPDUSend = E_NOT_OK;
}

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Com_Init
 * ---------------------------------------------------------------------- */

extern
void __real_Com_Init(const Com_ConfigType* Config);
void __wrap_Com_Init(const Com_ConfigType* Config)
{
    CallCount_Com_Init++;
    Log_Write(LOG_T, TAG, "Com_Init", "called %u times", CallCount_Com_Init);

    __real_Com_Init(Config);
}

/* ----------------------------------------------------------------------
 * Com_DeInit
 * ---------------------------------------------------------------------- */

extern 
void __real_Com_DeInit(void);
void __wrap_Com_DeInit(void)
{
    CallCount_Com_DeInit++;
    Log_Write(LOG_T, TAG, "Com_DeInit", "called %u times", CallCount_Com_DeInit);

    __real_Com_DeInit();    
}

/* ----------------------------------------------------------------------
 * Com_IpduGroupStart
 * ---------------------------------------------------------------------- */

extern 
void __real_Com_IpduGroupStart(Com_IpduGroupIdType IpduGroupId, boolean initialize);
void __wrap_Com_IpduGroupStart(Com_IpduGroupIdType IpduGroupId, boolean initialize)
{
    CallCount_Com_IpduGroupStart++;
    Log_Write(LOG_T, TAG, "Com_IpduGroupStart", "called %u times", CallCount_Com_IpduGroupStart);

    __real_Com_IpduGroupStart(IpduGroupId, initialize);        
}

/* ----------------------------------------------------------------------
 * Com_IpduGroupStop
 * ---------------------------------------------------------------------- */

extern 
void __real_Com_IpduGroupStop(Com_IpduGroupIdType IpduGroupId);
void __wrap_Com_IpduGroupStop(Com_IpduGroupIdType IpduGroupId)
{
    CallCount_Com_IpduGroupStop++;
    Log_Write(LOG_T, TAG, "Com_IpduGroupStop", "called %u times", CallCount_Com_IpduGroupStop);

    __real_Com_IpduGroupStop(IpduGroupId);        
}

/* ----------------------------------------------------------------------
 * Com_EnableReceptionDM
 * ---------------------------------------------------------------------- */

extern 
void __real_Com_EnableReceptionDM(Com_IpduGroupIdType IpduGroupId);
void __wrap_Com_EnableReceptionDM(Com_IpduGroupIdType IpduGroupId)
{
    CallCount_Com_EnableReceptionDM++;
    Log_Write(LOG_T, TAG, "Com_EnableReceptionDM", "called %u times", CallCount_Com_EnableReceptionDM);

    __real_Com_EnableReceptionDM(IpduGroupId); 
}

/* ----------------------------------------------------------------------
 * Com_DisableReceptionDM
 * ---------------------------------------------------------------------- */

extern 
void __real_Com_DisableReceptionDM(Com_IpduGroupIdType IpduGroupId);
void __wrap_Com_DisableReceptionDM(Com_IpduGroupIdType IpduGroupId)
{
    CallCount_Com_DisableReceptionDM++;
    Log_Write(LOG_T, TAG, "Com_DisableReceptionDM", "called %u times", CallCount_Com_DisableReceptionDM);

    __real_Com_DisableReceptionDM(IpduGroupId); 
}

/* ----------------------------------------------------------------------
 * Com_GetStatus
 * ---------------------------------------------------------------------- */

extern 
Com_StatusType __real_Com_GetStatus(void);
Com_StatusType __wrap_Com_GetStatus(void)
{
    CallCount_Com_GetStatus++;
    Log_Write(LOG_T, TAG, "Com_GetStatus", "called %u times", CallCount_Com_GetStatus);

    return __real_Com_GetStatus(); 
}

/* ----------------------------------------------------------------------
 * Com_GetVersionInfo
 * ---------------------------------------------------------------------- */

extern
void __real_Com_GetVersionInfo(Std_VersionInfoType* versioninfo);
void __wrap_Com_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    CallCount_Com_GetVersionInfo++;
    Log_Write(LOG_T, TAG, "PduR_GetVersionInfo", "called %u times", CallCount_Com_GetVersionInfo);

    __real_Com_GetVersionInfo(versioninfo);
}

/* ----------------------------------------------------------------------
 * Com_SendSignal
 * ---------------------------------------------------------------------- */

extern 
uint8 __real_Com_SendSignal(Com_SignalIdType SignalId, const void* SignalDataPtr);
uint8 __wrap_Com_SendSignal(Com_SignalIdType SignalId, const void* SignalDataPtr)
{
    CallCount_Com_SendSignal++;
    Log_Write(LOG_T, TAG, "Com_SendSignal", "called %u times", CallCount_Com_SendSignal);

    return __real_Com_SendSignal(SignalId, SignalDataPtr);
}

/* ----------------------------------------------------------------------
 * Com_SendDynSignal
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Com_ReceiveSignal
 * ---------------------------------------------------------------------- */

extern 
uint8 __real_Com_ReceiveSignal(Com_SignalIdType SignalId, void* SignalDataPtr);
uint8 __wrap_Com_ReceiveSignal(Com_SignalIdType SignalId, void* SignalDataPtr)
{
    CallCount_Com_ReceiveSignal++;
    Log_Write(LOG_T, TAG, "Com_ReceiveSignal", "called %u times", CallCount_Com_ReceiveSignal);

    return __real_Com_ReceiveSignal(SignalId, SignalDataPtr);
}

/* ----------------------------------------------------------------------
 * Com_ReceiveDynSignal
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Com_SendSignalGroup
 * ---------------------------------------------------------------------- */

extern 
uint8 __real_Com_SendSignalGroup(Com_SignalGroupIdType SignalGroupId);
uint8 __wrap_Com_SendSignalGroup(Com_SignalGroupIdType SignalGroupId)
{
    CallCount_Com_SendSignalGroup++;
    Log_Write(LOG_T, TAG, "Com_SendSignalGroup", "called %u times", CallCount_Com_SendSignalGroup);

    return __real_Com_SendSignalGroup(SignalGroupId);
}

/* ----------------------------------------------------------------------
 * Com_ReceiveSignalGroup
 * ---------------------------------------------------------------------- */

extern 
uint8 __real_Com_ReceiveSignalGroup(Com_SignalGroupIdType SignalGroupId);
uint8 __wrap_Com_ReceiveSignalGroup(Com_SignalGroupIdType SignalGroupId)
{
    CallCount_Com_ReceiveSignalGroup++;
    Log_Write(LOG_T, TAG, "Com_ReceiveSignalGroup", "called %u times", CallCount_Com_ReceiveSignalGroup);

    return __real_Com_ReceiveSignalGroup(SignalGroupId);
}

/* ----------------------------------------------------------------------
 * Com_SendSignalGroupArray
 * ---------------------------------------------------------------------- */

extern 
uint8 __real_Com_SendSignalGroupArray(Com_SignalGroupIdType SignalGroupId, const uint8* DataPtr);
uint8 __wrap_Com_SendSignalGroupArray(Com_SignalGroupIdType SignalGroupId, const uint8* DataPtr)
{
    CallCount_Com_SendSignalGroupArray++;
    Log_Write(LOG_T, TAG, "Com_SendSignalGroupArray", "called %u times", CallCount_Com_SendSignalGroupArray);

    return __real_Com_SendSignalGroupArray(SignalGroupId, DataPtr);
}

/* ----------------------------------------------------------------------
 * Com_ReceiveSignalGroupArray
 * ---------------------------------------------------------------------- */

extern 
uint8 __real_Com_ReceiveSignalGroupArray(Com_SignalGroupIdType SignalGroupId, uint8* DataPtr);
uint8 __wrap_Com_ReceiveSignalGroupArray(Com_SignalGroupIdType SignalGroupId, uint8* DataPtr)
{
    CallCount_Com_ReceiveSignalGroupArray++;
    Log_Write(LOG_T, TAG, "Com_ReceiveSignalGroupArray", "called %u times", CallCount_Com_ReceiveSignalGroupArray);

    return __real_Com_ReceiveSignalGroupArray(SignalGroupId, DataPtr);
}

/* ----------------------------------------------------------------------
 * Com_InvalidateSignal
 * ---------------------------------------------------------------------- */

extern 
uint8 __real_Com_InvalidateSignal(Com_SignalIdType SignalId);
uint8 __wrap_Com_InvalidateSignal(Com_SignalIdType SignalId)
{
    CallCount_Com_InvalidateSignal++;
    Log_Write(LOG_T, TAG, "Com_InvalidateSignal", "called %u times", CallCount_Com_InvalidateSignal);

    return __real_Com_InvalidateSignal(SignalId);
}

/* ----------------------------------------------------------------------
 * Com_InvalidateSignalGroup
 * ---------------------------------------------------------------------- */

extern 
uint8 __real_Com_InvalidateSignalGroup(Com_SignalGroupIdType SignalGroupId);
uint8 __wrap_Com_InvalidateSignalGroup(Com_SignalGroupIdType SignalGroupId)
{
    CallCount_Com_InvalidateSignalGroup++;
    Log_Write(LOG_T, TAG, "Com_InvalidateSignalGroup", "called %u times", CallCount_Com_InvalidateSignalGroup);

    return __real_Com_InvalidateSignalGroup(SignalGroupId);
}

/* ----------------------------------------------------------------------
 * Com_TriggerIPDUSend
 * ---------------------------------------------------------------------- */

extern 
Std_ReturnType __real_Com_TriggerIPDUSend(Com_IPduIdType PduId);
Std_ReturnType __wrap_Com_TriggerIPDUSend(Com_IPduIdType PduId)
{
    CallCount_Com_TriggerIPDUSend++;
    Log_Write(LOG_T, TAG, "Com_TriggerIPDUSend", "called %u times", CallCount_Com_TriggerIPDUSend);

    if (CallCount_Com_TriggerIPDUSend >= FailFromCallCount_Com_TriggerIPDUSend)
    {
        return ForcedReturn_Com_TriggerIPDUSend;
    }
    return __real_Com_TriggerIPDUSend(PduId);
}

/* ----------------------------------------------------------------------
 * Com_TriggerIPDUSendWithMetaData
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Com_SwitchIpduTxMode
 * ---------------------------------------------------------------------- */

extern 
void __real_Com_SwitchIpduTxMode(Com_IPduIdType PduId, boolean Mode);
void __wrap_Com_SwitchIpduTxMode(Com_IPduIdType PduId, boolean Mode)
{
    CallCount_Com_SwitchIpduTxMode++;
    Log_Write(LOG_T, TAG, "Com_SwitchIpduTxMode", "called %u times", CallCount_Com_SwitchIpduTxMode);

    return __real_Com_SwitchIpduTxMode(PduId, Mode);
}

/* ======================================================================
 * Callback Functions and Notifications
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Com_TriggerTransmit
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Com_RxIndication
 * ---------------------------------------------------------------------- */

extern 
void __real_Com_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr);
void __wrap_Com_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)
{
    CallCount_Com_RxIndication++;
    Log_Write(LOG_T, TAG, "Com_RxIndication", "called %u times", CallCount_Com_RxIndication);

    return __real_Com_RxIndication(RxPduId, PduInfoPtr);
}

/* ----------------------------------------------------------------------
 * Com_TpRxIndication
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Com_TxConfirmation
 * ---------------------------------------------------------------------- */

extern 
void __real_Com_TxConfirmation(PduIdType TxPduId, Std_ReturnType result);
void __wrap_Com_TxConfirmation(PduIdType TxPduId, Std_ReturnType result)
{
    CallCount_Com_TxConfirmation++;
    Log_Write(LOG_T, TAG, "Com_TxConfirmation", "called %u times", CallCount_Com_TxConfirmation);

    return __real_Com_TxConfirmation(TxPduId, result);
}

/* ----------------------------------------------------------------------
 * Com_TpTxConfirmation
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Com_StartOfReception
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Com_CopyRxData
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Com_CopyTxData
 * ---------------------------------------------------------------------- */

/* 未実装 */
 
/* ======================================================================
 * Scheduled Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Com_MainFunctionRx
 * ---------------------------------------------------------------------- */

extern 
void __real_Com_MainFunctionRx(void);
void __wrap_Com_MainFunctionRx(void)
{
    CallCount_Com_MainFunctionRx++;
    Log_Write(LOG_T, TAG, "Com_MainFunctionRx", "called %u times", CallCount_Com_MainFunctionRx);

    return __real_Com_MainFunctionRx();
}

/* ----------------------------------------------------------------------
 * Com_MainFunctionTx
 * ---------------------------------------------------------------------- */

extern 
void __real_Com_MainFunctionTx(void);
void __wrap_Com_MainFunctionTx(void)
{
    CallCount_Com_MainFunctionTx++;
    Log_Write(LOG_T, TAG, "Com_MainFunctionTx", "called %u times", CallCount_Com_MainFunctionTx);

    return __real_Com_MainFunctionTx();
}

/* ----------------------------------------------------------------------
 * Com_MainFunctionRouteSignals
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * PduR_SecOCTxConfirmation
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * PduR_<User:Lo>TxConfirmation
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * PduR_<User:Lo>TriggerTransmit
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ======================================================================
 * Internal Functions
 * ====================================================================== */
