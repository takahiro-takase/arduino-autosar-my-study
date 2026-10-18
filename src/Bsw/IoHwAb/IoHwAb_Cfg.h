/**
 * \file    IoHwAb_Cfg.h
 * \brief   IoHwAb プリコンパイル設定
 * \details ボタンのデバウンス・固着判定のサンプル数と、ADC 電圧低下の閾値を定義する。
 *          値は config/data/IoHwAb.json から生成する。サンプル数は、時間 [ms] を
 *          IoHwAb_MainFunction を呼ぶタスクの周期（config/data/Os.json）で割って求める。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * 
ote    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef IOHWAB_CFG_H
#define IOHWAB_CFG_H

/* ======================================================================
 * Includes
 * ====================================================================== */

/* ======================================================================
 * Definitions
 * ====================================================================== */

/* -----------------------------------------------------------------------
 * DET（Default Error Tracer）関連定数
 *
 * AUTOSAR_SWS_IOHardwareAbstraction.pdf 7.6.1 Development Errors 章:
 * IoHwAb の開発エラー分類は AUTOSAR 標準では規定されず、「報告したいエラーの
 * 定義自体を実装者に一任する」("Up to the implementer to define error he
 * wants to report") とされている（この記述自体に SWS 番号は付与されて
 * いない）。ModuleId のみ AUTOSAR_TR_BSWModuleList（Release 4.3.1、
 * docs/ 配下）の「List of Basic Software Modules」表で IO Hardware
 * Abstraction (IoHwAb) に割り当てられた固定値 254 を使う。エラーコード自体は
 * 本プロジェクト独自に、Dio/Adc 経由の呼び出しや IoHwAb_GetVersionInfo() で
 * NULL 出力ポインタが渡された場合のみを対象として定義する。
 * ----------------------------------------------------------------------- */

/** AUTOSAR IO Hardware Abstraction の ModuleId（AUTOSAR_TR_BSWModuleList 参照、固定値 254） */
#define IOHWAB_MODULE_ID  254U

/** 開発エラーコード（実装者定義。SWS の例示値 0x01 をそのまま踏襲） */
#define IOHWAB_E_PARAM_POINTER  0x01U

/** ApiId（既存の Doxygen \ServiceID タグと一致させる）。
 *  IOHWAB_API_ID_GET_VERSION_INFO は [SWS_IoHwAb_00120] の Service ID（0x10）。 */
#define IOHWAB_API_ID_GET_VERSION_INFO  0x10U
#define IOHWAB_API_ID_BUTTON_GET_LEVEL  0xC2U
#define IOHWAB_API_ID_ADC_GET_VALUE_MV  0xC6U

/** バージョン情報（Com/E2EXf/PduR 等の既存モジュールと同じ命名規則） */
#define IOHWAB_VENDOR_ID          0U
#define IOHWAB_SW_MAJOR_VERSION   1U
#define IOHWAB_SW_MINOR_VERSION   0U
#define IOHWAB_SW_PATCH_VERSION   0U

/* @@GEN-BEGIN iohwab-params  生成元: config/data/IoHwAb.json  （自動生成: 手編集禁止） */
/** ボタン確定に必要な連続一致サンプル数 (4 × 10ms = 40ms) */
#define IOHWAB_BUTTON_DEBOUNCE_COUNT  4U

/** ボタン固着と判定するまでの確定押下継続サンプル数 (500 × 10ms = 5000ms) */
#define IOHWAB_BUTTON_STUCK_COUNT     500U

/** ADC 電圧低下と判定する閾値 [mV] */
#define IOHWAB_ADC_LOW_VOLT_THRESHOLD_MV  1000U
/* @@GEN-END iohwab-params */

#endif /* IOHWAB_CFG_H */
