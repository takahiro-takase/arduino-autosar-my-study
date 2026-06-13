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
void Port_Init(void)
{
    Port_Hw_SetPinDirection(PORT_PIN_LED_RUNNING, PORT_PIN_OUT);
    Port_Hw_SetPinDirection(PORT_PIN_LED_FAULT,   PORT_PIN_OUT);
    Port_Hw_SetPinDirection(PORT_PIN_LED_WARNING,  PORT_PIN_OUT);
    Port_Hw_SetPinDirection(PORT_PIN_BUTTON,       PORT_PIN_IN_PULLUP);
    DET_LOGI(TAG, "Init pins=%u", (unsigned)PORT_PIN_COUNT);
}

/**
 * \brief   指定ピンの方向を動的に変更する。
 *
 * \ServiceID      {0x03}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Port_SetPinDirection(Port_PinType Pin, Port_PinDirectionType Direction)
{
    Port_Hw_SetPinDirection(Pin, Direction);
}
