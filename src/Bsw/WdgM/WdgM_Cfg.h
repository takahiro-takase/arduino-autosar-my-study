/**
 * \file    WdgM_Cfg.h
 * \brief   ウォッチドッグマネージャ プリコンパイル設定 (AUTOSAR SWS_WdgM 準拠)
 * \details WdgM が監視する Supervised Entity と Alive/Logical Supervision パラメータを定義する。
 *
 *          Alive Supervision の仕組み:
 *            WdgM_MainFunction が WDGM_SUPERVISION_CYCLE_MS ごとに呼ばれるとき、
 *            監視対象が WdgM_CheckpointReached() を、エンティティごとに設定された
 *            WDGM_*_EXPECTED_ALIVE_INDICATIONS 回以上呼んでいれば OK、
 *            下回ったならば FAILED とみなす。
 *
 *          Logical Supervision の仕組み:
 *            WdgM_CheckpointReached() が呼ばれるたびに、直前に報告された
 *            チェックポイントからの遷移が許可テーブル (WdgM_PBCfg.c の
 *            WdgM_TransitionCfgType 配列) に含まれるか即座に確認する。
 *            許可されない遷移（順序違反）が来た場合は即座に FAILED と判定する。
 *
 *          Deadline Supervision の仕組み (Alive/Logical に続く 3 つ目のアルゴリズム):
 *            WdgM_CheckpointReached() が呼ばれるたびに、直前のチェックポイントから
 *            今回のチェックポイントまでの実際の経過時間を計測し、許可テーブル
 *            (WdgM_PBCfg.c の WdgM_DeadlineCfgType 配列) に設定された
 *            [MinMs, MaxMs] の範囲内かを確認する。範囲外（遅すぎる／速すぎる）
 *            なら即座に FAILED と判定する。
 *
 *          Supervised Entity:
 *            WDGM_ENTITY_ENGINE — App_EngineManager_Run (3000ms 周期)
 *              Alive  : 監視サイクル 6000ms の間に 1 回以上 CheckpointReached が来ることを期待する
 *                       (期待呼び出し回数 約2回に対し最小1回、大きめの余裕を持たせている)。
 *              Logical: WDGM_CP_ENGINE_START → WDGM_CP_ENGINE_END → (次サイクルの) START
 *                       の順序のみを許可する。
 *            WDGM_ENTITY_WARNING — App_WarningIndicator_Run (500ms 周期)
 *              Alive  : 監視サイクル 6000ms の間に 6 回以上 CheckpointReached が来ることを期待する
 *                       (期待呼び出し回数 約12回に対し最小6回。ENGINE と同じ「期待値の半分」という
 *                       比率で許容し、協調スケジューラのジッタを吸収する)。
 *              Logical: WDGM_CP_WARNING_START → WDGM_CP_WARNING_END → (次サイクルの) START
 *                       の順序のみを許可する。
 *              周期が ENGINE (3000ms) と異なる 2 つ目のエンティティを登録することで、
 *              WdgM のローカル判定（エンティティごとに独立）→グローバル判定
 *              （WdgM_TriggerHwWatchdog が全エンティティの結果を集約）という、
 *              単一エンティティ構成では確認できない挙動を実機で検証できる。
 *
 *          HW ウォッチドッグの二重周期設計:
 *            判定（Alive/Logical/Deadline Supervision）は WDGM_SUPERVISION_CYCLE_MS
 *            ごと、HW ウォッチドッグへの実際のリフレッシュ（trigger）は
 *            WDGM_HW_TRIGGER_CYCLE_MS ごとと、意図的に周期を分離している。
 *            詳細は WDGM_HW_WATCHDOG_TIMEOUT_MS のコメントを参照。
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
#define WDGM_SUPERVISED_ENTITY_COUNT     2U

/** 監視対象エンティティ ID: App_EngineManager_Run (3000ms 周期) */
#define WDGM_ENTITY_ENGINE               0U

/** 監視対象エンティティ ID: App_WarningIndicator_Run (500ms 周期) */
#define WDGM_ENTITY_WARNING              1U

/** Alive Supervision サイクル時間 (ms)。全エンティティ共通で、
 *  WdgM_MainFunction の呼び出し周期と一致させること (Os_PBCfg.c Task 7)。
 *  ENGINE のタスク周期 3000ms の 2 倍に設定し、サイクル内に 2 回の Runnable
 *  実行を許容する（WARNING は 500ms 周期のため、同じ 6000ms サイクル内に
 *  約 12 回の実行が期待される）。 */
#define WDGM_SUPERVISION_CYCLE_MS        6000UL

/** ENGINE: サイクル内に期待する CheckpointReached 呼び出し最小回数
 *  (期待値 約2回に対し最小1回)。 */
#define WDGM_ENGINE_EXPECTED_ALIVE_INDICATIONS   1U

/** WARNING: サイクル内に期待する CheckpointReached 呼び出し最小回数
 *  (期待値 約12回に対し最小6回、ENGINE と同じ「期待値の半分」の比率)。 */
