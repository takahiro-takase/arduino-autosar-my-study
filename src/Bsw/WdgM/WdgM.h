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
 *            実 HW ウォッチドッグ（WdgIf → Wdg → Wdg_Hw 層経由。Wdg_Hw の実体は
 *            Renesas RA の WDT ライブラリ）と連携している。判定は WdgM_MainFunction が
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

/**
 * \brief   Supervised Entity ID 型 ([SWS_WdgM_00356])。
 * \details 実仕様は uint16 由来と規定する。本プロジェクトのエンティティ数は
 *          少数（WdgM_Cfg.h の WDGM_ENTITY_* 参照）で uint8 でも実害はないが、
 *          型としては仕様に合わせる（2026-09-05 是正、以前は uint8 だった）。
 *
 * \AUTOSARReq     {SWS_WdgM_00356}
 */
typedef uint16 WdgM_SupervisedEntityIdType;

/**
 * \brief   チェックポイント ID 型 ([SWS_WdgM_00357])。
 * \details 実仕様は uint16 由来と規定する。本プロジェクトのチェックポイント数は
 *          少数（WdgM_Cfg.h の WDGM_CP_* 参照）で uint8 でも実害はないが、
 *          型としては仕様に合わせる（2026-09-05 是正、以前は素の uint8 引数
 *          だった）。
 *
 * \AUTOSARReq     {SWS_WdgM_00357}
 */
typedef uint16 WdgM_CheckpointIdType;

/**
 * \brief   WdgM に設定されたモードを区別する型。
 * \details AUTOSAR WdgM_ModeType に相当する ([SWS_WdgM_00358])。範囲は
 *          0〜<設定モード数>-1 だが、本プロジェクトは単一の静的コンフィグ
 *          のみ保持するため実質的に WDGM_MODE_DEFAULT (0) のみが有効な値。
 *
 * \AUTOSARReq     {SWS_WdgM_00358}
 */
typedef uint8 WdgM_ModeType;

/**
 * \brief   Supervised Entity のローカルステータス
 * \details AUTOSAR WdgM_LocalStatusType に相当する ([SWS_WdgM_00359])。
 *
 * \AUTOSARReq     {SWS_WdgM_00359}
 */
typedef enum
{
    WDGM_LOCAL_STATUS_OK          = 0x00U,  /**< 正常: Checkpoint が期待回数以上届いた */
    WDGM_LOCAL_STATUS_FAILED      = 0x01U,  /**< 失敗: Checkpoint 不足 */
    WDGM_LOCAL_STATUS_DEACTIVATED = 0x04U   /**< 無効: 初期化前または ID 不正 (SWS_WdgM_00359) */
} WdgM_LocalStatusType;

/**
 * \brief   WdgM 全体のグローバル supervision ステータス
 * \details AUTOSAR WdgM_GlobalStatusType ([SWS_WdgM_00360]) に相当する。値も仕様書と
 *          一致させている (OK=0/FAILED=1/EXPIRED=2/STOPPED=3/DEACTIVATED=4)。
 *          WdgM_GetGlobalStatus() が返す。
 *
 * \AUTOSARReq     {SWS_WdgM_00360}
 */
typedef enum
{
    WDGM_GLOBAL_STATUS_OK          = 0x00U,  /**< 正常: 全エンティティ OK */
    WDGM_GLOBAL_STATUS_FAILED      = 0x01U,  /**< 失敗: いずれかのエンティティが FAILED だが、
                                               *   まだグローバル猶予サイクルを消費していない */
    WDGM_GLOBAL_STATUS_EXPIRED     = 0x02U,  /**< 猶予消費中: FAILED が継続し
                                               *   WdgM_ExpiredCycleCount が進んでいるが、
                                               *   まだ WDGM_EXPIRED_SUPERVISION_CYCLE_TOL に
                                               *   達していない (HW ウォッチドッグは refresh 継続) */
    WDGM_GLOBAL_STATUS_STOPPED     = 0x03U,  /**< 猶予を使い切り、HW ウォッチドッグの refresh を
                                               *   拒否している状態。リセットが差し迫っている */
    WDGM_GLOBAL_STATUS_DEACTIVATED = 0x04U   /**< 無効: 未初期化 */
} WdgM_GlobalStatusType;

