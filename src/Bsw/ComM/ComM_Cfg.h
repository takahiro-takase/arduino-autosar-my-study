/**
 * \file    ComM_Cfg.h
 * \brief   ComM プリコンパイル設定 (AUTOSAR SWS_ComM 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef COMM_CFG_H
#define COMM_CFG_H

/* -----------------------------------------------------------------------
 * DET（Default Error Tracer）関連定数
 *
 * SWS_ComM_00234 (Table 4: Error classification) に基づく開発エラーコード。
 * ModuleId は SWS 本文には明記されないため、AUTOSAR_TR_BSWModuleList
 * （Release 4.3.1、docs/ 配下）の「List of Basic Software Modules」表で
 * COM Manager (ComM) に割り当てられた固定値 12 を使う。
 *
 * [SWS_ComM_00612/00858]: ComM_Init/GetVersionInfo/GetStatus を除く全 API は、
 * 未初期化状態で呼ばれた場合に COMM_E_UNINIT を報告しなければならない。
 * 本実装は ComM_ConfigType を保持しない簡略設計のため、初期化済みかどうかを
 * 示す専用フラグ（ComM_Initialized）を別途持つ。
 * ----------------------------------------------------------------------- */

/** AUTOSAR COM Manager の ModuleId（AUTOSAR_TR_BSWModuleList 参照、固定値 12） */
#define COMM_MODULE_ID  12U

/** 開発エラーコード（SWS_ComM_00234 表より、実際に使用する分のみ） */
#define COMM_E_UNINIT             0x1U
#define COMM_E_WRONG_PARAMETERS   0x2U
#define COMM_E_PARAM_POINTER      0x3U

/** ApiId（各関数の Doxygen \ServiceID タグと一致させること。値は SWS 8.x 章の
 *  「Service ID[hex]」記載を実測して確認済み） */
#define COMM_API_ID_INIT                    0x01U
#define COMM_API_ID_REQUEST_COM_MODE        0x05U
#define COMM_API_ID_GET_CURRENT_COM_MODE    0x08U
#define COMM_API_ID_BUS_SM_MODE_INDICATION  0x33U
#define COMM_API_ID_NM_NETWORK_MODE          0x18U
#define COMM_API_ID_NM_PREPARE_BUS_SLEEP_MODE 0x19U
#define COMM_API_ID_NM_BUS_SLEEP_MODE       0x1AU
#define COMM_API_ID_MAIN_FUNCTION           0x60U
#define COMM_API_ID_DEINIT                  0x02U
#define COMM_API_ID_GET_VERSION_INFO        0x10U
#define COMM_API_ID_GET_STATUS              0x03U

/** バージョン情報（SWS_ComM、Com/E2EXf/PduR 等の既存モジュールと同じ命名規則） */
#define COMM_VENDOR_ID          0U
#define COMM_SW_MAJOR_VERSION   1U
#define COMM_SW_MINOR_VERSION   0U
#define COMM_SW_PATCH_VERSION   0U

/** 管理チャネル数 */
#define COMM_CHANNEL_COUNT  1U

/** 管理ユーザ数 */
#define COMM_USER_COUNT     2U

/** チャネル 0 ID */
#define COMM_CHANNEL_0      0U

/** ユーザ 0 ID（EcuM が RUN 中は常時 FULL_COM を要求する。実質「エンジン運用アプリ」の代表） */
#define COMM_USER_0         0U

/** ユーザ 1 ID（Dcm。extendedSession の間だけ FULL_COM を要求し、
 *  defaultSession 復帰（明示要求 / S3 タイムアウト / ECUReset）で解除する。
 *  「診断ツールが繋がっている間はバスを落とさない」という実車の要件を再現する） */
#define COMM_USER_1         1U

#endif /* COMM_CFG_H */
