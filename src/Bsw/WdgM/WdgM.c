/**
 * \file    WdgM.c
 * \brief   ウォッチドッグマネージャ 実装 (AUTOSAR SWS_WdgM 準拠)
 * \details Supervised Entity の Alive Supervision / Logical Supervision を管理する。
 *
 *          Alive Supervision アルゴリズム:
 *            1. 監視対象 Runnable が WdgM_CheckpointReached() を呼ぶたびに
 *               WdgM_AliveCount[SEID] をインクリメントする。
 *            2. WdgM_MainFunction() (WDGM_SUPERVISION_CYCLE_MS 周期) が
 *               WdgM_AliveCount >= entity->ExpectedAliveIndications
 *               (WdgM_Cfg.h の WDGM_*_EXPECTED_ALIVE_INDICATIONS、エンティティごとに
 *               個別設定) を確認する。
 *               - 満たす → LOCAL_STATUS_OK: 正常継続
 *               - 満たさない → LOCAL_STATUS_FAILED: WARN ログ出力
 *            3. 検査後カウンタを 0 にリセットし次サイクルを開始する。
 *
 *          Logical Supervision アルゴリズム:
 *            1. WdgM_CheckpointReached(SEID, CheckpointId) が呼ばれるたびに、
 *               WdgM_LastCheckpoint[SEID]（直前のチェックポイント）から
 *               今回の CheckpointId への遷移が許可遷移テーブル
 *               (WdgM_PBCfg.c の Transitions[]) に含まれるかを即座に確認する。
 *            2. 含まれない場合は WdgM_LogicalStatus[SEID] を FAILED にして
 *               WARN ログを出力する (MainFunction の周期を待たず即時検出)。
 *            3. WdgM_LastCheckpoint[SEID] を今回の CheckpointId に更新する。
 *
 *          Deadline Supervision アルゴリズム (Alive/Logical に続く 3 つ目):
 *            1. WdgM_CheckpointReached(SEID, CheckpointId) が呼ばれるたびに、
 *               WdgM_LastCheckpointTimeMs[SEID]（直前のチェックポイントの発生時刻）
 *               からの実際の経過時間を計算する。
 *            2. 直前のチェックポイントから今回のチェックポイントへの区間が
 *               許容テーブル (WdgM_PBCfg.c の Deadlines[]) に設定されていれば、
 *               経過時間が [MinMs, MaxMs] の範囲内かを確認する。範囲外なら
 *               WdgM_DeadlineStatus[SEID] を FAILED にして WARN ログを出力する。
 *            3. WdgM_LastCheckpointTimeMs[SEID] を現在時刻に更新する
 *               (WDGM_CP_INITIAL からの最初の遷移には基準時刻がないため対象外)。
 *
 *          3 アルゴリズムの判定結果統合 (AUTOSAR 同等の挙動):
 *            WdgM_AliveStatus[] (Alive)・WdgM_LogicalStatus[] (Logical)・
 *            WdgM_DeadlineStatus[] (Deadline) を別々の配列で保持する。
 *            WdgM_GetLocalStatus() と WdgM_MainFunction() の HW ウォッチドッグ
 *            refresh 判定は、3 つ全てが OK の場合のみ OK とみなす。
 *            WdgM_AliveStatus は周期ごとに OK/FAILED を再評価するが、
 *            WdgM_LogicalStatus と WdgM_DeadlineStatus は WdgM_Init() でのみ
 *            OK に戻る（違反が起きたという事実は Alive 条件を満たしても消えない）。
 *            旧実装ではこれらを 1 つの WdgM_LocalStatus に統合していたため、
 *            Logical/Deadline Supervision が FAILED と判定した直後でも次の
 *            MainFunction サイクルで Alive 条件を満たせば OK に上書きされ、
 *            検出した違反が消えてしまう問題があった。
 *
 *          POST_RUN→RUN 復帰時の誤検出対策 (Deadline / Alive Supervision):
 *            POST_RUN 中は監視対象タスクが意図的に停止し WdgM_CheckpointReached()
 *            が呼ばれないため、WdgM_LastCheckpoint / WdgM_LastCheckpointTimeMs は
 *            停止前の古い値のまま残る。これをリセットせずに RUN へ復帰すると、
 *            再開後最初のチェックポイントで「POST_RUN 中の停止時間」を実際の
 *            処理時間と誤認し、Deadline Supervision が誤って FAILED と判定して
 *            しまう（Deadline Supervision 導入直後に発覚した不具合）。
 *            同様に、POST_RUN 中の Alive カウンタ不足で WdgM_MainFunction() が
 *            SE を FAILED とラッチすることがあり（POST_RUN 中は
 *            WdgM_SupervisionSuppressed で実害はないが）、これをリセットせずに
 *            RUN へ復帰すると、実際にはアプリが正常に再開しているのに古い
 *            FAILED ラッチのせいで HW ウォッチドッグリフレッシュが拒否され
 *            続け、次の判定サイクル（最大6000ms後）が来る前に実際に MCU が
 *            リセットされてしまう（実機で確認された不具合）。
 *            EcuM が RUN へ復帰する際に WdgM_ResumeSupervision() を呼び、
 *            チェックポイント基準を WDGM_CP_INITIAL に、Alive カウンタ/
 *            ステータスを 0/OK にリセットすることで、再開後最初の遷移を
 *            起動直後と同じ「基準なし」として扱い、この誤検出を防ぐ。
 *
 *            さらに、WdgM_MainFunction() 自身（Task 7）は POST_RUN 中も
 *            継続動作するタスクのため、その呼び出しタイミングは
 *            WdgM_ResumeSupervision() の AliveCount リセットと同期していない。
 *            POST_RUN が WDGM_SUPERVISION_CYCLE_MS (6000ms) より大幅に短い
 *            場合、リセット直後にたまたま判定タイミングが来てしまい、
 *            エンティティが一度もチェックインできていないうちに FAILED と
 *            誤判定することが実機で確認された（短時間のボランタリスリープ→
 *            即座のウェイクアップ、というシナリオで顕在化）。
 *            WdgM_ResumeSupervision() は WdgM_SkipNextAliveJudgment も立て、
 *            次回の WdgM_MainFunction() 呼び出し 1 回分だけ判定をスキップする
 *            ことでこれを防ぐ（詳細は WdgM_MainFunction() 冒頭のコメント参照）。
 *
 *          HW ウォッチドッグ連携:
 *            WdgM_Init() で実ハードウェアウォッチドッグ (WdgM_Hw 層が
 *            MCU 固有 API をラップする) を WDGM_HW_WATCHDOG_TIMEOUT_MS
 *            (4000ms) で有効化する。
 *            判定 (Alive/Logical/Deadline Supervision) は WdgM_MainFunction が
 *            WDGM_SUPERVISION_CYCLE_MS (6000ms) ごとに行うが、HW ウォッチドッグへの
 *            実際のリフレッシュは WdgM_TriggerHwWatchdog が
 *            WDGM_HW_TRIGGER_CYCLE_MS (1000ms) ごとに、WdgM_GlobalStopped を
 *            見て行う（WARN ログのみだった従来の学習用簡略化を解消し、本物の
 *            フェールセーフ動作にした）。
 *
 *            グローバルレベルの EXPIRED 許容サイクル
 *            (コードレビューで発覚・修正済み): 当初は 1 つでも FAILED な
 *            エンティティがあればその場でリフレッシュを止める設計だったが、
 *            AUTOSAR 実仕様（SWS_WdgM_00119-00122）を確認すると、Global
 *            Supervision Status が OK・FAILED・EXPIRED のいずれであっても
 *            WdgIf_SetTriggerCondition（リフレッシュ相当）は同一に呼ばれ続け、
 *            リフレッシュを止めるのは WDGM_GLOBAL_STATUS_STOPPED に達した
 *            ときだけだと判明した。STOPPED に到達するには
 *            WdgMExpiredSupervisionCycleTol 分の判定サイクルを消費する必要が
 *            あり、単発の異常でいきなり止める設計は仕様の意図する猶予機構を
 *            欠いていた。実際、本ファイルの WDGM_ENGINE/WARNING_DEADLINE_*
 *            のコメントに記録されている「NvM の EEPROM ブロッキング書き込みで
 *            数百ms のスケジューラ停止が起き、Deadline 違反を誤検出した」
 *            実機不具合は、まさにこの猶予機構が本来吸収すべきシナリオを、
 *            タイミングマージンの拡張という対症療法だけで凌いでいた状態
 *            だった。WdgM_ExpiredCycleCount・WdgM_GlobalStopped
 *            (WDGM_EXPIRED_SUPERVISION_CYCLE_TOL、詳細は WdgM_Cfg.h 参照) を
 *            追加し、連続 FAILED 判定サイクルがこの許容回数を超えて初めて
 *            リフレッシュを止めるようにした。
 *
 *            重大なリグレッション（実機で発覚・修正済み）: 追加当初、
 *            WdgM_ResumeSupervision() で WdgM_ExpiredCycleCount・
 *            WdgM_GlobalStopped も AliveCount と同様にリセットしていたが、
 *            これだと「ボランタリスリープ→復帰」を繰り返すだけで、
 *            Logical/Deadline Supervision の恒久的な違反（本物のバグ）が
 *            あってもグローバル猶予カウンタが毎回 0 に戻り、猶予を
 *            使い切ってリフレッシュを止める状態へ絶対に到達できなくなる
 *            （フェイルセーフとして機能しなくなる）ことが、実機で
 *            意図的な Logical 違反を注入したテスト（500 秒以上リセットが
 *            発生しなかった）で判明した。WdgM_ResumeSupervision() での
 *            リセットをやめ、真に全エンティティが OK に戻ったときのみ
 *            （WdgM_MainFunction() 末尾の自然な回復判定）クリアされるよう
 *            修正した。あわせて、POST_RUN 中の想定内の Alive 不足が
 *            グローバル猶予を消費してしまわないよう、
 *            WdgM_SupervisionSuppressed 中は猶予カウンタの判定自体を
 *            凍結するようにした。
 *            判定周期とリフレッシュ周期を分離しているのは、Renesas RA4M1 の
 *            IWDT 最大タイムアウト（約 5592ms）が判定サイクル（6000ms）より
 *            短く、判定サイクルに直接リフレッシュを同期できないため
 *            （詳細は WdgM_Cfg.h の WDGM_HW_WATCHDOG_TIMEOUT_MS コメントを参照）。
 *            EcuM が POST_RUN へ遷移する際（Rte_Engine タスクが意図的に停止し
 *            Alive Supervision が必ず FAILED になる）は WdgM_DisableHwWatchdog() を
 *            呼ぶ。WdgM_SupervisionSuppressed フラグも併せて立て、HW を実際に
 *            無効化できない MCU (例: Renesas RA の WDT は一度有効化すると
 *            ソフトウェアから無効化できない) でも、FAILED 判定に関わらず
 *            refresh を継続することで意図しないリセットを防ぐ。RUN へ復帰する
 *            際は WdgM_EnableHwWatchdog() でフラグを下ろし、通常の
 *            FAILED 時リセット動作に戻す。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "WdgM.h"
#include "WdgM_Hw.h"
#include "Det.h"

/* millis() is declared in Arduino wiring.c with C linkage. */
extern unsigned long millis(void);

