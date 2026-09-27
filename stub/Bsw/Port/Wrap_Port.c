/**
 * \file    Wrap_Port.c
 * \brief   `src/Bsw/Port/Port.c` 内の関数を対象とした wrap 実体
 *          （Wrap_Port.h 参照）。
 *
 * \details 変数を先頭の Global Variables セクションへ集約し、その後に
 *          Functions セクションで各 `__wrap_...` の実装をまとめる
 *          （`Wrap_Can.c` と同じ構成。`[[reference_wrap_stub_naming_convention]]`
 *          参照）。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Wrap_Port.h"
#include "Det.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "Port"

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * Global Variables
 * ====================================================================== */

uint32 CallCount_Port_Init                   = 0U;
uint32 CallCount_Port_SetPinDirection        = 0U;
uint32 CallCount_Port_RefreshPortDirection   = 0U;
uint32 CallCount_Port_GetVersionInfo         = 0U;
uint32 CallCount_Port_SetPinMode             = 0U;

/* ----------------------------------------------------------------------
 * WrapPort_Reset — 5関数すべての状態を一括で初期化する（Wrap_Port.h 参照）。
 * ---------------------------------------------------------------------- */

void WrapPort_Reset(void)
{
    CallCount_Port_Init                 = 0U;
    CallCount_Port_SetPinDirection      = 0U;
    CallCount_Port_RefreshPortDirection = 0U;
    CallCount_Port_GetVersionInfo       = 0U;
    CallCount_Port_SetPinMode           = 0U;
}

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * Port_Init
 * ---------------------------------------------------------------------- */

extern
void __real_Port_Init(const Port_ConfigType* ConfigPtr);
void __wrap_Port_Init(const Port_ConfigType* ConfigPtr)
{
    CallCount_Port_Init++;
    Log_Write(LOG_T, TAG, "Port_Init", "called %u times", CallCount_Port_Init);

    __real_Port_Init(ConfigPtr);
}

/* ----------------------------------------------------------------------
 * Port_SetPinDirection
 * ---------------------------------------------------------------------- */

extern
void __real_Port_SetPinDirection(Port_PinType Pin, Port_PinDirectionType Direction);
void __wrap_Port_SetPinDirection(Port_PinType Pin, Port_PinDirectionType Direction)
{
    CallCount_Port_SetPinDirection++;
    Log_Write(LOG_T, TAG, "Port_SetPinDirection", "called %u times", CallCount_Port_SetPinDirection);

    __real_Port_SetPinDirection(Pin, Direction);
}

/* ----------------------------------------------------------------------
 * Port_RefreshPortDirection
 * ---------------------------------------------------------------------- */

extern
void __real_Port_RefreshPortDirection(void);
void __wrap_Port_RefreshPortDirection(void)
{
    CallCount_Port_RefreshPortDirection++;
    Log_Write(LOG_T, TAG, "Port_RefreshPortDirection", "called %u times", CallCount_Port_RefreshPortDirection);

    __real_Port_RefreshPortDirection();
}

/* ----------------------------------------------------------------------
 * Port_GetVersionInfo
 * ---------------------------------------------------------------------- */

extern
void __real_Port_GetVersionInfo(Std_VersionInfoType* versioninfo);
void __wrap_Port_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    CallCount_Port_GetVersionInfo++;
    Log_Write(LOG_T, TAG, "Port_GetVersionInfo", "called %u times", CallCount_Port_GetVersionInfo);

    __real_Port_GetVersionInfo(versioninfo);
}

/* ----------------------------------------------------------------------
 * Port_SetPinMode
 * ---------------------------------------------------------------------- */

extern
void __real_Port_SetPinMode(Port_PinType Pin, Port_PinModeType Mode);
void __wrap_Port_SetPinMode(Port_PinType Pin, Port_PinModeType Mode)
{
    CallCount_Port_SetPinMode++;
    Log_Write(LOG_T, TAG, "Port_SetPinMode", "called %u times", CallCount_Port_SetPinMode);

    __real_Port_SetPinMode(Pin, Mode);
}

/* ======================================================================
 * Internal Functions
 * ====================================================================== */