#define WDGM_WARNING_EXPECTED_ALIVE_INDICATIONS  6U

/**
 * グローバルレベルの EXPIRED 許容判定サイクル数
 * （AUTOSAR WdgMExpiredSupervisionCycleTol [ECUC_WdgM_00329] 相当）。
 *
 * AUTOSAR 実仕様（docs/AUTOSAR_SWS_WatchdogManager.pdf で確認済み）では、
 * いずれかのエンティティが FAILED/EXPIRED になっても HW ウォッチドッグへの
 * リフレッシュ（WdgIf_SetTriggerCondition）は即座には止まらない:
 *   - SWS_WdgM_00119/00120/00121: Global Supervision Status が
 *     OK / FAILED / EXPIRED のいずれであっても、同一の WdgMTriggerCondition
 *     でリフレッシュを呼び続ける。
 *   - SWS_WdgM_00122: リフレッシュを 0（停止）にするのは
 *     WDGM_GLOBAL_STATUS_STOPPED に達したときだけ。
 *   - STOPPED に到達するには、EXPIRED のまま WdgMExpiredSupervisionCycleTol
 *     回分の判定サイクルを消費する必要がある（SWS_WdgM_00216/00217 等）。
 *     ゼロ回に設定すれば単発の異常で即 STOPPED になるが、それは意図的な
 *     コンフィグレーション上の選択であり、デフォルトの挙動ではない。
 *
 * 本実装は Local Supervision Status を OK/FAILED の 2 値に単純化しており
 * （実仕様の FAILED/EXPIRED の区別、per-SE の
 * WdgMFailedAliveSupervisionRefCycleTol は実装していない）、その代わりに
 * この 1 段のグローバル許容サイクル数だけを持つ。値 2 は、実機で発生した
 * NvM の EEPROM ブロッキング書き込み（数百ms、最大で確認された elapsed=741ms）
 * のような単発の一時的なスケジューラ遅延を、判定サイクル
 * (WDGM_SUPERVISION_CYCLE_MS=6000ms) 1 回分の猶予で吸収できるように選んだ
 * （このマージン自体は Deadline 許容範囲の拡張で別途吸収済みだが、
 * 想定外の一時的な遅延に対する多重の安全余裕として、この許容サイクルも
 * 追加する）。0 に設定すれば、この猶予機構導入前と同じ「単発の異常で即座に
 * リフレッシュを止める」旧来の挙動に戻る。
 */
#define WDGM_EXPIRED_SUPERVISION_CYCLE_TOL   2U

/**
 * HW ウォッチドッグの trigger（リフレッシュ）周期。
 * WdgM_TriggerHwWatchdog の呼び出し周期と一致させること (Os_PBCfg.c 新設タスク)。
 * WDGM_SUPERVISION_CYCLE_MS（Alive/Logical/Deadline の判定サイクル）とは
 * 意図的に分離している。理由は WDGM_HW_WATCHDOG_TIMEOUT_MS のコメントを参照。
 */
#define WDGM_HW_TRIGGER_CYCLE_MS     1000UL

/**
 * HW ウォッチドッグのタイムアウト。
 * WdgM_Hw.cpp で AVR は wdt_enable(WDTO_4S)、Renesas RA は WDT.begin(4000) として
 * 直接使用する（本ファイルを MCU 固有ヘッダに依存させないため、列挙値はここでは
 * 定義しない）。
 *
 * WDGM_HW_TRIGGER_CYCLE_MS (1000ms) より十分長く設定し、trigger 呼び出しの
 * ジッタで誤ってタイムアウトしないマージンを持たせること。
 *
 * 以前は「WDGM_SUPERVISION_CYCLE_MS (6000ms) より長く」という制約だったが、
 * Renesas RA4M1 の IWDT は最大タイムアウトが約 5592ms しかなく、6000ms 周期の
 * 判定サイクルにそのままリフレッシュを同期させると仕様上不可能だった
 * （AVR の WDTO_8S=8000ms では問題にならなかった）。
 * このため HW ウォッチドッグの trigger（リフレッシュ）を Alive/Logical/Deadline
 * の判定サイクルから切り離し、判定は WDGM_SUPERVISION_CYCLE_MS（6000ms）ごと、
 * リフレッシュは WDGM_HW_TRIGGER_CYCLE_MS（1000ms）ごとの、判定結果を参照する
 * だけの軽量処理（WdgM_TriggerHwWatchdog）に分離した。
 * 実車の AUTOSAR WdgM も、Wdg への trigger 周期と WdgMSupervisionCycle を
 * 独立して設定できる点で同じ考え方を採る。
 */
#define WDGM_HW_WATCHDOG_TIMEOUT_MS  4000UL

/* -----------------------------------------------------------------------
 * 論理監視 (Logical Supervision) チェックポイント ID
 * Entity 0 (App_EngineManager_Run) のプログラムフロー上の 2 点を表す。
 * ----------------------------------------------------------------------- */
