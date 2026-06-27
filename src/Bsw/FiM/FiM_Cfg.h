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
 * 機能 ID (FID) 定義
 * ASW (App_EngineManager / App_WarningIndicator) が
 * Rte_Call_FiM_GetFunctionPermission() の引数として使用する。
 * ----------------------------------------------------------------------- */
#define FIM_FID_RUNNING_LED  0U  /**< RUNNING LED (D6) 点灯機能       */
#define FIM_FID_BUTTON_ACK   1U  /**< 警告確認ボタンによる FAULT 解除 */
#define FIM_FUNCTION_COUNT   2U  /**< 管理する機能の総数              */

#endif /* FIM_CFG_H */
