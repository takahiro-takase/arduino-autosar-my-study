/**
 * \file    Det_Cfg.h
 * \brief   Det プリコンパイル設定 (AUTOSAR SWS_Det 準拠)
 * \details 出力するログの最低重要度を定義する。
 *          LogLevel は数値が小さいほど重要度が高い (E=0 が最重要)。
 *          DET_LOG_LEVEL 以下の数値 (＝同等以上に重要) のログのみ出力する。
 *
 *          例: DET_LOG_LEVEL を LOG_I にすると ERROR/WARN/INFO のみ出力し、
 *              TRACE/DEBUG は抑制する。LOG_T にすると TRACE（関数コールチェーン
 *              確認専用）まで出力するが DEBUG（詳細ログ）は抑制したままにできる。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef DET_CFG_H
#define DET_CFG_H

#ifndef DET_LOG_LEVEL
#  define DET_LOG_LEVEL  LOG_I  /**< 既定値: ERROR/WARN/INFO を出力、TRACE/DEBUG を抑制 */
#endif

/* -----------------------------------------------------------------------
 * DET（Default Error Tracer）自身の Det_GetVersionInfo 関連定数
 *
 * ModuleId は AUTOSAR_TR_BSWModuleList（Release 4.3.1、docs/ 配下）の
 * 「List of Basic Software Modules」表で Det に割り当てられた固定値 15。
 * ----------------------------------------------------------------------- */

/** AUTOSAR Det の ModuleId（AUTOSAR_TR_BSWModuleList 参照、固定値 15） */
#define DET_MODULE_ID  15U

/** 開発エラーコード（SWS_Det_00301、Det_GetVersionInfo の NULL チェックのみ使用） */
#define DET_E_PARAM_POINTER  0x01U

/** ApiId（各関数の Doxygen \ServiceID タグと一致させること。SWS_Det_00011 の
 *  「Service ID[hex]」記載を実測して確認済み） */
#define DET_API_ID_INIT               0x00U
#define DET_API_ID_START              0x02U
#define DET_API_ID_GET_VERSION_INFO  0x03U

/** バージョン情報（Com/E2EXf/PduR/Port 等の既存モジュールと同じ命名規則） */
#define DET_VENDOR_ID          0U
#define DET_SW_MAJOR_VERSION   1U
#define DET_SW_MINOR_VERSION   0U
#define DET_SW_PATCH_VERSION   0U

#endif /* DET_CFG_H */
