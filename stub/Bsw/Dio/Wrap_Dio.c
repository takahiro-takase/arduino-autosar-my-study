/**
 * \file    Wrap_Dio.c
 * \brief   `src/Bsw/Dio/Dio.c` 内の関数を対象とした wrap 実体
 *          （Wrap_Dio.h 参照）。
 *
 * \details 変数を先頭の Global Variables セクションへ集約し、その後に
 *          Functions セクションで各 `__wrap_...` の実装をまとめる
 *          （`Wrap_Can.c` と同じ構成。`[[reference_wrap_stub_naming_convention]]`
 *          参照）。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Wrap_Dio.h"
#include "Det.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "Dio"

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * Global Variables
 * ====================================================================== */

uint32 CallCount_Dio_ReadChannel        = 0U;
uint32 CallCount_Dio_WriteChannel       = 0U;
uint32 CallCount_Dio_ReadPort           = 0U;
uint32 CallCount_Dio_WritePort          = 0U;
uint32 CallCount_Dio_ReadChannelGroup   = 0U;
uint32 CallCount_Dio_WriteChannelGroup  = 0U;
uint32 CallCount_Dio_GetVersionInfo     = 0U;
uint32 CallCount_Dio_FlipChannel        = 0U;

/* ----------------------------------------------------------------------
 * WrapDio_Reset — 8関数すべての状態を一括で初期化する（Wrap_Dio.h 参照）。
 * ---------------------------------------------------------------------- */

void WrapDio_Reset(void)
{
    CallCount_Dio_ReadChannel       = 0U;
    CallCount_Dio_WriteChannel      = 0U;
    CallCount_Dio_ReadPort          = 0U;
    CallCount_Dio_WritePort         = 0U;
    CallCount_Dio_ReadChannelGroup  = 0U;
    CallCount_Dio_WriteChannelGroup = 0U;
    CallCount_Dio_GetVersionInfo    = 0U;
    CallCount_Dio_FlipChannel       = 0U;
}

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Dio_ReadChannel
 * ---------------------------------------------------------------------- */

extern
Dio_LevelType __real_Dio_ReadChannel(Dio_ChannelType channelId);
Dio_LevelType __wrap_Dio_ReadChannel(Dio_ChannelType channelId)
{
    CallCount_Dio_ReadChannel++;
    Log_Write(LOG_T, TAG, "Dio_ReadChannel", "called %u times", CallCount_Dio_ReadChannel);

    return __real_Dio_ReadChannel(channelId);
}

/* ----------------------------------------------------------------------
 * Dio_WriteChannel
 * ---------------------------------------------------------------------- */

extern
void __real_Dio_WriteChannel(Dio_ChannelType channelId, Dio_LevelType level);
void __wrap_Dio_WriteChannel(Dio_ChannelType channelId, Dio_LevelType level)
{
    CallCount_Dio_WriteChannel++;
    Log_Write(LOG_T, TAG, "Dio_WriteChannel", "called %u times", CallCount_Dio_WriteChannel);

    __real_Dio_WriteChannel(channelId, level);
}

/* ----------------------------------------------------------------------
 * Dio_ReadPort
 * ---------------------------------------------------------------------- */

extern
Dio_PortLevelType __real_Dio_ReadPort(Dio_PortType PortId);
Dio_PortLevelType __wrap_Dio_ReadPort(Dio_PortType PortId)
{
    CallCount_Dio_ReadPort++;
    Log_Write(LOG_T, TAG, "Dio_ReadPort", "called %u times", CallCount_Dio_ReadPort);

    return __real_Dio_ReadPort(PortId);
}

/* ----------------------------------------------------------------------
 * Dio_WritePort
 * ---------------------------------------------------------------------- */

extern
void __real_Dio_WritePort(Dio_PortType PortId, Dio_PortLevelType Level);
void __wrap_Dio_WritePort(Dio_PortType PortId, Dio_PortLevelType Level)
{
    CallCount_Dio_WritePort++;
    Log_Write(LOG_T, TAG, "Dio_WritePort", "called %u times", CallCount_Dio_WritePort);

    __real_Dio_WritePort(PortId, Level);
}

/* ----------------------------------------------------------------------
 * Dio_ReadChannelGroup
 * ---------------------------------------------------------------------- */

extern
Dio_PortLevelType __real_Dio_ReadChannelGroup(const Dio_ChannelGroupType* ChannelGroupIdPtr);
Dio_PortLevelType __wrap_Dio_ReadChannelGroup(const Dio_ChannelGroupType* ChannelGroupIdPtr)
{
    CallCount_Dio_ReadChannelGroup++;
    Log_Write(LOG_T, TAG, "Dio_ReadChannelGroup", "called %u times", CallCount_Dio_ReadChannelGroup);

    return __real_Dio_ReadChannelGroup(ChannelGroupIdPtr);
}

/* ----------------------------------------------------------------------
 * Dio_WriteChannelGroup
 * ---------------------------------------------------------------------- */

extern
void __real_Dio_WriteChannelGroup(const Dio_ChannelGroupType* ChannelGroupIdPtr, Dio_PortLevelType Level);
void __wrap_Dio_WriteChannelGroup(const Dio_ChannelGroupType* ChannelGroupIdPtr, Dio_PortLevelType Level)
{
    CallCount_Dio_WriteChannelGroup++;
    Log_Write(LOG_T, TAG, "Dio_WriteChannelGroup", "called %u times", CallCount_Dio_WriteChannelGroup);

    __real_Dio_WriteChannelGroup(ChannelGroupIdPtr, Level);
}

/* ----------------------------------------------------------------------
 * Dio_GetVersionInfo
 * ---------------------------------------------------------------------- */

extern
void __real_Dio_GetVersionInfo(Std_VersionInfoType* VersionInfo);
void __wrap_Dio_GetVersionInfo(Std_VersionInfoType* VersionInfo)
{
    CallCount_Dio_GetVersionInfo++;
    Log_Write(LOG_T, TAG, "Dio_GetVersionInfo", "called %u times", CallCount_Dio_GetVersionInfo);

    __real_Dio_GetVersionInfo(VersionInfo);
}

/* ----------------------------------------------------------------------
 * Dio_FlipChannel
 * ---------------------------------------------------------------------- */

extern
Dio_LevelType __real_Dio_FlipChannel(Dio_ChannelType channelId);
Dio_LevelType __wrap_Dio_FlipChannel(Dio_ChannelType channelId)
{
    CallCount_Dio_FlipChannel++;
    Log_Write(LOG_T, TAG, "Dio_FlipChannel", "called %u times", CallCount_Dio_FlipChannel);

    return __real_Dio_FlipChannel(channelId);
}

/* ======================================================================
 * Internal Functions
 * ====================================================================== */
