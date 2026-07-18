/**
 * \file    WdgM.h
 * \brief   ウォッチドッグマネージャ 公開インタフェース (AUTOSAR SWS_WdgM 準拠)
 * \details Supervised Entity の Alive / Logical / Deadline Supervision インタフェース。
 *
 *          使い方:
 *            1. EcuM_Init 内で WdgM_Init(&WdgM_Config) を呼ぶ。
 *            2. 監視対象の Runnable 内のプログラムフロー上の各地点で
 *               WdgM_CheckpointReached(WDGM_ENTITY_*, WDGM_CP_*) を呼ぶ。
 *            3. Os スケジューラが WdgM_MainFunction() を定期実行する。
 *               CheckpointReached がエンティティごとの期待回数
 *               (WdgM_Cfg.h の WDGM_*_EXPECTED_ALIVE_INDICATIONS) 以上来ていれば OK、
 *               満たさなければ FAILED とみなしログで通知する (Alive Supervision)。
 *            4. WdgM_CheckpointReached() は呼ばれた瞬間にも、直前のチェックポイントから
 *               今回のチェックポイントへの遷移が許可されているかを即座に確認する
 *               (Logical Supervision)。許可されない順序が来た場合は即座に FAILED とする。
 *            5. 同じ呼び出しの中で、直前のチェックポイントからの実際の経過時間が
 *               許容範囲 [MinMs, MaxMs] 内かも即座に確認する (Deadline Supervision)。
 *               範囲外（遅すぎる・速すぎる）の場合も即座に FAILED とする。
 *
 *          本プロジェクトでの失敗アクション:
 *            Alive・Logical・Deadline Supervision の判定結果はそれぞれ独立した
 *            内部ステータスに保持し（AUTOSAR が個々のアルゴリズムごとに判定結果を
 *            保持するのと同じ考え方）、WdgM_GetLocalStatus() はいずれか一つでも
 *            FAILED ならローカルステータスとして FAILED を返す。
 *            実 HW ウォッチドッグ（WdgM_Hw 層。AVR は <avr/wdt.h>、Renesas RA は
 *            WDT ライブラリ）と連携している。判定は WdgM_MainFunction が
 *            WDGM_SUPERVISION_CYCLE_MS ごとに行うが、HW ウォッチドッグへの実際の
 *            リフレッシュは WdgM_TriggerHwWatchdog が WDGM_HW_TRIGGER_CYCLE_MS
 *            ごとに、直近の判定結果を見て行う（周期を分離している理由は
 *            WdgM_Cfg.h の WDGM_HW_WATCHDOG_TIMEOUT_MS コメントを参照）。
 *            いずれかが FAILED の間はリフレッシュが止まり、
 *            WDGM_HW_WATCHDOG_TIMEOUT_MS 後に実際に MCU がリセットされる
 *            （シミュレーションではなく実機で本当に発生する）。
 *
 *          Logical / Deadline Supervision の FAILED は WdgM_Init() までラッチされる:
 *            WdgM_CheckpointReached() が不正な遷移、または許容範囲外の経過時間を
 *            検出すると、対応する内部ステータスを FAILED にする。Alive Supervision
 *            とは異なり WdgM_MainFunction() の周期処理では自動的に OK へ復帰させない
 *            （違反が起きたという事実は、その後 Alive 条件を満たしても消えないため）。
 *            HW ウォッチドッグのリフレッシュも止まるので、WDGM_HW_WATCHDOG_TIMEOUT_MS
 *            後の MCU リセット → WdgM_Init() の再実行でのみ OK に戻る。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef WDGM_H
#define WDGM_H

#include "Std_Types.h"
#include "WdgM_Cfg.h"
#include "WdgM_PBCfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * 型定義
 * ----------------------------------------------------------------------- */

/** Supervised Entity ID 型 */
typedef uint8 WdgM_SupervisedEntityIdType;

/**
 * \brief   Supervised Entity のローカルステータス
 * \details AUTOSAR WdgM_LocalStatusType に相当する。
 */
typedef enum
{
    WDGM_LOCAL_STATUS_OK          = 0x00U,  /**< 正常: Checkpoint が期待回数以上届いた */
    WDGM_LOCAL_STATUS_FAILED      = 0x01U,  /**< 失敗: Checkpoint 不足 */
    WDGM_LOCAL_STATUS_DEACTIVATED = 0x0FU   /**< 無効: 初期化前または ID 不正 */
} WdgM_LocalStatusType;

/* -----------------------------------------------------------------------
 * 公開 API
 * ----------------------------------------------------------------------- */

