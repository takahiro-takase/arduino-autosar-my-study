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

/* ======================================================================
 * Includes
 * ====================================================================== */

/* ======================================================================
 * Definitions
 * ====================================================================== */

/* -----------------------------------------------------------------------
 * DET（Default Error Tracer）関連定数
 *
 * SWS_Adc 7.6.1 Development Error 表（Table 3）に基づく開発エラーコード。
 * ModuleId は SWS 本文には明記されないため、AUTOSAR_TR_BSWModuleList
 * （Release 4.3.1、docs/ 配下）の「List of Basic Software Modules」表で
 * ADC Driver (Adc) に割り当てられた固定値 123 を使う。
 *
 * 本実装は、AUTOSAR の ADC のグループ・バッファ方式の API
 * （Adc_SetupResultBuffer / Adc_StartGroupConversion / Adc_ReadGroup /
 * Adc_GetGroupStatus）で、1 グループ・1 チャネルのソフトウェアトリガの
 * ワンショット変換だけを扱う（以前は、実仕様に存在しない独自 API の
 * Adc_ReadChannel() で即時に読み取っていた。2026-10 に置き換え）。
 * ----------------------------------------------------------------------- */
/** AUTOSAR ADC Driver の ModuleId（AUTOSAR_TR_BSWModuleList 参照、固定値 123） */
#define ADC_MODULE_ID  123U

/** エラーコード（SWS_Adc Table 3 より）。UNINIT/PARAM_GROUP/PARAM_POINTER は開発エラー、BUSY/IDLE/BUFFER_UNINIT は実行時エラー。 */
#define ADC_E_UNINIT         0x0AU
#define ADC_E_BUSY           0x0BU
#define ADC_E_IDLE           0x0CU
#define ADC_E_PARAM_POINTER  0x14U
#define ADC_E_PARAM_GROUP    0x15U
#define ADC_E_BUFFER_UNINIT  0x19U

/** ApiId（既存の Doxygen \ServiceID タグと一致させる） */
/* cppcheck-suppress misra-c2012-2.5 */
#define ADC_API_ID_INIT  0x00U          /**< [SWS_Adc_00365] Adc_Init と同じ ServiceID */
#define ADC_API_ID_DEINIT  0x01U
#define ADC_API_ID_SETUP_RESULT_BUFFER  0x0CU
#define ADC_API_ID_START_GROUP_CONVERSION  0x02U
#define ADC_API_ID_READ_GROUP  0x04U
#define ADC_API_ID_GET_GROUP_STATUS  0x09U
#define ADC_API_ID_GET_VERSION_INFO  0x0AU

/** バージョン情報（SWS_Adc、Com/E2EXf/PduR 等の既存モジュールと同じ命名規則） */
#define ADC_VENDOR_ID          0U
#define ADC_SW_MAJOR_VERSION   1U
#define ADC_SW_MINOR_VERSION   0U
#define ADC_SW_PATCH_VERSION   0U

/* @@GEN-BEGIN adc-channels  生成元: config/data/Adc.json  （自動生成: 手編集禁止） */
/** Arduino UNO アナログ入力 A0。
 *  可変抵抗（ポテンショメータ）等を接続して電圧値を読み取る。 */
#define ADC_CHANNEL_SENSOR  0U

/** ADC チャネルグループ（AdcGroup）。1 グループ = 1 チャネル、ソフトウェアトリガ、
 *  ワンショット変換、単一アクセス。ADC_GROUP_SENSOR はセンサ入力グループ（ADC_CHANNEL_SENSOR を変換）、
 *  ADC_GROUP_COUNT はグループ数。 */
#define ADC_GROUP_SENSOR  0U
#define ADC_GROUP_COUNT   1U

/** Arduino UNO の ADC 分解能は 10-bit（0〜1023）。10-bit ADC 最大生値。 */
#define ADC_RESOLUTION_MAX  1023U

/** Arduino UNO の ADC 基準電圧は 5V（5000mV）。基準電圧 [mV]。
 *  analogRead(0) = 1023 のとき 5000mV となる。 */
#define ADC_REF_VOLTAGE_MV  5000U
/* @@GEN-END adc-channels */

#endif /* ADC_CFG_H */
