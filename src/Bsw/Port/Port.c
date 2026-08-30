/**
 * \file    Port.c
 * \brief   ポートドライバ 実装 (AUTOSAR SWS_Port 準拠)
 * \details ピンの方向設定を担う MCAL Port モジュール。
 *          Arduino 依存コードを持たない純粋 C ファイル。
 *          ピン操作は Port_Hw.cpp（Arduino GPIO ラッパー）へ委譲する。
 *
 *          Dio モジュールとの責務分担:
 *            本モジュール (Port) — ピン方向（INPUT / OUTPUT）設定
 *            Dio モジュール      — ピン値の読み書き
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "Port.h"
#include "Port_Hw.h"
#include "Det.h"

#define TAG "Port"

typedef struct
{
    Port_PinType          Pin;
    Port_PinDirectionType Direction;
} Port_PinConfigType;

/** PORT_PIN_COUNT と要素数が食い違えば初期化子の過不足でコンパイルエラーになる
 *  （Port_Cfg.h にピンを追加する際は両方を同時に更新する必要がある）。 */
static const Port_PinConfigType Port_PinConfig[PORT_PIN_COUNT] =
{
    { PORT_PIN_LED_RUNNING, PORT_PIN_OUT },
    { PORT_PIN_LED_FAULT,   PORT_PIN_OUT },
    { PORT_PIN_LED_WARNING, PORT_PIN_OUT },
    { PORT_PIN_BUTTON,      PORT_PIN_IN_PULLUP },
};

/** Port_Init() と Port_RefreshPortDirection() の両方から呼ばれる（Port.h 参照）。 */
static void Port_ApplyConfiguredDirections(void)
{
    for (uint8 i = 0U; i < PORT_PIN_COUNT; i++)
    {
        Port_Hw_SetPinDirection(Port_PinConfig[i].Pin, Port_PinConfig[i].Direction);
    }
}

/**
 * \brief   Port モジュールを初期化する。
 *
 * \details Port_Cfg.h で定義されたすべてのピンを所定方向に設定する。
 *          以降の Dio_WriteChannel() 呼び出しより前に完了している必要がある。
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Port_Init(const Port_ConfigType* ConfigPtr)
{
    DET_LOGT(TAG, "called");
    (void)ConfigPtr; /* 本プロジェクトは Port_Cfg.h の静的テーブルを直接参照する（Port.h 参照） */
    Port_ApplyConfiguredDirections();
    DET_LOGI(TAG, "Init pins=%u", (unsigned)PORT_PIN_COUNT);
}

/**
 * \brief   全ピンの方向を設定方向へ再適用する（詳細は Port.h 参照）。
 *
 * \ServiceID      {0x02}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Port_RefreshPortDirection(void)
{
    DET_LOGT(TAG, "called");
    Port_ApplyConfiguredDirections();
    DET_LOGI(TAG, "RefreshPortDirection pins=%u", (unsigned)PORT_PIN_COUNT);
}

/**
 * \brief   指定ピンの方向を動的に変更する。
 *
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Port_SetPinDirection(Port_PinType Pin, Port_PinDirectionType Direction)
{
    DET_LOGT(TAG, "called");
    Port_Hw_SetPinDirection(Pin, Direction);
}

void Port_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    DET_LOGT(TAG, "called");
    if (versioninfo == NULL)
    {
        Det_ReportError(PORT_MODULE_ID, 0U, PORT_API_ID_GET_VERSION_INFO, PORT_E_PARAM_POINTER);
        return;
    }

    versioninfo->vendorID         = PORT_VENDOR_ID;
    versioninfo->moduleID         = PORT_MODULE_ID;
    versioninfo->sw_major_version = PORT_SW_MAJOR_VERSION;
    versioninfo->sw_minor_version = PORT_SW_MINOR_VERSION;
    versioninfo->sw_patch_version = PORT_SW_PATCH_VERSION;
}
