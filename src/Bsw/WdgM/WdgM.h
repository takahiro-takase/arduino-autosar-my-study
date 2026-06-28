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
 *               CheckpointReached が WDGM_EXPECTED_ALIVE_INDICATIONS 回以上来ていれば OK、
 *               0 回のまま周期が来たら FAILED とみなしログで通知する (Alive Supervision)。
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
 *            AVR 実ハードウェアウォッチドッグ（<avr/wdt.h>）と連携しており、
 *            正常な間だけ WdgM_MainFunction が wdt_reset() を呼ぶ。異常時は
 *            リフレッシュを止め、WDGM_HW_WATCHDOG_TIMEOUT_MS 後に実際に MCU が
 *            リセットされる（シミュレーションではなく実機で本当に発生する）。
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
 *          復帰する際にも、Alive Supervision の対象タスクが再開するのに合わせて
 *          再度呼び出す。
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
 * \brief   WdgM 周期処理。Alive Supervision を評価し、HW ウォッチドッグを refresh する。
 *
 * \details Os スケジューラから WDGM_SUPERVISION_CYCLE_MS ごとに呼ばれる。
 *          各エンティティの Alive カウンタを検査し、期待回数を満たさない場合は
 *          ローカルステータスを FAILED に更新して WARN ログを出力する。
 *          検査後、カウンタは次のサイクルのためにリセットする。
 *          全エンティティが OK の場合のみ wdt_reset() を呼ぶ。1 つでも FAILED
 *          （Alive 不足または Logical Supervision 違反）があれば呼ばない
 *          ため、WDGM_HW_WATCHDOG_TIMEOUT_MS 後に実際に MCU がリセットされる。
 *
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void WdgM_MainFunction(void);

#ifdef __cplusplus
}
#endif

#endif /* WDGM_H */
