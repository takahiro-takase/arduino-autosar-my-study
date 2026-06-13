/**
 * \file    Port_Hw.cpp
 * \brief   Port ハードウェア依存層 実装 (Arduino GPIO ラッパー)
 * \details Arduino の pinMode を直接呼び出す唯一のファイル（ポート方向設定）。
 *          Det.cpp / Can_Hw.cpp / Dio_Hw.cpp と同じ方針で
 *          C/C++ 境界を一箇所に集約する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include <Arduino.h>
#include "Port_Hw.h"

extern "C" {

void Port_Hw_SetPinDirection(Port_PinType pin, Port_PinDirectionType direction)
{
    uint8_t mode;
    if (direction == PORT_PIN_OUT)
    {
        mode = OUTPUT;
    }
    else if (direction == PORT_PIN_IN_PULLUP)
    {
        mode = INPUT_PULLUP;
    }
    else
    {
        mode = INPUT;
    }
    pinMode((uint8_t)pin, mode);
}

} /* extern "C" */
