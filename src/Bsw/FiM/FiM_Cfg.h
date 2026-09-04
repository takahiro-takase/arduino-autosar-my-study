/**
 * \file    FiM_Cfg.h
 * \brief   機能抑止マネージャ プリコンパイル設定 (AUTOSAR SWS_FiM 準拠)
 * \details FiM が管理する機能 ID (FID) を定義する。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツールが生成する
 *          ファイルに相当する。
 *
 *          本プロジェクトで管理する機能:
 *            FIM_FID_RUNNING_LED — RUNNING LED (D6) の点灯機能。
 *              DEM_EVENT_CAN_BUSOFF が CONFIRMED の間は抑止する
 *              (EngineState は CAN 受信データ由来のため、Bus-Off 確定中は
 *               信頼できる値ではない)。
 *            FIM_FID_BUTTON_ACK  — 警告確認ボタンによる FAULT 解除機能。
 *              DEM_EVENT_BUTTON_STUCK が CONFIRMED の間は抑止する
 *              (ボタン固着確定中の押下は物理的な固着による偽信号の可能性があり、
 *               意図した確認操作とみなせない)。
 *
 *          FID と監視対象イベントの対応は FiM_PBCfg.c の FiM_Functions[] で定義する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef FIM_CFG_H
#define FIM_CFG_H

/* -----------------------------------------------------------------------
 * DET（Default Error Tracer）関連定数
 *
 * SWS_Fim 7.3.1 Development Errors 表（Table 7.1）に基づく開発エラーコード。
 * ModuleId は SWS 本文には明記されないため、AUTOSAR_TR_BSWModuleList
 * （Release 4.3.1、docs/ 配下）の「List of Basic Software Modules」表で
 * Function Inhibition Manager (FiM) に割り当てられた固定値 11 を使う。
 * ----------------------------------------------------------------------- */

/** AUTOSAR Function Inhibition Manager の ModuleId
 *  （AUTOSAR_TR_BSWModuleList 参照、固定値 11） */
#define FIM_MODULE_ID  11U

/** 開発エラーコード（SWS_Fim 7.3.1 表より、実際に使用する分のみ） */
#define FIM_E_UNINIT           0x01U
#define FIM_E_FID_OUT_OF_RANGE 0x02U
#define FIM_E_PARAM_POINTER    0x04U

/** ApiId（各関数の Doxygen \ServiceID タグと一致させること。値は SWS 8.x 章の
 *  「Service ID[hex]」記載を実測して確認済み） */
#define FIM_API_ID_INIT                     0x00U
#define FIM_API_ID_GET_FUNCTION_PERMISSION  0x01U
#define FIM_API_ID_SET_FUNCTION_AVAILABLE   0x07U
#define FIM_API_ID_MAIN_FUNCTION            0x05U
#define FIM_API_ID_GET_VERSION_INFO         0x04U

/** バージョン情報（SWS_Fim_GetVersionInfo、Com/E2EXf/PduR 等の既存モジュールと同じ命名規則） */
#define FIM_VENDOR_ID          0U
#define FIM_SW_MAJOR_VERSION   1U
#define FIM_SW_MINOR_VERSION   0U
#define FIM_SW_PATCH_VERSION   0U

/* -----------------------------------------------------------------------
 * 機能 ID (FID) 定義
 * ASW (App_EngineManager / App_WarningIndicator) が
 * Rte_Call_FiM_GetFunctionPermission() の引数として使用する。
 * ----------------------------------------------------------------------- */
#define FIM_FID_RUNNING_LED  0U  /**< RUNNING LED (D6) 点灯機能       */
#define FIM_FID_BUTTON_ACK   1U  /**< 警告確認ボタンによる FAULT 解除 */
#define FIM_FUNCTION_COUNT   2U  /**< 管理する機能の総数              */

#endif /* FIM_CFG_H */
