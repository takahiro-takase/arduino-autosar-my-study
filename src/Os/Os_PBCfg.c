/**
 * \file    Os_PBCfg.c
 * \brief   OS ポストビルドコンフィグ 定義
 * \details プロジェクトで使用するタスクテーブルを定義する。
 *          AUTOSAR 環境ではコンフィギュレーションツールが自動生成するファイル
 *          に相当する。
 *
 *          タスク一覧:
 *            Task 0: Can_Isr                      1 ms  — CAN 受信ポーリング
 *            Task 1: CanTp_MainFunction            1 ms  — タイムアウト監視・CF 送信
 *            Task 2: Rte_ScheduleRunnables      3000 ms  — エンジン Runnable 起動
 *            Task 3: Rte_ScheduleWarningIndicator 500 ms — 警告灯 Runnable 起動
 *            Task 4: CanSM_MainFunction            10 ms  — Bus-Off 回復タイマ監視
 *            Task 5: Com_MainFunction             100 ms  — RX 受信デッドライン監視
 *            Task 6: IoHwAb_MainFunction           10 ms  — ボタンデバウンスサンプリング
 *            Task 7: WdgM_MainFunction           6000 ms  — Alive Supervision 評価
 *
 *          周期の根拠:
 *            Can_Isr / CanTp_MainFunction は CAN フレーム到着の応答性と
 *            CanTp タイムアウト精度のため 1 ms とする。
 *            App_EngineManager_Run は RTE コントラクト (3 秒周期) に従い 3000 ms とする。
 *            App_WarningIndicator_Run は LED 点滅半周期 (500 ms ON/OFF) のため 500 ms とする。
 *            CanSM_MainFunction は Bus-Off 回復タイマの精度を確保するため 10 ms とする。
 *            IoHwAb_MainFunction はデバウンス閾値 40ms (4 サンプル × 10ms) を実現するため 10 ms とする。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "Os_PBCfg.h"
#include "Os_Cfg.h"

/* タスク本体の前方宣言 (各モジュールのヘッダをここでインクルードする代わりに
 * extern 宣言を使用し、Os が各 BSW モジュールに依存しないようにする)  */
extern void Can_Isr(void);
extern void CanTp_MainFunction(void);
extern void Rte_ScheduleRunnables(void);
extern void Rte_ScheduleWarningIndicator(void);
extern void CanSM_MainFunction(void);
extern void Com_MainFunction(void);
extern void IoHwAb_MainFunction(void);
extern void WdgM_MainFunction(void);

/* -----------------------------------------------------------------------
 * タスクテーブル
 * インデックスがそのままタスク ID に対応する。
 * 実行順序はインデックス昇順 (優先度なし)。
 * ----------------------------------------------------------------------- */
static const Os_TaskType Os_TaskTable[OS_TASK_COUNT] =
{
    /* Task 0 */ { Can_Isr,                      1U    },  /* 1 ms    : CAN 受信ポーリング        */
    /* Task 1 */ { CanTp_MainFunction,           1U    },  /* 1 ms    : CanTp タイムアウト監視    */
    /* Task 2 */ { Rte_ScheduleRunnables,        3000U },  /* 3000 ms : エンジン Runnable         */
    /* Task 3 */ { Rte_ScheduleWarningIndicator, 500U  },  /* 500 ms  : 警告灯 Runnable           */
    /* Task 4 */ { CanSM_MainFunction,           10U   },  /* 10 ms   : BusOff 回復タイマ監視     */
    /* Task 5 */ { Com_MainFunction,             100U  },  /* 100 ms  : COM 受信デッドライン監視  */
    /* Task 6 */ { IoHwAb_MainFunction,          10U   },  /* 10 ms   : ボタンデバウンスサンプリング */
    /* Task 7 */ { WdgM_MainFunction,            6000U }   /* 6000 ms : Alive Supervision 評価    */
};

/* -----------------------------------------------------------------------
 * ポストビルドコンフィグインスタンス (EcuM が Os_Init に渡す)
 * ----------------------------------------------------------------------- */
const Os_ConfigType Os_Config =
{
    Os_TaskTable,
    OS_TASK_COUNT
};
