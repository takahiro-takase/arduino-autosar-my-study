/**
 * \file    WdgM.c
 * \brief   ウォッチドッグマネージャ 実装 (AUTOSAR SWS_WdgM 準拠)
 * \details Supervised Entity の Alive Supervision / Logical Supervision を管理する。
 *
 *          Alive Supervision アルゴリズム:
 *            1. 監視対象 Runnable が WdgM_CheckpointReached() を呼ぶたびに
 *               WdgM_AliveCount[SEID] をインクリメントする。
 *            2. WdgM_MainFunction() (WDGM_SUPERVISION_CYCLE_MS 周期) が
 *               WdgM_AliveCount >= WDGM_EXPECTED_ALIVE_INDICATIONS を確認する。
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
 *          Alive と Logical の判定結果統合 (AUTOSAR 同等の挙動):
 *            WdgM_AliveStatus[] (Alive Supervision 用) と WdgM_LogicalStatus[]
 *            (Logical Supervision 用) を別々の配列で保持する。WdgM_GetLocalStatus()
 *            と WdgM_MainFunction() の HW ウォッチドッグ refresh 判定は、両方が
 *            OK の場合のみ OK とみなす。WdgM_AliveStatus は周期ごとに OK/FAILED を
 *            再評価するが、WdgM_LogicalStatus は WdgM_Init() でのみ OK に戻る
 *            （不正な遷移が起きたという事実は Alive 条件を満たしても消えない）。
 *            旧実装ではこの 2 つを 1 つの WdgM_LocalStatus に統合していたため、
 *            Logical Supervision が FAILED と判定した直後でも次の MainFunction
 *            サイクルで Alive 条件を満たせば OK に上書きされ、検出した違反が
 *            消えてしまう問題があった。
 *
 *          HW ウォッチドッグ連携:
 *            WdgM_Init() で AVR 実ハードウェアウォッチドッグ (<avr/wdt.h>) を
 *            WDGM_HW_WATCHDOG_TIMEOUT_MS (8000ms) で有効化する。
 *            WdgM_MainFunction() は全エンティティが OK の場合のみ wdt_reset() を
 *            呼ぶ。1 つでも FAILED があれば呼ばないため、リフレッシュが止まり
 *            タイムアウト後に実際に MCU がリセットされる（WARN ログのみだった
 *            従来の学習用簡略化を解消し、本物のフェールセーフ動作にした）。
 *            EcuM が POST_RUN へ遷移する際（Rte_Engine タスクが意図的に停止し
 *            Alive Supervision が必ず FAILED になる）は WdgM_DisableHwWatchdog() で
 *            無効化し、RUN へ復帰する際は WdgM_EnableHwWatchdog() で再度有効化する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "WdgM.h"
#include "Det.h"
#include <avr/wdt.h>

#define TAG "WdgM"

/* -----------------------------------------------------------------------
 * モジュール内部変数
 * ----------------------------------------------------------------------- */

static const WdgM_ConfigType* WdgM_Cfg = NULL;

/** エンティティごとの Alive カウンタ (CheckpointReached 呼び出し回数) */
static uint8 WdgM_AliveCount[WDGM_SUPERVISED_ENTITY_COUNT];

/** エンティティごとの Alive Supervision ステータス (WdgM_MainFunction が周期更新) */
static WdgM_LocalStatusType WdgM_AliveStatus[WDGM_SUPERVISED_ENTITY_COUNT];

/** エンティティごとの Logical Supervision ステータス (WdgM_Init まで FAILED がラッチされる) */
static WdgM_LocalStatusType WdgM_LogicalStatus[WDGM_SUPERVISED_ENTITY_COUNT];

/** エンティティごとの直前のチェックポイント ID (Logical Supervision 用) */
static uint8 WdgM_LastCheckpoint[WDGM_SUPERVISED_ENTITY_COUNT];

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
        WdgM_AliveCount[i]     = 0U;
        WdgM_AliveStatus[i]    = WDGM_LOCAL_STATUS_OK;
        WdgM_LogicalStatus[i]  = WDGM_LOCAL_STATUS_OK;
        WdgM_LastCheckpoint[i] = WDGM_CP_INITIAL;
    }

    WdgM_EnableHwWatchdog();

    DET_LOGI(TAG, "Init ok entities=%u", (unsigned)ConfigPtr->EntityCount);
}