#define TAG "WdgM"

/* -----------------------------------------------------------------------
 * モジュール内部変数
 * ----------------------------------------------------------------------- */

static const WdgM_ConfigType* WdgM_Cfg = NULL;

/** 1 = WdgM_DisableHwWatchdog() 以降 (POST_RUN 中等)。Alive Supervision の
 *  FAILED 判定に関わらず HW ウォッチドッグを refresh し続ける。
 *  HW を実際に無効化できる MCU では実質的に無意味だが、HW ウォッチドッグを
 *  一度有効化すると無効化できない MCU (例: Renesas RA) でも、POST_RUN の
 *  意図的な Alive Supervision FAILED でリセットしないようにするために使う。 */
static uint8 WdgM_SupervisionSuppressed = 0U;

/** エンティティごとの Alive カウンタ (CheckpointReached 呼び出し回数) */
static uint8 WdgM_AliveCount[WDGM_SUPERVISED_ENTITY_COUNT];

/** エンティティごとの Alive Supervision ステータス (WdgM_MainFunction が周期更新) */
static WdgM_LocalStatusType WdgM_AliveStatus[WDGM_SUPERVISED_ENTITY_COUNT];

/** エンティティごとの Logical Supervision ステータス (WdgM_Init まで FAILED がラッチされる) */
static WdgM_LocalStatusType WdgM_LogicalStatus[WDGM_SUPERVISED_ENTITY_COUNT];

