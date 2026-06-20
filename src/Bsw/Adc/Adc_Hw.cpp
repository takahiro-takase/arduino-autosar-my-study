/**
 * \file    Adc_Hw.cpp
 * \brief   ADC ハードウェア抽象化 実装 (Arduino analogRead ラッパー)
 * \details Arduino の analogRead() API を純粋 C から呼び出せるよう
 *          C++ でラップする。本ファイルが Arduino API を呼ぶ唯一の ADC 関連ファイルである。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "Adc_Hw.h"
#include <Arduino.h>

/**
 * \brief   指定チャネルのアナログ値を読み取る。
 *
 * \details Arduino の analogRead() へ委譲する。
 *          A0 ピンはアナログ入力専用のため pinMode 設定は不要。
 *          変換時間は約 100 µs（ATmega328P ADC クロック 125 kHz 時）。
 *
 * \param[in]  channel  アナログ入力チャネル番号（0 = A0）。
 *
 * \return  ADC 生値（0〜1023）。
 */
uint16 Adc_Hw_ReadChannel(uint8 channel)
{
    return (uint16)analogRead(channel);
}