/**
 * \brief   WdgM モジュールを初期化する。
 *
 * \details 全エンティティの Alive カウンタとステータスを初期化し、
 *          WdgM_EnableHwWatchdog() で AVR 実ハードウェアウォッチドッグを
 *          有効化する。EcuM_Init() の末尾、他の全 BSW モジュール初期化が
 *          完了した後（Os_Init より前）に呼び出すこと。
 *
 * \param[in]  ConfigPtr  ポストビルドコンフィグへのポインタ。NULL 禁止。
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void WdgM_Init(const WdgM_ConfigType* ConfigPtr);

/**
 * \brief   AVR 実ハードウェアウォッチドッグを WDGM_HW_WATCHDOG_TIMEOUT_MS で有効化する。
 *
 * \details WdgM_Init() がこの関数を呼ぶ。また、EcuM が POST_RUN から RUN へ
 *          復帰する際にも、監視対象タスクが再開するのに合わせて再度呼び出す
 *          （その際は WdgM_ResumeSupervision() も併せて呼ぶこと）。
 *
 * \ServiceID      {0x07}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void WdgM_EnableHwWatchdog(void);

/**
 * \brief   AVR 実ハードウェアウォッチドッグを無効化する。
 *
 * \details EcuM が POST_RUN へ遷移する際に呼び出す。POST_RUN では
 *          Rte_Engine タスク（WdgM の監視対象）が意図的に停止するため、
 *          Alive Supervision は必ず FAILED になる。無効化しないと、
 *          意図した停止にもかかわらず HW ウォッチドッグのタイムアウト後に
 *          MCU がリセットされてしまう。
 *
 * \ServiceID      {0x06}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void WdgM_DisableHwWatchdog(void);

/**
 * \brief   全エンティティのチェックポイント追跡基準をリセットする。
 *
 * \details EcuM が POST_RUN から RUN へ復帰し、監視対象タスクの実行を
 *          再開する直前に呼び出すこと（WdgM_EnableHwWatchdog() と対になる）。
 *          POST_RUN 中は監視対象タスクが意図的に停止しているため、
 *          WdgM_CheckpointReached() が呼ばれず、内部の「直前チェックポイント」
 *          基準（チェックポイント ID・発生時刻）は停止前の古い値のまま残る。
 *          これをリセットせずに再開すると、再開後最初のチェックポイントで
 *          Deadline Supervision が「POST_RUN 中の停止時間」を実際の処理時間と
 *          誤認し、誤って FAILED と判定してしまう。
 *          チェックポイント基準を WDGM_CP_INITIAL に戻すことで、再開後最初の
 *          チェックポイントは起動直後と同じ「基準なしの遷移」として扱われ、
 *          Deadline 比較の対象から外れる（Logical Supervision も同様に
 *          WDGM_CP_INITIAL からの遷移として許可される）。
 *          既にラッチされている Logical/Deadline の FAILED 状態自体は
 *          リセットしない（停止前に本当に違反していた事実は消さない）。
 *
 * \ServiceID      {0x08}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void WdgM_ResumeSupervision(void);

/**
 * \brief   Supervised Entity がチェックポイントに到達したことを報告する。
 *
 * \details 監視対象の Runnable 内のプログラムフロー上の各地点から呼び出す。
 *          WdgM 内部の Alive カウンタをインクリメントし (Alive Supervision)、
 *          直前に報告されたチェックポイントから今回のチェックポイントへの遷移が
 *          許可テーブルに含まれるかを即座に検査する (Logical Supervision)。
 *          許可されない遷移の場合はローカルステータスを即座に FAILED にする。
 *          さらに、直前のチェックポイントからの実際の経過時間が許容範囲
 *          [MinMs, MaxMs] 内かも即座に検査する (Deadline Supervision)。
 *          範囲外の場合もローカルステータスを即座に FAILED にする。
 *
 * \param[in]  SEID          エンティティ ID (WdgM_Cfg.h の WDGM_ENTITY_*)。
 * \param[in]  CheckpointId  チェックポイント ID (WdgM_Cfg.h の WDGM_CP_*)。
 * \return     E_OK: 正常受付。E_NOT_OK: ID 不正。
 *
 * \ServiceID      {0x0E}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType WdgM_CheckpointReached(WdgM_SupervisedEntityIdType SEID, uint8 CheckpointId);

/**
 * \brief   Supervised Entity の現在のローカルステータスを取得する。
 *
 * \details Alive・Logical・Deadline Supervision のいずれか一つでも FAILED
 *          なら FAILED を返す（AUTOSAR の「全アルゴリズムの結果の最悪値」と
 *          同じ考え方）。
 *
 * \param[in]  SEID  エンティティ ID。
 * \return     WdgM_LocalStatusType 値。ID 不正の場合は DEACTIVATED。
 *
 * \ServiceID      {0x0B}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
WdgM_LocalStatusType WdgM_GetLocalStatus(WdgM_SupervisedEntityIdType SEID);

/**
 * \brief   WdgM 周期処理。Alive Supervision を評価する。
 *
 * \details Os スケジューラから WDGM_SUPERVISION_CYCLE_MS ごとに呼ばれる。
 *          各エンティティの Alive カウンタを検査し、期待回数を満たさない場合は
 *          ローカルステータスを FAILED に更新して WARN ログを出力する。
 *          検査後、カウンタは次のサイクルのためにリセットする。
 *          HW ウォッチドッグへの実際のリフレッシュはここでは行わない
 *          （WdgM_TriggerHwWatchdog が別周期で判定結果を見て行う）。
 *
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void WdgM_MainFunction(void);

/**
 * \brief   HW ウォッチドッグの trigger（リフレッシュ）処理。
 *
 * \details Os スケジューラから WDGM_HW_TRIGGER_CYCLE_MS ごとに呼ばれる。
 *          全エンティティの WdgM_GetLocalStatus() が OK（または
 *          WdgM_DisableHwWatchdog() による抑制中）の場合のみ
 *          WdgM_Hw_Refresh() を呼ぶ。1 つでも FAILED があれば呼ばないため、
 *          WDGM_HW_WATCHDOG_TIMEOUT_MS 後に実際に MCU がリセットされる。
 *
 *          WdgM_MainFunction（判定, 6000ms）と周期を分離しているのは、
 *          Renesas RA4M1 の IWDT 最大タイムアウト（約 5592ms）が判定サイクルより
 *          短く、判定サイクルに直接リフレッシュを同期できないため。
 *          詳細は WdgM_Cfg.h の WDGM_HW_WATCHDOG_TIMEOUT_MS コメントを参照。
 *
 * \ServiceID      {0x09}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void WdgM_TriggerHwWatchdog(void);

#ifdef __cplusplus
}
#endif

#endif /* WDGM_H */