/** エンティティごとの Deadline Supervision ステータス (WdgM_Init まで FAILED がラッチされる) */
static WdgM_LocalStatusType WdgM_DeadlineStatus[WDGM_SUPERVISED_ENTITY_COUNT];

/** エンティティごとの直前のチェックポイント ID (Logical Supervision 用) */
static uint8 WdgM_LastCheckpoint[WDGM_SUPERVISED_ENTITY_COUNT];

/** エンティティごとの直前のチェックポイント発生時刻 [ms] (Deadline Supervision 用) */
static unsigned long WdgM_LastCheckpointTimeMs[WDGM_SUPERVISED_ENTITY_COUNT];

/** 1 = WdgM_ResumeSupervision() 直後、次回の WdgM_MainFunction() 呼び出しで
 *  Alive Supervision の判定を 1 回だけスキップする。
 *  WdgM_MainFunction() 自身（Task 7）は POST_RUN 中も継続動作するタスクのため、
 *  Os 側の呼び出しスケジュール（Os_LastRunMs[7]）は resume イベントと同期して
 *  いない。POST_RUN が WDGM_SUPERVISION_CYCLE_MS (6000ms) より大幅に短い場合、
 *  resume 直後にたまたま次回の判定タイミングが来てしまうと、AliveCount
 *  リセットからエンティティが一度もチェックインできていないうちに FAILED と
 *  誤判定し、HW ウォッチドッグリフレッシュ拒否 → 実リセットに至ることが
 *  実機で確認された（詳細は WdgM_MainFunction() 冒頭のコメント参照）。 */
