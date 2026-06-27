/**
 * \file    BswM_Cfg.h
 * \brief   BSW モードマネージャ プリコンパイル設定 (AUTOSAR SWS_BswM 準拠)
 * \details BswM ルールテーブルで使用するタスク ID とタスクビットマスクを定義する。
 *          タスク ID は Os_PBCfg.c のタスクテーブルインデックスと一致させること。
 *
 *          タスクビットマスク:
 *            BSWM_TASK_MASK_ALL — 全タスク (RUN 時に一括起動)
 *            BSWM_TASK_MASK_APP — アプリ Runnable のみ (POST_RUN 時に停止)
 *            BSWM_TASK_MASK_BSW — BSW タスクのみ (後処理用に継続動作)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef BSWM_CFG_H
#define BSWM_CFG_H

/* -----------------------------------------------------------------------
 * Os タスク ID (Os_PBCfg.c のインデックスと一致させること)
 * ----------------------------------------------------------------------- */
#define BSWM_OS_TASK_CAN_ISR        0U  /**< Can_Isr              (1 ms)    */
#define BSWM_OS_TASK_CANTP_MAIN     1U  /**< CanTp_MainFunction   (1 ms)    */
#define BSWM_OS_TASK_RTE_ENGINE     2U  /**< Rte_ScheduleRunnables (3000 ms) */
#define BSWM_OS_TASK_RTE_WARNING    3U  /**< Rte_ScheduleWarningIndicator (500 ms) */
#define BSWM_OS_TASK_CANSM_MAIN     4U  /**< CanSM_MainFunction   (10 ms)   */
#define BSWM_OS_TASK_COM_MAIN       5U  /**< Com_MainFunction     (100 ms)  */
#define BSWM_OS_TASK_IOHWAB_MAIN    6U  /**< IoHwAb_MainFunction  (10 ms)   */
#define BSWM_OS_TASK_WDGM_MAIN      7U  /**< WdgM_MainFunction    (6000 ms) */
#define BSWM_OS_TASK_DCM_MAIN       8U  /**< Dcm_MainFunction     (1000 ms) */
#define BSWM_OS_TASK_FIM_MAIN       9U  /**< FiM_MainFunction     (100 ms)  */

/* -----------------------------------------------------------------------
 * タスクビットマスク (1ビット = 1タスク; ビット位置 = タスク ID)
 * タスク数が 8 を超えるため uint16 を使用する。
 * ----------------------------------------------------------------------- */

/** 全タスク (bits 0〜9) */
#define BSWM_TASK_MASK_ALL  0x3FFU

/** アプリ Runnable タスク: RTE_ENGINE + RTE_WARNING
 *  POST_RUN 時に停止し、アプリロジックを凍結する */
#define BSWM_TASK_MASK_APP  ((uint16)((1U << BSWM_OS_TASK_RTE_ENGINE) | \
                                      (1U << BSWM_OS_TASK_RTE_WARNING)))

/** BSW タスク = ALL & ~APP (後処理・診断・CAN 受信を継続するため残す) */
#define BSWM_TASK_MASK_BSW  ((uint16)(BSWM_TASK_MASK_ALL & (uint16)(~BSWM_TASK_MASK_APP)))

/* -----------------------------------------------------------------------
 * ルール数
 * ----------------------------------------------------------------------- */
#define BSWM_RULE_COUNT  3U

#endif /* BSWM_CFG_H */
