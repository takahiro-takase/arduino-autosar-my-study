/**
 * \file    Wrap_Gpt.c
 * \brief   `src/Bsw/Gpt/Gpt.c` 内の関数を対象とした wrap 実体
 *          （Wrap_Gpt.h 参照）。
 *
 * \details 変数を先頭の Global Variables セクションへ集約し、その後に
 *          Functions セクションで各 `__wrap_...` の実装をまとめる
 *          （`Wrap_Can.c` と同じ構成。`[[reference_wrap_stub_naming_convention]]`
 *          参照）。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Wrap_Gpt.h"
#include "Det.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "Gpt"

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * Global Variables
 * ====================================================================== */

uint32 CallCount_Gpt_GetVersionInfo       = 0U;
uint32 CallCount_Gpt_Init                 = 0U;
uint32 CallCount_Gpt_DeInit               = 0U;
uint32 CallCount_Gpt_GetTimeElapsed       = 0U;
uint32 CallCount_Gpt_GetTimeRemaining     = 0U;
uint32 CallCount_Gpt_StartTimer           = 0U;
uint32 CallCount_Gpt_StopTimer            = 0U;
uint32 CallCount_Gpt_EnableNotification   = 0U;
uint32 CallCount_Gpt_DisableNotification  = 0U;

/* ----------------------------------------------------------------------
 * WrapGpt_Reset — 9関数すべての状態を一括で初期化する（Wrap_Gpt.h 参照）。
 * ---------------------------------------------------------------------- */

void WrapGpt_Reset(void)
{
    CallCount_Gpt_GetVersionInfo      = 0U;
    CallCount_Gpt_Init                = 0U;
    CallCount_Gpt_DeInit              = 0U;
    CallCount_Gpt_GetTimeElapsed      = 0U;
    CallCount_Gpt_GetTimeRemaining    = 0U;
    CallCount_Gpt_StartTimer          = 0U;
    CallCount_Gpt_StopTimer           = 0U;
    CallCount_Gpt_EnableNotification  = 0U;
    CallCount_Gpt_DisableNotification = 0U;
}

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Gpt_GetVersionInfo
 * ---------------------------------------------------------------------- */

extern
void __real_Gpt_GetVersionInfo(Std_VersionInfoType* versioninfo);
void __wrap_Gpt_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    CallCount_Gpt_GetVersionInfo++;
    Log_Write(LOG_T, TAG, "Gpt_GetVersionInfo", "called %u times", CallCount_Gpt_GetVersionInfo);

    __real_Gpt_GetVersionInfo(versioninfo);
}

/* ----------------------------------------------------------------------
 * Gpt_Init
 * ---------------------------------------------------------------------- */

extern
void __real_Gpt_Init(const Gpt_ConfigType* ConfigPtr);
void __wrap_Gpt_Init(const Gpt_ConfigType* ConfigPtr)
{
    CallCount_Gpt_Init++;
    Log_Write(LOG_T, TAG, "Gpt_Init", "called %u times", CallCount_Gpt_Init);

    __real_Gpt_Init(ConfigPtr);
}

/* ----------------------------------------------------------------------
 * Gpt_DeInit
 * ---------------------------------------------------------------------- */

extern
void __real_Gpt_DeInit(void);
void __wrap_Gpt_DeInit(void)
{
    CallCount_Gpt_DeInit++;
    Log_Write(LOG_T, TAG, "Gpt_DeInit", "called %u times", CallCount_Gpt_DeInit);

    __real_Gpt_DeInit();
}

/* ----------------------------------------------------------------------
 * Gpt_GetTimeElapsed
 * ---------------------------------------------------------------------- */

extern
Gpt_ValueType __real_Gpt_GetTimeElapsed(Gpt_ChannelType Channel);
Gpt_ValueType __wrap_Gpt_GetTimeElapsed(Gpt_ChannelType Channel)
{
    CallCount_Gpt_GetTimeElapsed++;
    Log_Write(LOG_T, TAG, "Gpt_GetTimeElapsed", "called %u times", CallCount_Gpt_GetTimeElapsed);

    return __real_Gpt_GetTimeElapsed(Channel);
}

/* ----------------------------------------------------------------------
 * Gpt_GetTimeRemaining
 * ---------------------------------------------------------------------- */

extern
Gpt_ValueType __real_Gpt_GetTimeRemaining(Gpt_ChannelType Channel);
Gpt_ValueType __wrap_Gpt_GetTimeRemaining(Gpt_ChannelType Channel)
{
    CallCount_Gpt_GetTimeRemaining++;
    Log_Write(LOG_T, TAG, "Gpt_GetTimeRemaining", "called %u times", CallCount_Gpt_GetTimeRemaining);

    return __real_Gpt_GetTimeRemaining(Channel);
}

/* ----------------------------------------------------------------------
 * Gpt_StartTimer
 * ---------------------------------------------------------------------- */

extern
void __real_Gpt_StartTimer(Gpt_ChannelType Channel, Gpt_ValueType Value);
void __wrap_Gpt_StartTimer(Gpt_ChannelType Channel, Gpt_ValueType Value)
{
    CallCount_Gpt_StartTimer++;
    Log_Write(LOG_T, TAG, "Gpt_StartTimer", "called %u times", CallCount_Gpt_StartTimer);

    __real_Gpt_StartTimer(Channel, Value);
}

/* ----------------------------------------------------------------------
 * Gpt_StopTimer
 * ---------------------------------------------------------------------- */

extern
void __real_Gpt_StopTimer(Gpt_ChannelType Channel);
void __wrap_Gpt_StopTimer(Gpt_ChannelType Channel)
{
    CallCount_Gpt_StopTimer++;
    Log_Write(LOG_T, TAG, "Gpt_StopTimer", "called %u times", CallCount_Gpt_StopTimer);

    __real_Gpt_StopTimer(Channel);
}

/* ----------------------------------------------------------------------
 * Gpt_EnableNotification
 * ---------------------------------------------------------------------- */

extern
void __real_Gpt_EnableNotification(Gpt_ChannelType Channel);
void __wrap_Gpt_EnableNotification(Gpt_ChannelType Channel)
{
    CallCount_Gpt_EnableNotification++;
    Log_Write(LOG_T, TAG, "Gpt_EnableNotification", "called %u times", CallCount_Gpt_EnableNotification);

    __real_Gpt_EnableNotification(Channel);
}

/* ----------------------------------------------------------------------
 * Gpt_DisableNotification
 * ---------------------------------------------------------------------- */

extern
void __real_Gpt_DisableNotification(Gpt_ChannelType Channel);
void __wrap_Gpt_DisableNotification(Gpt_ChannelType Channel)
{
    CallCount_Gpt_DisableNotification++;
    Log_Write(LOG_T, TAG, "Gpt_DisableNotification", "called %u times", CallCount_Gpt_DisableNotification);

    __real_Gpt_DisableNotification(Channel);
}

/* ----------------------------------------------------------------------
 * Gpt_SetMode
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Gpt_DisableWakeup
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Gpt_EnableWakeup
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Gpt_CheckWakeup
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ----------------------------------------------------------------------
 * Gpt_GetPredefTimerValue
 * ---------------------------------------------------------------------- */

/* 未実装 */

/* ======================================================================
 * Internal Functions
 * ====================================================================== */