static uint8 WdgM_SkipNextAliveJudgment = 0U;

/** グローバルレベルの EXPIRED 許容サイクルカウンタ
 *  (AUTOSAR WdgMExpiredSupervisionCycleTol 相当。詳細は WdgM_Cfg.h の
 *  WDGM_EXPIRED_SUPERVISION_CYCLE_TOL コメントを参照)。
 *  いずれかのエンティティが FAILED の判定サイクルが続くたびに増加し、
 *  WDGM_EXPIRED_SUPERVISION_CYCLE_TOL を超えた時点で WdgM_GlobalStopped が
 *  立つ。全エンティティが OK に戻ればリセットされる。 */
static uint8 WdgM_ExpiredCycleCount = 0U;

/** 1 = グローバル許容サイクルを使い切り、AUTOSAR の
 *  WDGM_GLOBAL_STATUS_STOPPED 相当に到達した。この時点で初めて
 *  WdgM_TriggerHwWatchdog() がリフレッシュを拒否する
 *  (SWS_WdgM_00119-00122: OK/FAILED/EXPIRED はいずれもリフレッシュを
 *  継続し、STOPPED でのみ止める)。 */
static uint8 WdgM_GlobalStopped = 0U;

/* -----------------------------------------------------------------------
 * 公開 API
 * ----------------------------------------------------------------------- */

/**
 * \brief   WdgM モジュールを初期化する。
 *
 * \details 全エンティティの Alive カウンタを 0、ステータスを OK にリセットし、
 *          WdgM_EnableHwWatchdog() で AVR 実ハードウェアウォッチドッグを
 *          有効化する。他の全 BSW モジュール初期化が完了した後、Os_Init() の
 *          直前に呼び出すこと（初期化中の正常な処理時間では誤って時間切れに
 *          ならないように、初期化の最後に有効化する）。
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void WdgM_Init(const WdgM_ConfigType* ConfigPtr)
{
    WdgM_Cfg = ConfigPtr;
    for (uint8 i = 0U; i < ConfigPtr->EntityCount; i++)
    {
        WdgM_AliveCount[i]          = 0U;
        WdgM_AliveStatus[i]         = WDGM_LOCAL_STATUS_OK;
        WdgM_LogicalStatus[i]       = WDGM_LOCAL_STATUS_OK;
        WdgM_DeadlineStatus[i]      = WDGM_LOCAL_STATUS_OK;
        WdgM_LastCheckpoint[i]      = WDGM_CP_INITIAL;
        WdgM_LastCheckpointTimeMs[i] = millis();
    }
    WdgM_SkipNextAliveJudgment = 0U;
    WdgM_ExpiredCycleCount     = 0U;
    WdgM_GlobalStopped         = 0U;

    WdgM_EnableHwWatchdog();

    DET_LOGI(TAG, "Init ok entities=%u", (unsigned)ConfigPtr->EntityCount);
}

/**
 * \brief   実 HW ウォッチドッグを WDGM_HW_WATCHDOG_TIMEOUT_MS (4000ms) で有効化する。
 *
 * \ServiceID      {0x07}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void WdgM_EnableHwWatchdog(void)
{
    WdgM_Hw_Enable();  /* WDGM_HW_WATCHDOG_TIMEOUT_MS (4000ms) に対応 */
    WdgM_SupervisionSuppressed = 0U;
    DET_LOGI(TAG, "HW watchdog enabled (4000ms)");
}

