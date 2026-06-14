/**
 * \file    WdgM_Cfg.h
 * \brief   ウォッチドッグマネージャ プリコンパイル設定 (AUTOSAR SWS_WdgM 準拠)
 * \details WdgM が監視する Supervised Entity と Alive Supervision パラメータを定義する。
 *
 *          Alive Supervision の仕組み:
 *            WdgM_MainFunction が WDGM_SUPERVISION_CYCLE_MS ごとに呼ばれるとき、
 *            監視対象が WdgM_CheckpointReached() を
 *            WDGM_EXPECTED_ALIVE_INDICATIONS 回以上呼んでいれば OK、
 *            0 回（下回った）ならば FAILED とみなす。
 *
 *          Supervised Entity:
 *            WDGM_ENTITY_ENGINE — App_EngineManager_Run (3000ms 周期)
 *              監視サイクル 6000ms の間に 1 回以上 CheckpointReached が来ることを期待する。
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

#endif /* WDGM_CFG_H */
