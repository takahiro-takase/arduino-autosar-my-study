/**
 * \file    Wrap_ComM.c
 * \brief   `src/Bsw/ComM/ComM.c` 内の関数を対象とした wrap 実体
 *          （Wrap_ComM.h 参照）。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Wrap_ComM.h"
#include "Det.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "ComM"

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * Global Variables
 * ====================================================================== */

uint32 CallCount_ComM_Init                      = 0U;
uint32 CallCount_ComM_DeInit                    = 0U;
uint32 CallCount_ComM_GetStatus                 = 0U;
uint32 CallCount_ComM_RequestComMode            = 0U;
uint32 CallCount_ComM_GetRequestedComMode       = 0U;
uint32 CallCount_ComM_GetCurrentComMode         = 0U;
uint32 CallCount_ComM_GetVersionInfo            = 0U;
uint32 CallCount_ComM_Nm_NetworkStartIndication = 0U;
uint32 CallCount_ComM_Nm_NetworkMode            = 0U;
uint32 CallCount_ComM_Nm_PrepareBusSleepMode    = 0U;
uint32 CallCount_ComM_Nm_BusSleepMode           = 0U;
uint32 CallCount_ComM_DCM_ActiveDiagnostic      = 0U;
uint32 CallCount_ComM_DCM_InactiveDiagnostic    = 0U;
uint32 CallCount_ComM_CommunicationAllowed      = 0U;
uint32 CallCount_ComM_BusSM_ModeIndication      = 0U;
uint32 CallCount_ComM_MainFunction              = 0U;

uint8 Suppressed_ComM_DcmDiagnostic = 0U;

/* ----------------------------------------------------------------------
 * WrapComM_Reset — すべての関数状態を一括で初期化する（Wrap_ComM.h 参照）。
 * ---------------------------------------------------------------------- */

void WrapComM_Reset(void)
{
    CallCount_ComM_Init                      = 0U;
    CallCount_ComM_DeInit                    = 0U;
    CallCount_ComM_GetStatus                 = 0U;
    CallCount_ComM_RequestComMode            = 0U;
    CallCount_ComM_GetRequestedComMode       = 0U;
    CallCount_ComM_GetCurrentComMode         = 0U;
    CallCount_ComM_GetVersionInfo            = 0U;
    CallCount_ComM_Nm_NetworkStartIndication = 0U;
    CallCount_ComM_Nm_NetworkMode            = 0U;
    CallCount_ComM_Nm_PrepareBusSleepMode    = 0U;
    CallCount_ComM_Nm_BusSleepMode           = 0U;
    CallCount_ComM_DCM_ActiveDiagnostic      = 0U;
    CallCount_ComM_DCM_InactiveDiagnostic    = 0U;
    CallCount_ComM_CommunicationAllowed      = 0U;
    CallCount_ComM_BusSM_ModeIndication      = 0U;
    CallCount_ComM_MainFunction              = 0U;

    Suppressed_ComM_DcmDiagnostic = 0U;
}

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * ComM_Init
 * ---------------------------------------------------------------------- */

extern 
void __real_ComM_Init(const ComM_ConfigType* ConfigPtr);
void __wrap_ComM_Init(const ComM_ConfigType* ConfigPtr)
{
    CallCount_ComM_Init++;
    Log_Write(LOG_T, TAG, "ComM_Init", "called %u times", CallCount_ComM_Init);

    __real_ComM_Init(ConfigPtr);
}

/* ----------------------------------------------------------------------
 * ComM_DeInit
 * ---------------------------------------------------------------------- */

extern 
void __real_ComM_DeInit(void);
void __wrap_ComM_DeInit(void)
{
    CallCount_ComM_DeInit++;
    Log_Write(LOG_T, TAG, "ComM_DeInit", "called %u times", CallCount_ComM_DeInit);

    __real_ComM_DeInit();
}

/* ----------------------------------------------------------------------
 * ComM_GetState
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * ComM_GetStatus
 * ---------------------------------------------------------------------- */

extern 
Std_ReturnType __real_ComM_GetStatus(ComM_InitStatusType* Status);
Std_ReturnType __wrap_ComM_GetStatus(ComM_InitStatusType* Status)
{
    CallCount_ComM_GetStatus++;
    Log_Write(LOG_T, TAG, "ComM_GetStatus", "called %u times", CallCount_ComM_GetStatus);

    return __real_ComM_GetStatus(Status);
}