/**
 * \brief   実 HW ウォッチドッグを無効化する。
 *
 * \ServiceID      {0x06}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void WdgM_DisableHwWatchdog(void)
{
    WdgM_Hw_Disable();
    WdgM_SupervisionSuppressed = 1U;
    DET_LOGI(TAG, "HW watchdog disabled");
}

/**
 * \brief   全エンティティのチェックポイント追跡基準・Alive Supervision 状態をリセットする。
 *
 * \details WdgM_LastCheckpoint / WdgM_LastCheckpointTimeMs を全エンティティ分
 *          WDGM_CP_INITIAL・現在時刻にリセットする。POST_RUN 中に蓄積した
 *          「最後のチェックポイントからの停止時間」が、再開後最初の
 *          Deadline Supervision 判定に誤って使われることを防ぐ。
 *
 *          あわせて WdgM_AliveCount / WdgM_AliveStatus も OK にリセットする。
 *          POST_RUN 中はアプリタスクが意図的に停止するため Alive カウンタが
 *          不足し、WdgM_MainFunction() が SE を FAILED とラッチすることがある
 *          （POST_RUN 中は WdgM_SupervisionSuppressed により実害はない）。
 *          この FAILED ラッチをリセットせずに RUN へ復帰すると、
 *          WdgM_EnableHwWatchdog() が抑制フラグを解除した直後、実際には
 *          アプリが正常に再開しているにもかかわらず「POST_RUN 中の古い
 *          FAILED ラッチ」のせいで WdgM_TriggerHwWatchdog() がリフレッシュを
 *          拒否し続け、次の WdgM_MainFunction 判定サイクル（最大 6000ms 後）が
 *          来る前に HW ウォッチドッグタイムアウト（4000ms）に達して実際に
 *          MCU がリセットされてしまう（実機で確認された不具合）。
 *          WdgM_LogicalStatus / WdgM_DeadlineStatus はリセットしない
 *          （こちらは POST_RUN 中に新規発生することがなく、Alive とは異なり
 *          純粋な RUN 中の異常のみを検出するため、ラッチしたままでよい）。
 *
 *          あわせて WdgM_SkipNextAliveJudgment を立て、次回の
 *          WdgM_MainFunction() 呼び出し 1 回分だけ Alive Supervision の判定を
 *          スキップする。WdgM_MainFunction()（Task 7）自身は POST_RUN 中も
 *          継続動作するタスクのため、その呼び出しタイミング（Os_LastRunMs[7]）
 *          はこの resume イベントと同期していない。POST_RUN が
 *          WDGM_SUPERVISION_CYCLE_MS (6000ms) より大幅に短い場合、resume 直後に
 *          たまたま次回の判定タイミングが来てしまうことがあり、その場合
 *          AliveCount リセットからエンティティが一度もチェックインできて
 *          いないうちに FAILED と誤判定してしまう（実機で確認された不具合。
 *          詳細は WdgM_MainFunction() 冒頭のコメント参照）。
 *
 *          WdgM_ExpiredCycleCount・WdgM_GlobalStopped は意図的にリセットしない
 *          （実機で見つかった重大な不具合の教訓）。当初はここで一緒にリセット
 *          していたが、それだと「ボランタリスリープ→復帰」を繰り返すだけで、
 *          Logical/Deadline Supervision の恒久的な違反（本物のプログラムフロー
 *          バグ）があってもグローバル猶予カウンタが毎回 0 に戻り、
 *          WDGM_EXPIRED_SUPERVISION_CYCLE_TOL を実質的に使い切れないまま
 *          リフレッシュ拒否 → 実リセットへ絶対に到達できなくなってしまう
 *          （フェイルセーフとして機能しなくなる重大なリグレッション。実機で
 *          意図的な Logical 違反を注入したテストで発覚した）。
 *          Logical/Deadline はそもそも WdgM_Init まで回復しない設計であり、
 *          それに応じて評価するグローバル猶予カウンタも resume では回復させず、
 *          真に全エンティティが OK に戻ったとき（WdgM_MainFunction() 末尾の
 *          回復判定）にのみクリアされるようにする。
 *
 * \ServiceID      {0x08}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void WdgM_ResumeSupervision(void)
{
    if (WdgM_Cfg == NULL)
        return;

    const unsigned long now = millis();
    for (uint8 i = 0U; i < WdgM_Cfg->EntityCount; i++)
    {
        WdgM_LastCheckpoint[i]       = WDGM_CP_INITIAL;
        WdgM_LastCheckpointTimeMs[i] = now;
        WdgM_AliveCount[i]           = 0U;
        WdgM_AliveStatus[i]          = WDGM_LOCAL_STATUS_OK;
    }
    WdgM_SkipNextAliveJudgment = 1U;

    DET_LOGI(TAG, "Supervision resumed (checkpoint baseline reset) entities=%u",
             (unsigned)WdgM_Cfg->EntityCount);
}

/**
 * \brief   Supervised Entity がチェックポイントに到達したことを報告する。
 *
 * \details 内部 Alive カウンタをインクリメントする (Alive Supervision)。
 *          カウンタは uint8 でラップアラウンドするが、期待値が小さいため問題なし。
 *          続けて、直前のチェックポイントから今回のチェックポイントへの遷移が
 *          許可遷移テーブルに含まれるかを即座に確認する (Logical Supervision)。
 *          さらに、直前のチェックポイントからの実際の経過時間が許容範囲内かを
 *          確認する (Deadline Supervision)。
 *
 * \ServiceID      {0x0E}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType WdgM_CheckpointReached(WdgM_SupervisedEntityIdType SEID, uint8 CheckpointId)
{
    if (WdgM_Cfg == NULL || SEID >= WdgM_Cfg->EntityCount)
        return E_NOT_OK;

    WdgM_AliveCount[SEID]++;

    const WdgM_EntityCfgType* entity = &WdgM_Cfg->Entities[SEID];
    const uint8 fromCp = WdgM_LastCheckpoint[SEID];
    const unsigned long now = millis();
    uint8 allowed = 0U;

    for (uint8 i = 0U; i < entity->TransitionCount; i++)
    {
        if (entity->Transitions[i].FromCheckpointId == fromCp
            && entity->Transitions[i].ToCheckpointId == CheckpointId)
        {
            allowed = 1U;
            break;
        }
    }

    if (allowed == 0U)
    {
        WdgM_LogicalStatus[SEID] = WDGM_LOCAL_STATUS_FAILED;
        DET_LOGW(TAG, "SE%u logical FAILED cp %u->%u (unexpected) [HW WDT reset pending]",
                 (unsigned)SEID, (unsigned)fromCp, (unsigned)CheckpointId);
    }

    /* Deadline Supervision: fromCp→CheckpointId に対応する許容範囲が設定されて
     * いれば、実際の経過時間と比較する。WDGM_CP_INITIAL からの最初の遷移は
     * 比較対象の基準時刻が存在しないため対象外とする。 */
    if (fromCp != WDGM_CP_INITIAL)
    {
        for (uint8 d = 0U; d < entity->DeadlineCount; d++)
        {
            const WdgM_DeadlineCfgType* dl = &entity->Deadlines[d];
            if (dl->FromCheckpointId != fromCp || dl->ToCheckpointId != CheckpointId)
                continue;

            unsigned long elapsed = now - WdgM_LastCheckpointTimeMs[SEID];
            if (elapsed < dl->MinMs || elapsed > dl->MaxMs)
            {
                WdgM_DeadlineStatus[SEID] = WDGM_LOCAL_STATUS_FAILED;
                DET_LOGW(TAG, "SE%u deadline FAILED cp %u->%u elapsed=%lu (exp %lu..%lu) [HW WDT reset pending]",
                         (unsigned)SEID, (unsigned)fromCp, (unsigned)CheckpointId,
                         elapsed, dl->MinMs, dl->MaxMs);
            }
            break;
        }
    }

    WdgM_LastCheckpoint[SEID]        = CheckpointId;
    WdgM_LastCheckpointTimeMs[SEID]  = now;

    return E_OK;
}