/** Run() 開始直後（RTE 読み取り前）に到達するチェックポイント */
#define WDGM_CP_ENGINE_START  0U
/** Run() 終了直前（CAN 送信後）に到達するチェックポイント */
#define WDGM_CP_ENGINE_END    1U

/* -----------------------------------------------------------------------
 * 論理監視 (Logical Supervision) チェックポイント ID
 * Entity 1 (App_WarningIndicator_Run) のプログラムフロー上の 2 点を表す。
 * チェックポイント ID はエンティティごとに独立した名前空間で解釈される
 * (WdgM_LastCheckpoint[] がエンティティ単位の配列のため、ENGINE の
 * 0/1 と数値が重複しても混同しない) が、設定ファイルを読む際の視認性を
 * 優先し、あえて重複しない値を割り当てている。
 * ----------------------------------------------------------------------- */
/** Run() 開始直後（RTE 読み取り前）に到達するチェックポイント */
#define WDGM_CP_WARNING_START  2U
/** Run() 終了直前（CAN 送信後）に到達するチェックポイント */
#define WDGM_CP_WARNING_END    3U

/** 「まだ一度もチェックポイントが来ていない」ことを示す遷移元の特別値。
 *  起動直後のみ有効な遷移元として許可テーブルに登場する。 */
#define WDGM_CP_INITIAL       0xFFU

/* -----------------------------------------------------------------------
 * Deadline Supervision 許容経過時間 (Entity 0: ENGINE 用)
 * ----------------------------------------------------------------------- */

/** START→END（Run() 1 回分の処理時間）の許容範囲。
 *  通常は数 ms で完了するはずの処理に上限を設け、無限ループや
 *  ブロッキング処理による異常な遅延を検出する。下限は本処理にとって
 *  特に意味を持たないため 0（実質チェックなし）とする。 */
#define WDGM_ENGINE_DEADLINE_START_TO_END_MIN_MS   0UL
#define WDGM_ENGINE_DEADLINE_START_TO_END_MAX_MS   500UL

/** END→START（次サイクルの Run() 呼び出しまでの間隔）の許容範囲。
 *  タスク周期 3000ms (Os_PBCfg.c Task 2) を中心に余裕を持たせる
 *  (協調スケジューラのため、他タスクの実行による多少のジッタを許容する)。
 *  上限は当初 3500ms（±500ms）だったが、Dem が新規 DTC 確定時に
 *  NvM_WriteBlock() 経由で EEPROM へ同期書き込みするブロッキング処理
 *  （Renesas RA の NvM_Hw_WriteBlock() はバイト単位の EEPROM.update() ループで、
 *  フラッシュ消去/書き込みを伴う）が数百ms 単位の協調スケジューラ停止を
 *  引き起こすことが実機で判明した（WARNING エンティティの Deadline 違反、
 *  elapsed=741ms として検出）ため、同じ要因で ENGINE 側も将来違反し得ると
 *  判断し、上限に余裕を持たせた。 */
#define WDGM_ENGINE_DEADLINE_END_TO_START_MIN_MS   2500UL
#define WDGM_ENGINE_DEADLINE_END_TO_START_MAX_MS   4500UL

/* -----------------------------------------------------------------------
 * Deadline Supervision 許容経過時間 (Entity 1: WARNING 用)
 * ----------------------------------------------------------------------- */

/** START→END（Run() 1 回分の処理時間）の許容範囲。
 *  LED 出力のみの単純な処理のため ENGINE より短い上限とする。 */
#define WDGM_WARNING_DEADLINE_START_TO_END_MIN_MS   0UL
#define WDGM_WARNING_DEADLINE_START_TO_END_MAX_MS   200UL

/** END→START（次サイクルの Run() 呼び出しまでの間隔）の許容範囲。
 *  タスク周期 500ms (Os_PBCfg.c Task 3) を中心に余裕を持たせる。
 *  当初は ±200ms（700ms 上限）だったが、実機で Dem の新規 DTC 確定時
 *  （NvM_WriteBlock() 経由の EEPROM 同期書き込み、Renesas RA では
 *  NvM_Hw_WriteBlock() がバイト単位の EEPROM.update() ループでフラッシュ
 *  消去/書き込みを伴う）による協調スケジューラの一時停止で
 *  elapsed=741ms の Deadline 違反 → HW ウォッチドッグ未リフレッシュ →
 *  実機リセットが発生することが判明した。この遅延はタスク自身の異常
 *  ではなく、他 BSW モジュールのブロッキング処理由来であるため、
 *  実測値に十分な余裕（約2倍）を持たせて上限を引き上げた。 */
#define WDGM_WARNING_DEADLINE_END_TO_START_MIN_MS   300UL
#define WDGM_WARNING_DEADLINE_END_TO_START_MAX_MS   1500UL

#endif /* WDGM_CFG_H */