/* ----------------------------------------------------------------------
 * ComM_GetInhibitionStatus
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * ComM_RequestComMode
 * ---------------------------------------------------------------------- */

extern 
Std_ReturnType __real_ComM_RequestComMode(ComM_UserHandleType User, ComM_ModeType ComMode);
Std_ReturnType __wrap_ComM_RequestComMode(ComM_UserHandleType User, ComM_ModeType ComMode)
{
    CallCount_ComM_RequestComMode++;
    Log_Write(LOG_T, TAG, "ComM_RequestComMode", "called %u times", CallCount_ComM_RequestComMode);

    return __real_ComM_RequestComMode(User, ComMode);
}

/* ----------------------------------------------------------------------
 * ComM_GetMaxComMode
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * ComM_GetRequestedComMode
 * ---------------------------------------------------------------------- */

extern 
Std_ReturnType __real_ComM_GetRequestedComMode(ComM_UserHandleType User, ComM_ModeType* ComMode);
Std_ReturnType __wrap_ComM_GetRequestedComMode(ComM_UserHandleType User, ComM_ModeType* ComMode)
{
    CallCount_ComM_GetRequestedComMode++;
    Log_Write(LOG_T, TAG, "ComM_GetRequestedComMode", "called %u times", CallCount_ComM_GetRequestedComMode);

    return __real_ComM_GetRequestedComMode(User, ComMode);
}

/* ----------------------------------------------------------------------
 * ComM_GetCurrentComMode
 * ---------------------------------------------------------------------- */

extern 
Std_ReturnType __real_ComM_GetCurrentComMode(ComM_UserHandleType User, ComM_ModeType* ComMode);
Std_ReturnType __wrap_ComM_GetCurrentComMode(ComM_UserHandleType User, ComM_ModeType* ComMode)
{
    CallCount_ComM_GetCurrentComMode++;
    Log_Write(LOG_T, TAG, "ComM_GetCurrentComMode", "called %u times", CallCount_ComM_GetCurrentComMode);

    return __real_ComM_GetCurrentComMode(User, ComMode);
}

/* ----------------------------------------------------------------------
 * ComM_PreventWakeUp
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * ComM_LimitChannelToNoComMode
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * ComM_LimitECUToNoComMode
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * ComM_ReadInhibitCounter
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * ComM_ResetInhibitCounter
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * ComM_SetECUGroupClassification
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * ComM_GetVersionInfo
 * ---------------------------------------------------------------------- */

extern 
void __real_ComM_GetVersionInfo(Std_VersionInfoType* Versioninfo);
void __wrap_ComM_GetVersionInfo(Std_VersionInfoType* Versioninfo)
{
    CallCount_ComM_GetVersionInfo++;
    Log_Write(LOG_T, TAG, "ComM_GetVersionInfo", "called %u times", CallCount_ComM_GetVersionInfo);

    __real_ComM_GetVersionInfo(Versioninfo);
}

/* ======================================================================
 *  Callback notifications
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * ComM_Nm_NetworkStartIndication
 * ---------------------------------------------------------------------- */

extern 
void __real_ComM_Nm_NetworkStartIndication(NetworkHandleType Network);
void __wrap_ComM_Nm_NetworkStartIndication(NetworkHandleType Network)
{
    CallCount_ComM_Nm_NetworkStartIndication++;
    Log_Write(LOG_T, TAG, "ComM_Nm_NetworkStartIndication", "called %u times", CallCount_ComM_Nm_NetworkStartIndication);

    __real_ComM_Nm_NetworkStartIndication(Network);
}

/* ----------------------------------------------------------------------
 * ComM_Nm_NetworkMode
 * ---------------------------------------------------------------------- */

extern 
void __real_ComM_Nm_NetworkMode(NetworkHandleType Network);
void __wrap_ComM_Nm_NetworkMode(NetworkHandleType Network)
{
    CallCount_ComM_Nm_NetworkMode++;
    Log_Write(LOG_T, TAG, "ComM_Nm_NetworkMode", "called %u times", CallCount_ComM_Nm_NetworkMode);

    __real_ComM_Nm_NetworkMode(Network);
}

/* ----------------------------------------------------------------------
 * ComM_Nm_PrepareBusSleepMode
 * ---------------------------------------------------------------------- */

extern 
void __real_ComM_Nm_PrepareBusSleepMode(NetworkHandleType Network);
void __wrap_ComM_Nm_PrepareBusSleepMode(NetworkHandleType Network)
{
    CallCount_ComM_Nm_PrepareBusSleepMode++;
    Log_Write(LOG_T, TAG, "ComM_Nm_PrepareBusSleepMode", "called %u times", CallCount_ComM_Nm_PrepareBusSleepMode);

    __real_ComM_Nm_PrepareBusSleepMode(Network);
}