/**
 * \brief   AVR 実ハードウェアウォッチドッグを WDTO_8S (8000ms) で有効化する。
 *
 * \ServiceID      {0x07}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void WdgM_EnableHwWatchdog(void)
{
    //wdt_enable(WDTO_8S);  /* WDGM_HW_WATCHDOG_TIMEOUT_MS (8000ms) に対応 */
    DET_LOGI(TAG, "HW watchdog enabled (8000ms)");
}

/**
 * \brief   AVR 実ハードウェアウォッチドッグを無効化する。
 *
 * \ServiceID      {0x06}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void WdgM_DisableHwWatchdog(void)
{
    wdt_disable();
    DET_LOGI(TAG, "HW watchdog disabled");
}

/**
 * \brief   Supervised Entity がチェックポイントに到達したことを報告する。
 *
 * \details 内部 Alive カウンタをインクリメントする (Alive Supervision)。
 *          カウンタは uint8 でラップアラウンドするが、期待値が小さいため問題なし。
 *          続けて、直前のチェックポイントから今回のチェックポイントへの遷移が
 *          許可遷移テーブルに含まれるかを即座に確認する (Logical Supervision)。
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

    WdgM_LastCheckpoint[SEID] = CheckpointId;

    return E_OK;
}

/**
 * \brief   Supervised Entity の現在のローカルステータスを取得する。
 *
 * \details Alive Supervision (WdgM_AliveStatus) と Logical Supervision
 *          (WdgM_LogicalStatus) のどちらか一方でも FAILED なら FAILED を返す。
 *
 * \ServiceID      {0x0B}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
WdgM_LocalStatusType WdgM_GetLocalStatus(WdgM_SupervisedEntityIdType SEID)
{
    if (WdgM_Cfg == NULL || SEID >= WdgM_Cfg->EntityCount)
        return WDGM_LOCAL_STATUS_DEACTIVATED;
    if (WdgM_AliveStatus[SEID] != WDGM_LOCAL_STATUS_OK
        || WdgM_LogicalStatus[SEID] != WDGM_LOCAL_STATUS_OK)
        return WDGM_LOCAL_STATUS_FAILED;
    return WDGM_LOCAL_STATUS_OK;
}

/**
 * \brief   WdgM 周期処理。Alive Supervision を評価し、HW ウォッチドッグを refresh する。
 *
 * \details 各エンティティの Alive カウンタを検査し、WdgM_AliveStatus に反映する
 *          (WdgM_LogicalStatus はここでは変更しない。WdgM_Init までラッチされる)。
 *          検査後カウンタをリセットして次サイクルを開始する。
 *          全エンティティの WdgM_GetLocalStatus() が OK の場合のみ wdt_reset() を
 *          呼ぶ。1 つでも FAILED (Alive 不足または Logical Supervision 違反) が
 *          あれば呼ばないため、WDGM_HW_WATCHDOG_TIMEOUT_MS 後に実際に MCU が
 *          リセットされる。
 *
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void WdgM_MainFunction(void)
{
    if (WdgM_Cfg == NULL)
        return;

    uint8 allOk = 1U;

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
            DET_LOGW(TAG, "SE%u alive FAILED alive=%u (exp>=%u) [HW WDT reset pending]",
                     (unsigned)i,
                     (unsigned)WdgM_AliveCount[i],
                     (unsigned)entity->ExpectedAliveIndications);
        }

        if (WdgM_LogicalStatus[i] != WDGM_LOCAL_STATUS_OK)
            DET_LOGW(TAG, "SE%u logical still FAILED (latched since violation) [HW WDT reset pending]",
                     (unsigned)i);

        if (WdgM_GetLocalStatus(i) != WDGM_LOCAL_STATUS_OK)
            allOk = 0U;

        /* 次サイクルのためカウンタリセット */
        WdgM_AliveCount[i] = 0U;
    }

    if (allOk)
    {
        wdt_reset();
        DET_LOGD(TAG, "HW watchdog refreshed");
    }
    else
    {
        DET_LOGE(TAG, "HW watchdog NOT refreshed - reset imminent");
    }
}
