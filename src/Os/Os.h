/**
 * \file    Os.h
 * \brief   OS タイムトリガスケジューラ 公開インタフェース (AUTOSAR SWS_Os 準拠)
 * \details AUTOSAR OS の Basic Task + OsAlarm (時間トリガ) を
 *          Arduino ベアメタル環境向けに簡略化した実装のインタフェース。
 *
 *          設計方針:
 *            - 各タスクは関数ポインタ (Os_TaskFuncType) と実行周期 (PeriodMs) を持つ。
 *            - Os_SchedulerStep() を毎ループ呼び出すことで、経過時間を確認し
 *              周期が到来したタスクを順番に実行する (協調スケジューリング)。
 *            - 「いつ実行するか」はすべて Os_PBCfg.c のタスクテーブルで管理し、
 *              EcuM や RTE に周期管理コードを持たせない。
 *
 *          AUTOSAR 実装との主な違い (学習用簡略化):
 *            - プリエンプションなし (run-to-completion)
 *            - タスク優先度なし (テーブル順に実行)
 *            - OsEvent / OsResource 未実装
 *            - OsAlarm は固定周期のみ (相対アラームのみ相当)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef OS_H
#define OS_H

#include "Std_Types.h"
#include "Os_Cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * 型定義
 * ----------------------------------------------------------------------- */

/** タスク本体関数の型 (AUTOSAR Os の TASK マクロに相当) */
typedef void (*Os_TaskFuncType)(void);

/**
 * \brief   タスク記述子 — 1 タスクの周期と実行関数を保持する。
 * \details AUTOSAR の OsTask + OsAlarm コンフィグに相当する。
 *          Os_PBCfg.c のテーブルに列挙する。
 */
typedef struct
{
    Os_TaskFuncType  Func;      /**< タスク本体関数ポインタ */
    uint32           PeriodMs;  /**< 実行周期 (ms); 0 = 毎ステップ実行 */
} Os_TaskType;

/**
 * \brief   OS ポストビルドコンフィグ型
 * \details Os_Init() に渡すコンフィグ構造体。Os_PBCfg.c でインスタンス化する。
 */
typedef struct
{
    const Os_TaskType*  Tasks;      /**< タスク記述子配列の先頭 */
    uint8               TaskCount;  /**< タスク数 */
} Os_ConfigType;

/* -----------------------------------------------------------------------
 * 公開 API
 * ----------------------------------------------------------------------- */

/**
 * \brief   OS スケジューラを初期化する。
 * \details 全タスクの「最終実行時刻」を現在の millis() 値に設定する。
 *          EcuM_Init() の末尾（全 BSW モジュール初期化後）に呼び出すこと。
 *
 * \param[in]  ConfigPtr  ポストビルドコンフィグへのポインタ。NULL 禁止。
 *
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Os_Init(const Os_ConfigType* ConfigPtr);

/**
 * \brief   スケジューラを 1 ステップ進め、周期が到来したタスクを実行する。
 * \details 全タスクを走査し、(millis() - 最終実行時刻) >= PeriodMs であれば
 *          タスク関数を呼び出す。EcuM_MainFunction() から毎ループ呼び出すこと。
 *
 *          タスクは配列インデックス順に実行される (優先度なし)。
 *          各タスクは完了まで実行される (プリエンプションなし)。
 *
 * \pre        Os_Init() が正常に完了していること。
 *
 * \ServiceID      {0x04}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Os_SchedulerStep(void);

#ifdef __cplusplus
}
#endif

#endif /* OS_H */
