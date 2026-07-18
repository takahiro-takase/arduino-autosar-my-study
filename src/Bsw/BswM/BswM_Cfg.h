/**
 * \file    BswM_Cfg.h
 * \brief   BSW モードマネージャ プリコンパイル設定 (AUTOSAR SWS_BswM 準拠)
 * \details BswM ルールテーブルで使用するタスク ID とタスクビットマスクを定義する。
 *          タスク ID は Os_PBCfg.c のタスクテーブルインデックスと一致させること。
 *
 *          タスクビットマスク:
 *            BSWM_TASK_MASK_ALL      — 全タスク (RUN 時に一括起動)
 *            BSWM_TASK_MASK_APP      — アプリ Runnable のみ (POST_RUN 時に停止)
 *            BSWM_TASK_MASK_BSW      — BSW タスクのみ (後処理用に継続動作)
 *            BSWM_TASK_MASK_SHUTDOWN — SHUTDOWN 時に停止するタスク
 *                                      (WdgM_TriggerHwWatchdog・
 *                                       Can_MainFunction_Read・
 *                                       Can_MainFunction_Wakeup・
 *                                       CanSM_MainFunction・NvM_MainFunction
 *                                       だけは除外)
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
#define BSWM_OS_TASK_CAN_READ       0U  /**< Can_MainFunction_Read (1 ms)   */
#define BSWM_OS_TASK_CANTP_MAIN     1U  /**< CanTp_MainFunction   (1 ms)    */
#define BSWM_OS_TASK_RTE_ENGINE     2U  /**< Rte_ScheduleRunnables (3000 ms) */
#define BSWM_OS_TASK_RTE_WARNING    3U  /**< Rte_ScheduleWarningIndicator (500 ms) */
#define BSWM_OS_TASK_CANSM_MAIN     4U  /**< CanSM_MainFunction   (10 ms)   */
#define BSWM_OS_TASK_COM_MAIN       5U  /**< Com_MainFunction     (100 ms)  */
#define BSWM_OS_TASK_IOHWAB_MAIN    6U  /**< IoHwAb_MainFunction  (10 ms)   */
#define BSWM_OS_TASK_WDGM_MAIN      7U  /**< WdgM_MainFunction    (6000 ms) */
#define BSWM_OS_TASK_DCM_MAIN       8U  /**< Dcm_MainFunction     (1000 ms) */
#define BSWM_OS_TASK_FIM_MAIN       9U  /**< FiM_MainFunction     (100 ms)  */
#define BSWM_OS_TASK_WDGM_TRIGGER   10U /**< WdgM_TriggerHwWatchdog (1000 ms) */
#define BSWM_OS_TASK_NM_MAIN        11U /**< Nm_MainFunction        (200 ms)  */
#define BSWM_OS_TASK_NVM_MAIN       12U /**< NvM_MainFunction       (10 ms)   */
#define BSWM_OS_TASK_CAN_TX_CONF    13U /**< Can_MainFunction_Write (1 ms)    */
#define BSWM_OS_TASK_CAN_BUSOFF     14U /**< Can_MainFunction_BusOff (1 ms)   */
#define BSWM_OS_TASK_CAN_WAKEUP     15U /**< Can_MainFunction_Wakeup (1 ms)   */

/* -----------------------------------------------------------------------
 * タスクビットマスク (1ビット = 1タスク; ビット位置 = タスク ID)
 * タスク数が 8 を超えるため uint16 を使用する。
 * ----------------------------------------------------------------------- */

/** 全タスク (bits 0〜15) */
#define BSWM_TASK_MASK_ALL  0xFFFFU

/** アプリ Runnable タスク: RTE_ENGINE + RTE_WARNING
 *  POST_RUN 時に停止し、アプリロジックを凍結する */
#define BSWM_TASK_MASK_APP  ((uint16)((1U << BSWM_OS_TASK_RTE_ENGINE) | \
                                      (1U << BSWM_OS_TASK_RTE_WARNING)))

