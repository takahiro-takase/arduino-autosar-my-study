/**
 * \file    WdgM_Cfg.h
 * \brief   ウォッチドッグマネージャ プリコンパイル設定 (AUTOSAR SWS_WdgM 準拠)
 * \details WdgM が監視する Supervised Entity と Alive/Logical Supervision パラメータを定義する。
 *
 *          Alive Supervision の仕組み:
 *            WdgM_MainFunction が WDGM_SUPERVISION_CYCLE_MS ごとに呼ばれるとき、
 *            監視対象が WdgM_CheckpointReached() を
 *            WDGM_EXPECTED_ALIVE_INDICATIONS 回以上呼んでいれば OK、
 *            0 回（下回った）ならば FAILED とみなす。
 *
 *          Logical Supervision の仕組み:
 *            WdgM_CheckpointReached() が呼ばれるたびに、直前に報告された
 *            チェックポイントからの遷移が許可テーブル (WdgM_PBCfg.c の
 *            WdgM_TransitionCfgType 配列) に含まれるか即座に確認する。
 *            許可されない遷移（順序違反）が来た場合は即座に FAILED と判定する。
 *
 *          Supervised Entity:
 *            WDGM_ENTITY_ENGINE — App_EngineManager_Run (3000ms 周期)
 *              Alive  : 監視サイクル 6000ms の間に 1 回以上 CheckpointReached が来ることを期待する。
 *              Logical: WDGM_CP_ENGINE_START → WDGM_CP_ENGINE_END → (次サイクルの) START
 *                       の順序のみを許可する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef WDGM_CFG_H
#define WDGM_CFG_H

/** 監視対象エンティティ総数 */
#define WDGM_SUPERVISED_ENTITY_COUNT     1U

/** 監視対象エンティティ ID: App_EngineManager_Run */
#define WDGM_ENTITY_ENGINE               0U

/** Alive Supervision サイクル時間 (ms)
 *  WdgM_MainFunction の呼び出し周期と一致させること (Os_PBCfg.c Task 7)。
 *  タスク周期 3000ms の 2 倍に設定し、サイクル内に 2 回の Runnable 実行を許容する。 */
#define WDGM_SUPERVISION_CYCLE_MS        6000UL

/** サイクル内に期待する CheckpointReached 呼び出し最小回数 */
#define WDGM_EXPECTED_ALIVE_INDICATIONS  1U

/**
 * AVR ハードウェアウォッチドッグのタイムアウト。
 * WdgM.c で <avr/wdt.h> の WDTO_8S（8000ms）として直接使用する
 * （本ファイルを AVR ヘッダに依存させないため、列挙値はここでは定義しない）。
 * WDGM_SUPERVISION_CYCLE_MS (6000ms) より長く設定すること。
 * 健全なサイクルで wdt_reset() が呼ばれなければ、この時間内に MCU がリセットされる。
 */
#define WDGM_HW_WATCHDOG_TIMEOUT_MS  8000UL

/* -----------------------------------------------------------------------
 * 論理監視 (Logical Supervision) チェックポイント ID
 * Entity 0 (App_EngineManager_Run) のプログラムフロー上の 2 点を表す。
 * ----------------------------------------------------------------------- */
/** Run() 開始直後（RTE 読み取り前）に到達するチェックポイント */
#define WDGM_CP_ENGINE_START  0U
/** Run() 終了直前（CAN 送信後）に到達するチェックポイント */
#define WDGM_CP_ENGINE_END    1U

/** 「まだ一度もチェックポイントが来ていない」ことを示す遷移元の特別値。
 *  起動直後のみ有効な遷移元として許可テーブルに登場する。 */
#define WDGM_CP_INITIAL       0xFFU

#endif /* WDGM_CFG_H */
