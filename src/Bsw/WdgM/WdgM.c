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
 *            2. 含まれない場合は LOCAL_STATUS_FAILED にして WARN ログを出力する
 *               (MainFunction の周期を待たず即時検出)。
 *            3. WdgM_LastCheckpoint[SEID] を今回の CheckpointId に更新する。
 *
 *          失敗時のアクション (本プロジェクト):
 *            WARN ログで通知する。
 *            実機製品では WdgM が HW ウォッチドッグのリフレッシュを停止し
 *            タイムアウト後にシステムリセットが発生する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "WdgM.h"
#include "Det.h"

#define TAG "WdgM"

/* -----------------------------------------------------------------------
 * モジュール内部変数
 * ----------------------------------------------------------------------- */

static const WdgM_ConfigType* WdgM_Cfg = NULL;

/** エンティティごとの Alive カウンタ (CheckpointReached 呼び出し回数) */
static uint8 WdgM_AliveCount[WDGM_SUPERVISED_ENTITY_COUNT];

/** エンティティごとのローカルステータス */
static WdgM_LocalStatusType WdgM_LocalStatus[WDGM_SUPERVISED_ENTITY_COUNT];

/** エンティティごとの直前のチェックポイント ID (Logical Supervision 用) */
static uint8 WdgM_LastCheckpoint[WDGM_SUPERVISED_ENTITY_COUNT];

/* -----------------------------------------------------------------------
 * 公開 API
 * ----------------------------------------------------------------------- */

/**
 * \brief   WdgM モジュールを初期化する。
 *
 * \details 全エンティティの Alive カウンタを 0、ステータスを OK にリセットする。
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
        WdgM_AliveCount[i]    = 0U;
        WdgM_LocalStatus[i]   = WDGM_LOCAL_STATUS_OK;
        WdgM_LastCheckpoint[i] = WDGM_CP_INITIAL;
    }
    DET_LOGI(TAG, "Init ok entities=%u", (unsigned)ConfigPtr->EntityCount);
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
        WdgM_LocalStatus[SEID] = WDGM_LOCAL_STATUS_FAILED;
        DET_LOGW(TAG, "SE%u logical FAILED cp %u->%u (unexpected) [HW reset in production]",
                 (unsigned)SEID, (unsigned)fromCp, (unsigned)CheckpointId);
    }

    WdgM_LastCheckpoint[SEID] = CheckpointId;

    return E_OK;
}

/**
 * \brief   Supervised Entity の現在のローカルステータスを取得する。
 *
 * \ServiceID      {0x0B}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
WdgM_LocalStatusType WdgM_GetLocalStatus(WdgM_SupervisedEntityIdType SEID)
{
    if (WdgM_Cfg == NULL || SEID >= WdgM_Cfg->EntityCount)
        return WDGM_LOCAL_STATUS_DEACTIVATED;
    return WdgM_LocalStatus[SEID];
}

/**
 * \brief   WdgM 周期処理。Alive Supervision を評価する。
 *
 * \details 各エンティティの Alive カウンタを検査し、結果をステータスに反映する。
 *          検査後カウンタをリセットして次サイクルを開始する。
 *
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void WdgM_MainFunction(void)
{
    if (WdgM_Cfg == NULL)
        return;

    for (uint8 i = 0U; i < WdgM_Cfg->EntityCount; i++)
    {
        const WdgM_EntityCfgType* entity = &WdgM_Cfg->Entities[i];

        if (WdgM_AliveCount[i] >= entity->ExpectedAliveIndications)
        {
            if (WdgM_LocalStatus[i] != WDGM_LOCAL_STATUS_OK)
            {
                /* FAILED から回復 */
                WdgM_LocalStatus[i] = WDGM_LOCAL_STATUS_OK;
                DET_LOGI(TAG, "SE%u recovered alive=%u", (unsigned)i, (unsigned)WdgM_AliveCount[i]);
            }
            else
            {
                DET_LOGD(TAG, "SE%u OK alive=%u", (unsigned)i, (unsigned)WdgM_AliveCount[i]);
            }
        }
        else
        {
            WdgM_LocalStatus[i] = WDGM_LOCAL_STATUS_FAILED;
            DET_LOGW(TAG, "SE%u FAILED alive=%u (exp>=%u) [HW reset in production]",
                     (unsigned)i,
                     (unsigned)WdgM_AliveCount[i],
                     (unsigned)entity->ExpectedAliveIndications);
        }

        /* 次サイクルのためカウンタリセット */
        WdgM_AliveCount[i] = 0U;
    }
}