/**
 * \brief   Supervised Entity の現在のローカルステータスを取得する。
 *
 * \details Alive Supervision (WdgM_AliveStatus)・Logical Supervision
 *          (WdgM_LogicalStatus)・Deadline Supervision (WdgM_DeadlineStatus)
 *          のいずれか一つでも FAILED なら FAILED を返す。
 *
 * \ServiceID      {0x0B}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
WdgM_LocalStatusType WdgM_GetLocalStatus(WdgM_SupervisedEntityIdType SEID)
{
    if (WdgM_Cfg == NULL || SEID >= WdgM_Cfg->EntityCount)
        return WDGM_LOCAL_STATUS_DEACTIVATED;
    if (WdgM_AliveStatus[SEID]    != WDGM_LOCAL_STATUS_OK
        || WdgM_LogicalStatus[SEID]  != WDGM_LOCAL_STATUS_OK
        || WdgM_DeadlineStatus[SEID] != WDGM_LOCAL_STATUS_OK)
        return WDGM_LOCAL_STATUS_FAILED;
    return WDGM_LOCAL_STATUS_OK;
}

/**
 * \brief   WdgM 周期処理。Alive Supervision を評価する。
 *
 * \details 各エンティティの Alive カウンタを検査し、WdgM_AliveStatus に反映する
 *          (WdgM_LogicalStatus / WdgM_DeadlineStatus はここでは変更しない。
 *          いずれも WdgM_Init までラッチされる)。
 *          検査後カウンタをリセットして次サイクルを開始する。
 *          HW ウォッチドッグへの実際のリフレッシュはここでは行わない
 *          (WdgM_TriggerHwWatchdog が別周期で判定結果を見て行う)。
 *
 *          エンティティ判定の後、グローバルレベルの EXPIRED 許容サイクル
 *          (WdgM_ExpiredCycleCount) も更新する。1 つでも FAILED なエンティティ
 *          がある判定サイクルが WDGM_EXPIRED_SUPERVISION_CYCLE_TOL 回を超えて
 *          連続して初めて WdgM_GlobalStopped を立て、
 *          WdgM_TriggerHwWatchdog() がリフレッシュを拒否するようになる
 *          （詳細は WdgM_Cfg.h の WDGM_EXPIRED_SUPERVISION_CYCLE_TOL コメントを
 *          参照。AUTOSAR SWS_WdgM_00119-00122 準拠: OK/FAILED/EXPIRED は
 *          いずれもリフレッシュを継続し、STOPPED でのみ止める）。
 *
 *          WdgM_ResumeSupervision() 直後 1 回だけ判定をスキップする理由:
 *          本関数（Task 7）は POST_RUN 中も継続動作するタスクのため、
 *          Os 側の呼び出しタイミング（Os_LastRunMs[7]）は
 *          WdgM_ResumeSupervision() が AliveCount をリセットするタイミングとは
 *          無関係である。POST_RUN が WDGM_SUPERVISION_CYCLE_MS (6000ms) より
 *          大幅に短い場合、AliveCount リセット直後にたまたま本関数の呼び出し
 *          タイミングが来てしまうことがあり、その場合エンティティが
 *          一度もチェックインできていないうちに「alive=0」で FAILED と
 *          誤判定し、HW ウォッチドッグのリフレッシュ拒否 → 実リセットに
 *          至ることが実機で確認された。AliveCount を触らずに 1 回スキップ
 *          するだけで、蓄積は継続したまま次回（Os 自身の周期により確実に
 *          一定時間後となる）の判定へ持ち越されるため、実害はない。
 *
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void WdgM_MainFunction(void)
{
    if (WdgM_Cfg == NULL)
        return;

    if (WdgM_SkipNextAliveJudgment)
    {
        WdgM_SkipNextAliveJudgment = 0U;
        DET_LOGI(TAG, "Alive judgment skipped once (resume grace period)");
        return;
    }

    for (uint8 i = 0U; i < WdgM_Cfg->EntityCount; i++)
    {
        const WdgM_EntityCfgType* entity = &WdgM_Cfg->Entities[i];

        if (WdgM_AliveCount[i] >= entity->ExpectedAliveIndications)
        {
            if (WdgM_AliveStatus[i] != WDGM_LOCAL_STATUS_OK)
            {
                /* Alive Supervision のみ FAILED から回復 (Logical は別管理) */
                WdgM_AliveStatus[i] = WDGM_LOCAL_STATUS_OK;
                DET_LOGI(TAG, "SE%u alive recovered alive=%u", (unsigned)i, (unsigned)WdgM_AliveCount[i]);
            }
            else
            {
                DET_LOGD(TAG, "SE%u alive OK alive=%u", (unsigned)i, (unsigned)WdgM_AliveCount[i]);
            }
        }
        else
        {
            WdgM_AliveStatus[i] = WDGM_LOCAL_STATUS_FAILED;
            DET_LOGW(TAG, "SE%u alive FAILED alive=%u (exp>=%u)",
                     (unsigned)i,
                     (unsigned)WdgM_AliveCount[i],
                     (unsigned)entity->ExpectedAliveIndications);
        }

        if (WdgM_LogicalStatus[i] != WDGM_LOCAL_STATUS_OK)
            DET_LOGW(TAG, "SE%u logical still FAILED (latched since violation)", (unsigned)i);

        if (WdgM_DeadlineStatus[i] != WDGM_LOCAL_STATUS_OK)
            DET_LOGW(TAG, "SE%u deadline still FAILED (latched since violation)", (unsigned)i);

        /* 次サイクルのためカウンタリセット */
        WdgM_AliveCount[i] = 0U;
    }

    /* ------------------------------------------------------------------
     * グローバルレベルの EXPIRED 許容サイクル判定
     * (AUTOSAR SWS_WdgM_00119-00122・WdgMExpiredSupervisionCycleTol 相当。
     * 詳細は WdgM_Cfg.h の WDGM_EXPIRED_SUPERVISION_CYCLE_TOL コメントを参照)。
     * 1 つでも FAILED なエンティティがあれば猶予カウンタを消費し、
     * WDGM_EXPIRED_SUPERVISION_CYCLE_TOL を超えて初めて WdgM_GlobalStopped を
     * 立てる。全エンティティが OK に戻れば猶予カウンタはリセットされる
     * （AUTOSAR 本来は EXPIRED から OK への回復には別途ルールがあるが、
     * 本実装は前述の 2 値簡略化に合わせてここも単純化している）。
     *
     * WdgM_SupervisionSuppressed 中（POST_RUN 中の意図的な Alive 不足）は
     * カウンタ自体を進めない。POST_RUN 中に Rte_Engine/Rte_Warning が
     * 意図的に停止して Alive Supervision が FAILED になるのは想定内の挙動
     * であり、これを毎回グローバル猶予に食い込ませてしまうと、本物の
     * Logical/Deadline 違反ではなく POST_RUN の頻度・長さ次第で猶予を
     * 消費してしまう（詳細は WdgM_TriggerHwWatchdog() 側の抑制と対になる
     * 判断）。
     * ------------------------------------------------------------------ */
    uint8 anyNotOk = 0U;
    for (uint8 i = 0U; i < WdgM_Cfg->EntityCount; i++)
    {
        if (WdgM_GetLocalStatus(i) != WDGM_LOCAL_STATUS_OK)
        {
            anyNotOk = 1U;
            break;
        }
    }

    if (anyNotOk && WdgM_SupervisionSuppressed)
    {
        /* 抑制中は猶予カウンタを進めも回復させもしない（判定を凍結する）。 */
    }
    else if (anyNotOk)
    {
        if (WdgM_ExpiredCycleCount < WDGM_EXPIRED_SUPERVISION_CYCLE_TOL)
        {
            WdgM_ExpiredCycleCount++;
            DET_LOGW(TAG, "Global status not OK, tolerance %u/%u cycles",
                     (unsigned)WdgM_ExpiredCycleCount, (unsigned)WDGM_EXPIRED_SUPERVISION_CYCLE_TOL);
        }
        else if (!WdgM_GlobalStopped)
        {
            WdgM_GlobalStopped = 1U;
            DET_LOGE(TAG, "Global supervision STOPPED (tolerance exhausted) [HW WDT reset pending]");
        }
    }
    else if (WdgM_ExpiredCycleCount > 0U || WdgM_GlobalStopped)
    {
        WdgM_ExpiredCycleCount = 0U;
        WdgM_GlobalStopped     = 0U;
        DET_LOGI(TAG, "Global status recovered (all entities OK)");
    }
}

