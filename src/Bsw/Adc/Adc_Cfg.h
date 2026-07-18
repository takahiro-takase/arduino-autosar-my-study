/**
 * \file    Adc_Cfg.h
 * \brief   ADC ドライバ プリコンパイル設定 (AUTOSAR SWS_ADC 準拠)
 * \details プロジェクトで使用するアナログ入力チャネルと
 *          ADC 変換パラメータを定義する。
 *          チャネル ID は Arduino のアナログピン番号に対応する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef ADC_CFG_H
#define ADC_CFG_H

/**
 * Arduino UNO アナログ入力 A0。
 * 可変抵抗（ポテンショメータ）等を接続して電圧値を読み取る。
 */
#define ADC_CHANNEL_SENSOR    0U    /**< A0: アナログセンサ入力チャネル     */

/**
 * Arduino UNO の ADC 分解能は 10-bit（0〜1023）。
 */
#define ADC_RESOLUTION_MAX  1023U   /**< 10-bit ADC 最大生値               */

/**
 * Arduino UNO の ADC 基準電圧は 5V（5000mV）。
 * analogRead(0) = 1023 のとき 5000mV となる。
 */
#define ADC_REF_VOLTAGE_MV  5000U   /**< 基準電圧 [mV]                     */

#endif /* ADC_CFG_H */
