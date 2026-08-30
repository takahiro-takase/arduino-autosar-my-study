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
 *                                       CanSM_MainFunction・NvM_MainFunction・
 *                                       MemIf_MainFunction・Nm_MainFunction
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
 * DET（Default Error Tracer）関連定数
 *
 * SWS_BswM 7.6.1 Development Errors 表（Table 3）に基づく開発エラーコード。
 * ModuleId は SWS 本文には明記されないため、AUTOSAR_TR_BSWModuleList
 * （Release 4.3.1、docs/ 配下）の「List of Basic Software Modules」表で
 * BSW Mode Manager (BswM) に割り当てられた固定値 42 を使う。
 * ----------------------------------------------------------------------- */

/** AUTOSAR BSW Mode Manager の ModuleId（AUTOSAR_TR_BSWModuleList 参照、固定値 42） */
#define BSWM_MODULE_ID  42U

/** 開発エラーコード（SWS_BswM 7.6.1 表より、実際に使用する分のみ） */
#define BSWM_E_NO_INIT               0x01U
#define BSWM_E_REQ_MODE_OUT_OF_RANGE 0x05U
#define BSWM_E_PARAM_CONFIG          0x06U
#define BSWM_E_PARAM_POINTER         0x07U

/** ApiId（各関数の Doxygen \ServiceID タグと一致させること。値は SWS 8.x 章の
 *  「Service ID[hex]」記載を実測して確認済み） */
#define BSWM_API_ID_INIT                  0x00U
#define BSWM_API_ID_GET_VERSION_INFO      0x01U
#define BSWM_API_ID_ECUM_CURRENT_STATE    0x0FU
#define BSWM_API_ID_COMM_CURRENT_MODE     0x0EU

/** バージョン情報（SWS_BswM_00003、Com/E2EXf/PduR 等の既存モジュールと同じ命名規則） */
#define BSWM_VENDOR_ID          0U
#define BSWM_SW_MAJOR_VERSION   1U
#define BSWM_SW_MINOR_VERSION   0U
#define BSWM_SW_PATCH_VERSION   0U

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
#define BSWM_OS_TASK_SECOC_MAIN     16U /**< SecOC_MainFunctionTx     (100 ms)  */
#define BSWM_OS_TASK_MEMIF_MAIN     17U /**< MemIf_MainFunction     (10 ms)   */

/* -----------------------------------------------------------------------
 * タスクビットマスク (1ビット = 1タスク; ビット位置 = タスク ID)
 * タスク数が 16 を超えるため uint32 を使用する（BswM_PBCfg.h の
 * BswM_RuleType.TaskMask 参照。Task 16 (SecOC_MainFunctionTx) 追加時に
 * uint16 では bit 16 を表現できず、いかなる BswM ルールからも制御不能に
 * なっていたバグを修正した経緯がある）。
 * ----------------------------------------------------------------------- */

/** 全タスク (bits 0〜17) */
#define BSWM_TASK_MASK_ALL  0x3FFFFUL

/** アプリ Runnable タスク: RTE_ENGINE + RTE_WARNING
 *  POST_RUN 時に停止し、アプリロジックを凍結する */
#define BSWM_TASK_MASK_APP  ((uint32)((1UL << BSWM_OS_TASK_RTE_ENGINE) | \
                                      (1UL << BSWM_OS_TASK_RTE_WARNING)))

/** BSW タスク = ALL & ~APP (後処理・診断・CAN 受信を継続するため残す) */
#define BSWM_TASK_MASK_BSW  ((uint32)(BSWM_TASK_MASK_ALL & (uint32)(~BSWM_TASK_MASK_APP)))

