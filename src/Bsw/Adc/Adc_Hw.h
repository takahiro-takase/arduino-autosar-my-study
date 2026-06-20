/**
 * \file    Adc_Hw.h
 * \brief   ADC ハードウェア抽象化 内部インタフェース
 * \details Adc.c (純粋 C) と Adc_Hw.cpp (C++) の境界を定義する。
 *          Adc.c はこのヘッダを経由して C++ ラッパーを呼び出す。
 *          本ヘッダは Adc.c と Adc_Hw.cpp 以外からインクルードしないこと。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef ADC_HW_H
#define ADC_HW_H

#include "Std_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   指定チャネルのアナログ値を読み取る。
 *
 * \details Arduino の analogRead() を呼び出し、10-bit の生 ADC 値を返す。
 *          channel 0 = A0, 1 = A1, … に対応する。
 *
 * \param[in]  channel  アナログ入力チャネル番号（0 = A0）。
 *
 * \return  ADC 生値（0〜1023）。
 */
uint16 Adc_Hw_ReadChannel(uint8 channel);

#ifdef __cplusplus
}
#endif

#endif /* ADC_HW_H */
