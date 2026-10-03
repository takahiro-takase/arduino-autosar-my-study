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
 *            - 経過時間の判定に使う時間源は Gpt (専用チャネル、HW 割り込み駆動)
 *              であり、Os_Init() 内で自ら Gpt_StartTimer() を行う。呼び出し元
 *              (EcuM) は Gpt_Init() を Os_Init() より前に済ませておくこと
 *              （詳細は Os.c 冒頭のコメント参照）。
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

/** Os API の戻り値型 (AUTOSAR Os の StatusType)。E_OK は Std_Types.h。 */
typedef uint8 StatusType;

/** Os カウンタの ID 型 (AUTOSAR Os の CounterType)。 */
typedef uint8 CounterType;

/** Os カウンタの tick 値の型 (AUTOSAR Os の TickType。仕様は 24bit 以上)。 */
typedef uint32 TickType;

/** TickType への参照型 (AUTOSAR Os の TickRefType)。 */
typedef TickType* TickRefType;

/* [SWS_Os_00376]/[SWS_Os_00381]: 不正な CounterID。
 * [SWS_Os_00391]: 不正な Value。
 * 数値は SWS 4.3.1 では規定されないため本プロジェクトでの割り当て
 * （呼び出し側はシンボルで比較すること）。 */
#define E_OS_ID     3U
#define E_OS_VALUE  8U

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
 * \details 自身の Gpt チャネルを起動し、全タスクの「最終実行時刻」を
 *          現在のティック値に設定する。
 *          EcuM_Init() の末尾（全 BSW モジュール初期化後、Gpt_Init() は
 *          さらにその前に済んでいること）に呼び出すこと。
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
 * \details 全タスクを走査し、(現在のティック値 - 最終実行時刻) >= PeriodMs
 *          であればタスク関数を呼び出す。EcuM_MainFunction() から
 *          毎ループ呼び出すこと。
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

/**
 * \brief   指定タスクの有効・無効を切り替える。
 *
 * \details BswM がルール実行時に呼び出す。無効化されたタスクは
 *          Os_SchedulerStep() の走査対象から除外され実行されない。
 *          有効化されたタスクは次の Os_SchedulerStep() から再び実行対象になる。
 *          無効→有効へ遷移する瞬間、そのタスクの Os_LastRunMs[] を現在時刻に
 *          リセットする（長時間無効だったタスクが、再開直後に周期を待たず
 *          即座に実行されてしまう不具合の対策。詳細は Os.c 参照）。
 *
 * \param[in]  TaskId  タスク ID (Os_PBCfg.c のインデックス)。
 * \param[in]  Active  1U = 有効, 0U = 無効。
 *
 * \ServiceID      {0x05}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Os_SetTaskActive(uint8 TaskId, uint8 Active);

/**
 * \brief   Os カウンタの現在の tick 値を読み出す。
 *
 * \details [SWS_Os_00383]。SYSTEM_COUNTER は 1 tick = 1 ms で、Arduino の
 *          millis() と同じ座標系の値を返す。BSW モジュールが millis() を直接
 *          呼ばずに時刻を取得するための窓口（[SWS_Os_00377]）。
 *
 *          連続性: 値は Os_Init() の前後、および Gpt ティック停滞による
 *          millis() フォールバックの前後でも飛ばない。Os_Init() 前は
 *          millis() をそのまま返す。Os_Init() で Gpt 駆動の時間源との
 *          オフセットを確定し、フォールバック時にオフセットを更新する
 *          （Os.c 参照）。これにより、モジュールが保持する「前回時刻」を
 *          時間源の切り替えをまたいで差分計算してよい。
 *
 *          32bit でラップアラウンドする（約 49 日）。差分は unsigned の
 *          引き算で正しく求まる（GetElapsedValue() 参照）。
 *
 * \param[in]  CounterID  カウンタ ID（SYSTEM_COUNTER のみ有効）。
 * \param[out] Value      現在の tick 値。
 * \return     E_OK、または E_OS_ID（CounterID 不正）。Value が NULL の場合は
 *              E_OS_VALUE（SWS には無い防御的な追加）。
 *
 * \ServiceID      {0x10}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
StatusType GetCounterValue(CounterType CounterID, TickRefType Value);

/**
 * \brief   前回読み出した tick 値からの経過 tick 数を求める。
 *
 * \details [SWS_Os_00392]。*Value に前回の tick 値を渡すと、*ElapsedValue に
 *          経過 tick 数が入り、*Value は現在の tick 値に更新される
 *          （[SWS_Os_00382]/[SWS_Os_00460]）。32bit ラップアラウンドをまたいでも
 *          正しい（ただし 2 周以上経過した場合は検出できない、[SWS_Os_00533]）。
 *          SYSTEM_COUNTER の最大値は 0xFFFFFFFF のため、[SWS_Os_00391] の
 *          E_OS_VALUE（Value が最大値超過）は発生し得ない。
 *
 * \param[in]     CounterID     カウンタ ID（SYSTEM_COUNTER のみ有効）。
 * \param[in,out] Value         in: 前回の tick 値 / out: 現在の tick 値。
 * \param[out]    ElapsedValue  前回からの経過 tick 数。
 * \return        E_OK、または E_OS_ID（CounterID 不正）。Value / ElapsedValue が
 *                 NULL の場合は E_OS_VALUE（SWS には無い防御的な追加）。
 *
 * \ServiceID      {0x11}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
StatusType GetElapsedValue(CounterType CounterID, TickRefType Value, TickRefType ElapsedValue);

#ifdef OS_UNIT_TEST
/**
 * \brief   Os の初期化状態を未初期化へ戻す（単体テスト専用）。
 *
 * \details Os に DeInit 相当の API が無く、Os_Cfg・カウンタのオフセット・
 *          フォールバック状態が static のままテストケースをまたいで残るため、
 *          各テストの SetUp で呼ぶ（標準外の関数。Wdg_Test_ResetInitState() と
 *          同じ設計方針）。
 */
void Os_Test_ResetInitState(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* OS_H */