/**
 * \brief   HW ウォッチドッグの trigger（リフレッシュ）処理。
 *
 * \details WdgM_GlobalStopped が立っていない限り（または
 *          WdgM_SupervisionSuppressed による抑制中は無条件に）
 *          WdgM_Hw_Refresh() を呼ぶ。
 *
 *          AUTOSAR SWS_WdgM_00119-00122 準拠: Global Supervision Status が
 *          OK・FAILED・EXPIRED のいずれであってもリフレッシュは継続する。
 *          リフレッシュを止めるのは WDGM_GLOBAL_STATUS_STOPPED
 *          （本実装では WdgM_GlobalStopped、WdgM_MainFunction() が
 *          WDGM_EXPIRED_SUPERVISION_CYCLE_TOL 回の連続 FAILED 判定サイクルを
 *          経て初めて立てる）に到達したときだけである。1 つのエンティティが
 *          FAILED になった瞬間に即座にリフレッシュを止める設計ではない
 *          （旧実装はこの猶予機構を持たず、単発の異常でも即座に停止していた）。
 *
 *          WdgM_MainFunction の判定サイクル (6000ms) より高頻度
 *          (WDGM_HW_TRIGGER_CYCLE_MS = 1000ms) で呼ばれることを想定しており、
 *          WdgM_GlobalStopped が変化してから最短 1 サイクル以内にリフレッシュ
 *          停止へ反映される。
 *
 * \ServiceID      {0x09}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void WdgM_TriggerHwWatchdog(void)
{
    if (!WdgM_GlobalStopped || WdgM_SupervisionSuppressed)
    {
        WdgM_Hw_Refresh();
        DET_LOGD(TAG, "HW watchdog refreshed");
    }
    else
    {
        DET_LOGE(TAG, "HW watchdog NOT refreshed - reset imminent");
    }
}