/* ----------------------------------------------------------------------
 * ComM_Nm_BusSleepMode
 * ---------------------------------------------------------------------- */

extern 
void __real_ComM_Nm_BusSleepMode(NetworkHandleType Network);
void __wrap_ComM_Nm_BusSleepMode(NetworkHandleType Network)
{
    CallCount_ComM_Nm_BusSleepMode++;
    Log_Write(LOG_T, TAG, "ComM_Nm_BusSleepMode", "called %u times", CallCount_ComM_Nm_BusSleepMode);

    __real_ComM_Nm_BusSleepMode(Network);
}

/* ----------------------------------------------------------------------
 * ComM_Nm_RestartIndication
 * ---------------------------------------------------------------------- */

/* 未実装 */


/* ----------------------------------------------------------------------
 * ComM_DCM_ActiveDiagnostic
 * ---------------------------------------------------------------------- */

extern
void __real_ComM_DCM_ActiveDiagnostic(NetworkHandleType Channel);
void __wrap_ComM_DCM_ActiveDiagnostic(NetworkHandleType Channel)
{
    CallCount_ComM_DCM_ActiveDiagnostic++;
    Log_Write(LOG_T, TAG, "ComM_DCM_ActiveDiagnostic", "called %u times", CallCount_ComM_DCM_ActiveDiagnostic);

    if (Suppressed_ComM_DcmDiagnostic)
    {
        return;
    }

    __real_ComM_DCM_ActiveDiagnostic(Channel);
}

/* ----------------------------------------------------------------------
 * ComM_DCM_InactiveDiagnostic
 * ---------------------------------------------------------------------- */
extern
void __real_ComM_DCM_InactiveDiagnostic(NetworkHandleType Channel);
void __wrap_ComM_DCM_InactiveDiagnostic(NetworkHandleType Channel)
{
    CallCount_ComM_DCM_InactiveDiagnostic++;
    Log_Write(LOG_T, TAG, "ComM_DCM_InactiveDiagnostic", "called %u times", CallCount_ComM_DCM_InactiveDiagnostic);

    if (Suppressed_ComM_DcmDiagnostic)
    {
        return;
    }

    __real_ComM_DCM_InactiveDiagnostic(Channel);
}

/* ----------------------------------------------------------------------
 * ComM_EcuM_WakeUpIndication
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * ComM_EcuM_PNCWakeUpIndication
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * ComM_CommunicationAllowed
 * ---------------------------------------------------------------------- */

extern 
void __real_ComM_CommunicationAllowed(NetworkHandleType Channel, boolean Allowed);
void __wrap_ComM_CommunicationAllowed(NetworkHandleType Channel, boolean Allowed)
{
    CallCount_ComM_CommunicationAllowed++;
    Log_Write(LOG_T, TAG, "ComM_CommunicationAllowed", "called %u times", CallCount_ComM_CommunicationAllowed);

    __real_ComM_CommunicationAllowed(Channel, Allowed);
}

/* ----------------------------------------------------------------------
 * ComM_BusSM_ModeIndication
 * ---------------------------------------------------------------------- */

extern 
void __real_ComM_BusSM_ModeIndication(NetworkHandleType Network, ComM_ModeType Mode);
void __wrap_ComM_BusSM_ModeIndication(NetworkHandleType Network, ComM_ModeType Mode)
{
    CallCount_ComM_BusSM_ModeIndication++;
    Log_Write(LOG_T, TAG, "ComM_BusSM_ModeIndication", "called %u times", CallCount_ComM_BusSM_ModeIndication);

    __real_ComM_BusSM_ModeIndication(Network, Mode);
}

/* ----------------------------------------------------------------------
 * ComM_COMCbk_<sn>
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ======================================================================
 *  Scheduled functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * ComM_MainFunction_<Channel_Id>
 * ---------------------------------------------------------------------- */

extern 
void __real_ComM_MainFunction(void);
void __wrap_ComM_MainFunction(void)
{
    CallCount_ComM_MainFunction++;
    Log_Write(LOG_T, TAG, "ComM_MainFunction", "called %u times", CallCount_ComM_MainFunction);

    __real_ComM_MainFunction();
}

/* ======================================================================
 * Internal functions
 * ====================================================================== */