/**
 * SHUTDOWN 時に停止するタスク = ALL & ~WDGM_TRIGGER & ~CAN_READ & ~CAN_WAKEUP
 *                                & ~CANSM_MAIN & ~NVM_MAIN & ~NM_MAIN。
 *
 * WdgM_TriggerHwWatchdog だけは SHUTDOWN 後も動かし続ける必要がある。
 * Renesas RA の IWDT は一度有効化すると無効化する手段がなく
 * (Wdg_Hw.cpp 参照)、誰もリフレッシュしなければ SHUTDOWN 後も
 * HW タイムアウトで MCU がリセットされてしまう
 * (WdgM_SupervisionSuppressed が立っているため、Task 10 さえ動いていれば
 * 無条件にリフレッシュを継続し、実害なく HW を満たし続けられる)。
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
 *
 * MemIf_MainFunction も同じ理由で動かし続ける必要がある。NvM_MainFunction は
 * MemIf_Write() でジョブを「開始」するだけで、実際の物理バイト書き込みを
 * 1 バイトずつ進めるのは MemIf_MainFunction（Fee 経由）である。
 * NvM_MainFunction だけを SHUTDOWN 中も動かして MemIf_MainFunction を
 * 止めてしまうと、NvM がジョブを開始したまま永久に MEMIF_JOB_PENDING を
 * 待ち続け、EEPROM への永続化が完了しない（詳細は NvM.c / MemIf.c を参照）。
 *
 * Nm_MainFunction も SHUTDOWN 中動かし続ける必要がある。CanSM の実物理
 * スリープ（Can_SetControllerMode(CAN_T_SLEEP)）は Nm が Bus-Sleep Mode へ
 * 到達した通知（ComM_Nm_BusSleepMode() 経由で CanSM_RequestComMode(NO_COM)
 * が呼ばれる）を受けて初めて行われる設計（協調スリープ、Nm.c/ComM.c/
 * CanSM.c 参照）に変更したため、Nm の状態機械タイマ
 * （Prepare Bus-Sleep Mode の Wait-Bus-Sleep Timer 等）がここで停止すると
 * Nm が Bus-Sleep Mode へ二度と到達できず、CAN コントローラが永久に
 * 物理スリープしなくなる。また Bus-Sleep/Prepare Bus-Sleep 中に他ノードの
 * NM フレームを受信して Repeat Message State へ戻った場合も、その後の
 * 周期送信・状態遷移判定は本タスクの中でしか行われないため、これを止めると
 * Nm がその状態に永久に固着してしまう（実機で確認された不具合）。
 *
 * SecOC_MainFunctionTx はこの「動かし続ける」リストに含まれない
 * （Com_MainFunction 等と同様、SHUTDOWN 中は停止してよいタスクの扱い。
 * SHUTDOWN 中は Com_MainFunction 自体が停止して SecOC_TxPending[] を
 * 誰も立てなくなるため送信要求は発生しないが、無効化しておくのが本来の
 * 設計意図であり、BSWM_TASK_MASK_ALL に bit 16 を含めることで
 * この無効化対象に含める）。
 */
#define BSWM_TASK_MASK_SHUTDOWN  ((uint32)(BSWM_TASK_MASK_ALL \
                                            & (uint32)(~((1UL << BSWM_OS_TASK_WDGM_TRIGGER) \
                                                        | (1UL << BSWM_OS_TASK_CAN_READ) \
                                                        | (1UL << BSWM_OS_TASK_CAN_WAKEUP) \
                                                        | (1UL << BSWM_OS_TASK_CANSM_MAIN) \
                                                        | (1UL << BSWM_OS_TASK_NVM_MAIN) \
                                                        | (1UL << BSWM_OS_TASK_MEMIF_MAIN) \
                                                        | (1UL << BSWM_OS_TASK_NM_MAIN)))))

/* -----------------------------------------------------------------------
 * ルール数
 * ----------------------------------------------------------------------- */
#define BSWM_RULE_COUNT  8U

/** モードソース数（BswM_ModeSrcType の列挙値数。複合条件ルール評価用の
 *  モードキャッシュ配列 BswM_ModeSrcCache[] のサイズに使う）。 */
#define BSWM_MODE_SRC_COUNT  2U

/** 1 ルールが持てる条件数の上限（BswMArgumentRef [ECUC_BswM_00820] は
 *  実 AUTOSAR では Multiplicity=1..* だが、本プロジェクトは AND/OR 2 条件までの
 *  簡略化とする（本プロジェクトが持つモードソースが ECUM/COMM の 2 つ
 *  だけであり、それ以上の条件数を要する使用実績がないため）。 */
#define BSWM_RULE_MAX_CONDITIONS  2U

#endif /* BSWM_CFG_H */
