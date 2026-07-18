/**
 * \file    Dio_Hw.cpp
 * \brief   Dio ハードウェア依存層 実装 (Arduino GPIO ラッパー)
 * \details Arduino の pinMode / digitalWrite を直接呼び出す唯一のファイル。
 *          Dio.c（純粋 C の AUTOSAR Dio モジュール）から呼び出される内部実装。
 *          Det.cpp / Can_Hw.cpp と同じ方針で C/C++ 境界を一箇所に集約する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include <Arduino.h>
#include "Dio_Hw.h"

extern "C" {

void Dio_Hw_WriteChannel(Dio_ChannelType channelId, Dio_LevelType level)
{
    digitalWrite((uint8_t)channelId, (level != DIO_LOW) ? HIGH : LOW);
}

Dio_LevelType Dio_Hw_ReadChannel(Dio_ChannelType channelId)
{
    return (digitalRead((uint8_t)channelId) == HIGH) ? DIO_HIGH : DIO_LOW;
}

} /* extern "C" */