/* -----------------------------------------------------------------------
 * 公開 API
 * ----------------------------------------------------------------------- */

/**
 * \brief   WdgM モジュールを初期化する。
 *
 * \details 全エンティティの Alive カウンタとステータスを初期化し、
 *          WdgM_EnableHwWatchdog() で実ハードウェアウォッチドッグを
 *          有効化する。EcuM_Init() の末尾、他の全 BSW モジュール初期化が
 *          完了した後（Os_Init より前）に呼び出すこと。
 *
 * \param[in]  ConfigPtr  ポストビルドコンフィグへのポインタ。NULL 禁止。
 *
 * \AUTOSARReq     {SWS_WdgM_00151}
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void WdgM_Init(const WdgM_ConfigType* ConfigPtr);

/**
 * \brief   WdgM モジュールを未初期化状態に戻す。
 *
 * \details 設定ポインタを NULL に戻す。未初期化状態で呼ばれた場合は
 *          WDGM_E_NO_INIT を報告し何もしない。実 HW ウォッチドッグの有効/
 *          無効化は行わない（WdgM_EnableHwWatchdog/DisableHwWatchdog は
 *          本プロジェクト独自の別 API であり、本関数の責務ではない）。
 *
 * \AUTOSARReq     {SWS_WdgM_00261, SWS_WdgM_00288}
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void WdgM_DeInit(void);

/**
 * \brief   WdgM の現在のモードを設定する。
 *
 * \details 実仕様は複数の監視設定セット（モード）間の切替 API だが、
 *          本プロジェクトは単一の静的コンフィグのみ保持するため、
 *          `WDGM_MODE_DEFAULT` (0) 以外はエラーとして拒否するだけの
 *          簡略実装とする（監視対象・許容値セット自体の実際の入れ替えは
 *          行わない）。
 *
 * \param[in]  Mode  設定するモード。`WDGM_MODE_DEFAULT` のみ有効。
 * \retval  E_OK      モードを正常に設定した。
 * \retval  E_NOT_OK  WdgM 未初期化、または Mode が範囲外
 *                    (`WDGM_MODE_DEFAULT` 以外)。
 *
 * \AUTOSARReq     {SWS_WdgM_00154, SWS_WdgM_00020, SWS_WdgM_00021}
 * \ServiceID      {0x03}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType WdgM_SetMode(WdgM_ModeType Mode);

/**
 * \brief   WdgM の現在のモードを取得する。
 *
 * \param[out]  Mode  現在のモードの格納先。NULL 禁止。
 * \retval  E_OK      モードを正常に返した。
 * \retval  E_NOT_OK  NULL ポインタ、または WdgM 未初期化
 *                    (この場合 *Mode は `WDGM_MODE_DEFAULT`)。
 *
 * \AUTOSARReq     {SWS_WdgM_00168, SWS_WdgM_00170, SWS_WdgM_00253}
 * \ServiceID      {0x0b}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType WdgM_GetMode(WdgM_ModeType* Mode);

/**
 * \brief   実ハードウェアウォッチドッグを WDGM_HW_WATCHDOG_TIMEOUT_MS で有効化する。
 *
 * \details WdgM_Init() がこの関数を呼ぶ。また、EcuM が POST_RUN から RUN へ
 *          復帰する際にも、監視対象タスクが再開するのに合わせて再度呼び出す
 *          （その際は WdgM_ResumeSupervision() も併せて呼ぶこと）。
 *
 * \note       AUTOSAR 標準の SWS_WdgM には存在しない本プロジェクト独自の
 *             拡張関数のため、対応する \AUTOSARReq は無い
 *             (WdgM_Cfg.h 冒頭のコメント参照)。
 * \ServiceID      {0x07}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void WdgM_EnableHwWatchdog(void);

/**
 * \brief   実ハードウェアウォッチドッグを無効化する。
 *
 * \details EcuM が POST_RUN へ遷移する際に呼び出す。POST_RUN では
 *          Rte_Engine タスク（WdgM の監視対象）が意図的に停止するため、
 *          Alive Supervision は必ず FAILED になる。無効化しないと、
 *          意図した停止にもかかわらず HW ウォッチドッグのタイムアウト後に
 *          MCU がリセットされてしまう。
 *
 * \note       AUTOSAR 標準の SWS_WdgM には存在しない本プロジェクト独自の
 *             拡張関数のため、対応する \AUTOSARReq は無い
 *             (WdgM_Cfg.h 冒頭のコメント参照)。
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
 * \note       AUTOSAR 標準の SWS_WdgM には存在しない本プロジェクト独自の
 *             拡張関数のため、対応する \AUTOSARReq は無い
 *             (WdgM_Cfg.h 冒頭のコメント参照)。ApiId は自己割当だが、
 *             従来の 0x08 は実仕様の WdgM_MainFunction([SWS_WdgM_00159])の
 *             正規 ServiceID と衝突していたため、未使用の 0x0a へ変更した。
 * \ServiceID      {0x0a}
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
 * \AUTOSARReq     {SWS_WdgM_00263, SWS_WdgM_00278, SWS_WdgM_00279,
 *                  SWS_WdgM_00356, SWS_WdgM_00357}
 * \ServiceID      {0x0E}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType WdgM_CheckpointReached(WdgM_SupervisedEntityIdType SEID, WdgM_CheckpointIdType CheckpointId);

/**
 * \brief   Supervised Entity の現在のローカルステータスを取得する。
 *
 * \details Alive・Logical・Deadline Supervision のいずれか一つでも FAILED
 *          なら FAILED を返す（AUTOSAR の「全アルゴリズムの結果の最悪値」と
 *          同じ考え方）。
 *
 * \param[in]   SEID    エンティティ ID。
 * \param[out]  Status  ローカルステータスの格納先。NULL 禁止。
 * \return      E_OK: 正常取得。E_NOT_OK: NULL ポインタ・未初期化・SEID 不正
 *              (この場合 *Status は WDGM_LOCAL_STATUS_DEACTIVATED)。
 *
 * \warning    戻り値 (Std_ReturnType: E_OK=0/E_NOT_OK=1) と *Status
 *             (WdgM_LocalStatusType: OK=0/FAILED=1) は数値がたまたま重なる。
 *             `WdgM_GetLocalStatus(seid, &status) == WDGM_LOCAL_STATUS_FAILED`
 *             のように戻り値と *Status の型を混同して比較しないこと
 *             （それは常に false になり、本来検出したい FAILED を
 *             見逃す）。必ず戻り値で成否を確認した上で *Status を見ること。
 * \AUTOSARReq     {SWS_WdgM_00169, SWS_WdgM_00171, SWS_WdgM_00172,
 *                  SWS_WdgM_00173, SWS_WdgM_00257}
 * \ServiceID      {0x0C}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType WdgM_GetLocalStatus(WdgM_SupervisedEntityIdType SEID, WdgM_LocalStatusType* Status);

/**
 * \brief   WdgM 全体のグローバル supervision ステータスを取得する。
 *
 * \details 全エンティティの WdgM_GetLocalStatus() とグローバル猶予サイクル
 *          (WdgM_ExpiredCycleCount)・停止フラグ (WdgM_GlobalStopped) から、
 *          AUTOSAR の 4 状態 (OK/FAILED/EXPIRED/STOPPED) を導出して返す:
 *            - 全エンティティ OK                              → OK
 *            - いずれか FAILED、猶予サイクル未消費 (count==0)  → FAILED
 *              (WdgM_CheckpointReached() が Logical/Deadline 違反を検出した
 *              直後、次の WdgM_MainFunction() 判定サイクルが来るまでの間の
 *              一時的な状態としても観測されうる)
 *            - いずれか FAILED、猶予サイクル消費中 (count>0)   → EXPIRED
 *            - グローバル猶予を使い切り refresh 拒否中          → STOPPED
 *
 * \param[out]  Status  グローバルステータスの格納先。NULL 禁止。
 * \return      E_OK: 正常取得。E_NOT_OK: NULL ポインタまたは未初期化
 *              (この場合 *Status は WDGM_GLOBAL_STATUS_DEACTIVATED)。
 *
 * \AUTOSARReq     {SWS_WdgM_00175, SWS_WdgM_00176, SWS_WdgM_00344}
 * \ServiceID      {0x0D}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType WdgM_GetGlobalStatus(WdgM_GlobalStatusType* Status);

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
 * \note       グローバル猶予サイクル判定 (OK→FAILED→EXPIRED→STOPPED、
 *             [SWS_WdgM_00119]〜[SWS_WdgM_00122]) 自体もここで行う。実
 *             AUTOSAR の WdgM_MainFunction はこれに加えて HW ウォッチドッグへの
 *             実際のリフレッシュ指示までを 1 関数で担うが、本プロジェクトは
 *             Renesas RA4M1 の IWDT タイムアウト制約 (詳細は WdgM_Cfg.h の
 *             WDGM_HW_WATCHDOG_TIMEOUT_MS 参照) によりリフレッシュ部分を
 *             WdgM_TriggerHwWatchdog() へ分離している。
 * \AUTOSARReq     {SWS_WdgM_00159, SWS_WdgM_00119, SWS_WdgM_00120,
 *                  SWS_WdgM_00121, SWS_WdgM_00122}
 * \ServiceID      {0x08}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void WdgM_MainFunction(void);

/**
 * \brief   HW ウォッチドッグの trigger（リフレッシュ）処理。
 *
 * \details Os スケジューラから WDGM_HW_TRIGGER_CYCLE_MS ごとに呼ばれる。
 *          WdgM_GlobalStopped が立っていない（または WdgM_DisableHwWatchdog()
 *          による抑制中）場合のみ WdgIf_SetTriggerCondition() を呼ぶ。
 *          WdgM_GlobalStopped は WdgM_MainFunction() がグローバル猶予サイクル
 *          (WDGM_EXPIRED_SUPERVISION_CYCLE_TOL) を使い切って初めて立てる
 *          （1 つのエンティティが FAILED になった瞬間に即座に呼ばなくなる
 *          わけではない）ため、それが続いた場合に最終的に
 *          WDGM_HW_WATCHDOG_TIMEOUT_MS 後に実際に MCU がリセットされる。
 *
 *          WdgM_MainFunction（判定, 6000ms）と周期を分離しているのは、
 *          Renesas RA4M1 の IWDT 最大タイムアウト（約 5592ms）が判定サイクルより
 *          短く、判定サイクルに直接リフレッシュを同期できないため。
 *          詳細は WdgM_Cfg.h の WDGM_HW_WATCHDOG_TIMEOUT_MS コメントを参照。
 *
 * \note       実 AUTOSAR の WdgM_MainFunction が内包するリフレッシュ指示
 *             部分を本プロジェクトが分離した独自 API のため、専用の
 *             Service ID は SWS_WdgM に存在しない（\ServiceID の値は
 *             プロジェクト内でのみ一意な自己割当）。継続/停止の判断根拠
 *             ([SWS_WdgM_00119]〜[SWS_WdgM_00122]) は WdgM_MainFunction() と
 *             共通。
 * \AUTOSARReq     {SWS_WdgM_00119, SWS_WdgM_00120, SWS_WdgM_00121, SWS_WdgM_00122}
 * \ServiceID      {0x09}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void WdgM_TriggerHwWatchdog(void);

/**
 * \brief   HW ウォッチドッグの trigger を永続的に止め、リセットさせる。
 *
 * \details 呼び出し時点で WdgM_ResetRequested フラグを立てる。実際に全
 *          Watchdog Driver の trigger condition を 0 にする（[SWS_WdgM_00232]）
 *          のは、このフラグを見る WdgM_TriggerHwWatchdog() の次回呼び出し
 *          （最大 WDGM_HW_TRIGGER_CYCLE_MS 後）であり、本関数自体は
 *          WdgIf/Wdg を同期的には一切呼ばない（WdgM_MainFunction/
 *          WdgM_TriggerHwWatchdog の周期分離という本プロジェクトの既存設計に
 *          合わせている）。以降 WdgM_TriggerHwWatchdog() は Global
 *          Supervision Status に関わらずリフレッシュを二度と行わなくなる
 *          （次回 WdgM_Init() が呼ばれるまで）。本プロジェクトの HW
 *          ウォッチドッグ（Renesas RA の IWDT）は一度有効化するとソフトウェア
 *          から無効化できないため、リフレッシュ停止から
 *          WDGM_HW_WATCHDOG_TIMEOUT_MS 以内に確実に実 MCU リセットへ至る。
 *
 * \note    WdgM が未初期化（WDGM_GLOBAL_STATUS_DEACTIVATED）の場合は
 *          WDGM_E_NO_INIT を報告し、何もせず戻る（[SWS_WdgM_00270]）。
 *
 * \AUTOSARReq     {SWS_WdgM_00264, SWS_WdgM_00232, SWS_WdgM_00233, SWS_WdgM_00270}
 * \ServiceID      {0x0f}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void WdgM_PerformReset(void);

/**
 * \brief   直近の HW ウォッチドッグリセットの原因となった Supervised Entity の
 *          ID を取得する。
 *
 * \details [SWS_WdgM_00349] 準拠: SEID を実 HW リセットをまたいで保持する
 *          必要があるため、C ランタイムが起動時にゼロクリアする通常の
 *          静的変数ではなく、リンカが未初期化のまま残す領域
 *          （`.noinit` セクション）に SEID とそのビット反転値を対で格納する。
 *          両者が一致すれば直前のリセット原因として有効な値、一致しなければ
 *          （初回起動・電源断からの起動等で RAM 内容が不定なため）無効と
 *          判断し E_NOT_OK を返す。
 *
 *          値の書き込みは WdgM_MainFunction() が
 *          WDGM_GLOBAL_STATUS_STOPPED（グローバル猶予サイクルを使い切り、
 *          HW リセットが確実に迫っている状態）へ遷移した瞬間に、その時点で
 *          FAILED な最初の Supervised Entity（走査順で最初に見つかったもの）
 *          を記録する形で行う。
 *
 * \note    [SWS_WdgM_00348] 準拠: WdgM_Init() より前（未初期化状態）でも
 *          呼び出せる（起動直後、WdgM_Init() を呼ぶ前に直前のリセット原因を
 *          診断する用途を想定した仕様のため）。他の全 API と異なり
 *          WDGM_E_NO_INIT チェックを行わない。
 *
 * \param[out]  SEID  直前のリセット原因となった SEID の格納先。NULL 禁止。
 *                    E_NOT_OK 時は 0 を書き込む（[SWS_WdgM_00349]）。
 * \retval  E_OK      SEID を正常に返した。
 * \retval  E_NOT_OK  SEID が NULL、または保存値が無効（初回起動・POR 等）。
 *
 * \AUTOSARReq     {SWS_WdgM_00346, SWS_WdgM_00347, SWS_WdgM_00348, SWS_WdgM_00349}
 * \ServiceID      {0x10}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType WdgM_GetFirstExpiredSEID(WdgM_SupervisedEntityIdType* SEID);

/**
 * \brief   WdgM モジュールのバージョン情報を取得する。
 *
 * \details WdgM_Init と並び、未初期化時でも WDGM_E_NO_INIT を報告しない
 *          例外 API（他 BSW モジュールと共通の慣例）のため、初期化状態は
 *          確認せず NULL ポインタチェックのみ行う。
 *
 * \param[out]  VersionInfo  バージョン情報の格納先。NULL 禁止。
 *
 * \AUTOSARReq     {SWS_WdgM_00153, SWS_WdgM_00256}
 * \ServiceID      {0x02}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void WdgM_GetVersionInfo(Std_VersionInfoType* VersionInfo);

#ifdef WDGM_UNIT_TEST
/**
 * \brief   [テスト専用] WdgM_GetFirstExpiredSEID() が読む .noinit 領域
 *          （WdgM_FirstExpiredSEID/Inv）に任意の値を直接書き込む。
 *
 * \details `test/test_wdgm/` の単体テストからのみ使用するアクセサ。
 *          `WDGM_UNIT_TEST` は `[env:native_wdgm]` の `build_flags` でのみ
 *          定義され、実機ビルド（`uno_r4`）では定義されないため、実機の
 *          `WdgM.h`/`WdgM.c` には一切含まれない（`Com.h` の
 *          `COM_UNIT_TEST`/`Com_Test_*` と同じ運用）。
 *          初回起動・電源断相当の「不定値」を模擬する場合は
 *          `value`/`inv` にビット反転の関係にない値を渡すこと。
 */
void WdgM_Test_SetFirstExpiredSEIDRaw(WdgM_SupervisedEntityIdType value, WdgM_SupervisedEntityIdType inv);
#endif

#ifdef __cplusplus
}
#endif

#endif /* WDGM_H */