/** BSW タスク = ALL & ~APP (後処理・診断・CAN 受信を継続するため残す) */
#define BSWM_TASK_MASK_BSW  ((uint16)(BSWM_TASK_MASK_ALL & (uint16)(~BSWM_TASK_MASK_APP)))

/**
 * SHUTDOWN 時に停止するタスク = ALL & ~WDGM_TRIGGER & ~CAN_READ & ~CAN_WAKEUP
 *                                & ~CANSM_MAIN & ~NVM_MAIN。
 *
 * WdgM_TriggerHwWatchdog だけは SHUTDOWN 後も動かし続ける必要がある。
 * Renesas RA の IWDT は一度有効化すると無効化する手段がなく
 * (WdgM_Hw.cpp 参照)、誰もリフレッシュしなければ SHUTDOWN 後も
 * HW タイムアウトで MCU がリセットされてしまう
 * (WdgM_SupervisionSuppressed が立っているため、Task 10 さえ動いていれば
 * 無条件にリフレッシュを継続し、実害なく HW を満たし続けられる)。
 * AVR では wdt_disable() が実際に機能するため本来は不要だが、
 * MCU に依存せず SHUTDOWN を安定した終端状態にするため両方で同じ扱いとする。
 *
 * Can_MainFunction_Read・Can_MainFunction_Wakeup も同様に SHUTDOWN 中
 * 動かし続ける必要がある。CAN 受信自体は真のハードウェア割り込み
 * (Can_Isr()) のため BswM のタスク無効化に関わらず常に起動するが、
 * ペンディングフラグの実処理（SPI 読み出し・CanIf 通知）はこの 2 つの
 * タスクに委譲しているため、無効化してしまうとウェイクアップ検出も
 * ウェイクアップ検証用フレームの受信処理も止まってしまう
 * （ComM の NO_COM 要求で CAN コントローラを実際にスリープさせた場合
 * （ボランタリスリープ）、バス活動によるウェイクアップ検出、および
 * ウェイクアップ検証中に届く診断フレームの受信処理は、この 2 つの
 * タスクだけが担う。詳細は Can.c を参照）。
 * Can_MainFunction_BusOff は SHUTDOWN 中（CAN コントローラは SLEEP か
 * Listen-Only）は判定条件 (CanState==CAN_CS_STARTED) が成立せず実質的に
 * 無意味なため、他の BSW タスクと同様に停止してよい。
 *
 * CanSM_MainFunction も SHUTDOWN 中動かし続ける必要がある。ウェイクアップ
 * 検証（CANSM_STATE_WAKEUP_VALIDATING、AUTOSAR EcuM Wakeup Validation
 * Protocol 相当）の検証タイムアウトを監視できるのがこのタスクだけのため
 * （詳細は CanSM.c を参照）。
 *
 * NvM_MainFunction も SHUTDOWN 中動かし続ける必要がある。SHUTDOWN へ
 * 遷移する直前に Dem が新規 DTC を確定して NvM_WriteBlock() の書き込み
 * ジョブが保留中のまま残っている可能性があり、このタスクを止めてしまうと
 * 次に RUN へ戻るまで（あるいは戻らないまま）EEPROM への永続化が完了しない。
 */
#define BSWM_TASK_MASK_SHUTDOWN  ((uint16)(BSWM_TASK_MASK_ALL \
                                            & (uint16)(~((1U << BSWM_OS_TASK_WDGM_TRIGGER) \
                                                        | (1U << BSWM_OS_TASK_CAN_READ) \
                                                        | (1U << BSWM_OS_TASK_CAN_WAKEUP) \
                                                        | (1U << BSWM_OS_TASK_CANSM_MAIN) \
                                                        | (1U << BSWM_OS_TASK_NVM_MAIN)))))

/* -----------------------------------------------------------------------
 * ルール数
 * ----------------------------------------------------------------------- */
#define BSWM_RULE_COUNT  3U

#endif /* BSWM_CFG_H */
