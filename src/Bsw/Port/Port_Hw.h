/**
 * \file    Port_Hw.h
 * \brief   Port ハードウェア依存層 内部インタフェース
 * \details Port.c（純粋 C）と Port_Hw.cpp（Arduino ラッパー）の境界を定義する。
 *          このヘッダは Port モジュール内部専用であり、上位層から直接参照しない。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef PORT_HW_H
#define PORT_HW_H

#include "Port.h"

#ifdef __cplusplus
extern "C" {
#endif

void Port_Hw_SetPinDirection(Port_PinType pin, Port_PinDirectionType direction);

#ifdef __cplusplus
}
#endif

#endif /* PORT_HW_H */
