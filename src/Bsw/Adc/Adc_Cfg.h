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

/* -----------------------------------------------------------------------
 * DET（Default Error Tracer）関連定数
 *
 * SWS_Adc 7.6.1 Development Error 表（Table 3）に基づく開発エラーコード。
 * ModuleId は SWS 本文には明記されないため、AUTOSAR_TR_BSWModuleList
 * （Release 4.3.1、docs/ 配下）の「List of Basic Software Modules」表で
 * ADC Driver (Adc) に割り当てられた固定値 123 を使う。
 *
 * Adc_Init は [SWS_Adc_00365] のシグネチャ（ConfigPtr 引数）に準拠させて
 * いるが、本実装の Adc_ReadChannel() は実際の SWS_Adc（Adc_ReadGroup 等の
 * グループ・バッファベース API 群）には存在しない、本プロジェクト独自の
 * 単一チャネル即時読み取り関数のため、個々の SWS 関数の ApiId とは対応
 * しない。NULL ポインタチェックのエラーコードのみ、Adc_SetupResultBuffer
 * 等の「バッファポインタ NULL」に対応する値 (0x14) を流用する。
 * ----------------------------------------------------------------------- */

/** AUTOSAR ADC Driver の ModuleId（AUTOSAR_TR_BSWModuleList 参照、固定値 123） */
#define ADC_MODULE_ID  123U

/** 開発エラーコード（SWS_Adc Table 3 より） */
#define ADC_E_PARAM_POINTER  0x14U

/** ApiId（既存の Doxygen \ServiceID タグと一致させる） */
#define ADC_API_ID_INIT  0x00U          /**< [SWS_Adc_00365] Adc_Init と同じ ServiceID */
#define ADC_API_ID_READ_CHANNEL  0xD0U
#define ADC_API_ID_GET_VERSION_INFO  0x0AU

/** バージョン情報（SWS_Adc、Com/E2EXf/PduR 等の既存モジュールと同じ命名規則） */
#define ADC_VENDOR_ID          0U
#define ADC_SW_MAJOR_VERSION   1U
#define ADC_SW_MINOR_VERSION   0U
#define ADC_SW_PATCH_VERSION   0U

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
