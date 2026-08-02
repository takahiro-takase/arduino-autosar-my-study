/**
 * \file    Gpt.c
 * \brief   GPT Driver 実装 (AUTOSAR SWS_Gpt 準拠)
 * \details 実際の HW タイマ（Renesas RA FspTimer）への Start/Stop は
 *          Gpt_Hw 層に委譲し、本ファイルは FspTimer / Arduino API を
 *          直接知らない（Wdg.c が Wdg_Hw に委譲するのと同じ境界の引き方。
 *          詳細は Gpt_Hw.h 参照）。
 *
 *          チャネル状態機械とチック処理:
 *            Gpt_Hw 層は、コンフィグされた TickFrequencyHz どおりの
 *            周期（例: 1000Hz なら 1ms ごと）で実 HW 割り込みを発生させ、
 *            そのたびに本ファイルの Gpt_OnTick() を呼ぶ。目標時間到達の
 *            検出（Value との比較）は HW のコンペアマッチではなく、
 *            Gpt_OnTick() 内のソフトウェア比較で行う。
 *            実装効率よりも「実機での動作が値のずれなく確実に検証できる
 *            こと」を優先した設計であり、正確な GetTimeElapsed/
 *            GetTimeRemaining を素直な整数演算だけで実現できる
 *            （Gpt_Hw の生カウンタレジスタの計数方向 [AGT は down-count,
 *            GPT は up-count] を実機で確認する必要がない）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include "Gpt.h"
#include "Gpt_Hw.h"
#include "SchM.h"
#include "Det.h"

#define TAG "Gpt"

/** チャネル状態機械（Gpt.h 冒頭のコメント参照）。 */
typedef enum
{
    GPT_CH_STATE_INITIALIZED = 0U,  /**< 一度も Start されていない */
    GPT_CH_STATE_RUNNING,
    GPT_CH_STATE_STOPPED,
    GPT_CH_STATE_EXPIRED            /**< ONESHOT のみ: 目標時間到達で自動停止 */
} Gpt_ChannelStateType;

static const Gpt_ConfigType* Gpt_Cfg = NULL;

/* Gpt_OnTick()（真の HW 割り込みコンテキスト）と、GetTimeElapsed/
 * GetTimeRemaining/StopTimer（メインループのタスク）の両方から読み書き
 * されるため volatile。複数フィールドにまたがる読み出し・書き換えは
 * SchM_Enter/Exit_Gpt_CHANNEL_EXCLUSIVE_AREA() で保護すること
 * （Can.c の Can_RxIrqPending と同じ考え方。SchM.h 参照）。 */
static volatile Gpt_ChannelStateType Gpt_ChannelState[GPT_CHANNEL_COUNT];
static volatile uint8                Gpt_NotificationEnabled[GPT_CHANNEL_COUNT];
static volatile Gpt_ValueType        Gpt_TargetValue[GPT_CHANNEL_COUNT];
/** running 中の現在値。RUNNING を離れると Gpt_OnTick() はそのチャネルを
 *  「running でない」として即座に無視するため、以降このチャネルの値は
 *  次の Gpt_StartTimer() まで自然に凍結される（STOPPED/EXPIRED 時点の値を
 *  保持するための別配列は不要）。 */
static volatile Gpt_ValueType        Gpt_ElapsedTicks[GPT_CHANNEL_COUNT];

/* -----------------------------------------------------------------------
 * 内部ヘルパ
 * ----------------------------------------------------------------------- */

static uint8 Gpt_IsValidChannel(Gpt_ChannelType Channel)
{
    return (Gpt_Cfg != NULL) && (Channel < Gpt_Cfg->ChannelCount);
}

/* -----------------------------------------------------------------------
 * Gpt_Hw から呼ばれる ISR コンテキスト関数
 * ----------------------------------------------------------------------- */

/**
 * \brief   Gpt_Hw 層の HW タイマ割り込みから、チャネルの TickFrequencyHz
 *          ごとに 1 回呼ばれる。
 *
 * \details ISR コンテキストで実行されるため、ここで行ってよいのは
 *          状態機械の更新と GptNotification 関数ポインタの呼び出しのみ
 *          （Det_LOGx 等のブロッキング処理は一切行わない。Gpt.h 冒頭の
 *          コメント参照）。
 */
void Gpt_OnTick(Gpt_ChannelType Channel)
{
    if (!Gpt_IsValidChannel(Channel)) return;
    if (Gpt_ChannelState[Channel] != GPT_CH_STATE_RUNNING) return;

    Gpt_ElapsedTicks[Channel]++;

    if (Gpt_ElapsedTicks[Channel] >= Gpt_TargetValue[Channel])
    {
        const Gpt_ChannelConfigType* chCfg = &Gpt_Cfg->Channels[Channel];

        if (chCfg->Mode == GPT_CH_MODE_ONESHOT)
        {
            Gpt_Hw_StopTimer(Channel);
            Gpt_ChannelState[Channel] = GPT_CH_STATE_EXPIRED;
        }
        else
        {
            Gpt_ElapsedTicks[Channel] = 0U;  /* CONTINUOUS: 次サイクルへ ([SWS_Gpt_00361]) */
        }

        if ((Gpt_NotificationEnabled[Channel] != 0U) && (chCfg->Notification != NULL))
        {
            chCfg->Notification();
        }
    }
}

/* -----------------------------------------------------------------------
 * 公開 API
 * ----------------------------------------------------------------------- */

void Gpt_Init(const Gpt_ConfigType* ConfigPtr)
{
    if (ConfigPtr == NULL)
    {
        DET_LOGE(TAG, "Init: NULL ConfigPtr");
        Det_ReportError(GPT_MODULE_ID, 0U, GPT_API_ID_INIT, GPT_E_PARAM_POINTER);
        return;
    }

    if (Gpt_Cfg != NULL)
    {
        Det_ReportError(GPT_MODULE_ID, 0U, GPT_API_ID_INIT, GPT_E_ALREADY_INITIALIZED);
        return;
    }

    if (ConfigPtr->ChannelCount > GPT_CHANNEL_COUNT)
    {
        /* 静的配列 Gpt_ChannelState 等のサイズ (GPT_CHANNEL_COUNT) を
         * 超えるチャネル数は扱えない。ConfigPtr は常に Gpt_PBCfg.c の
         * Gpt_Config（ChannelCount == GPT_CHANNEL_COUNT）を指すため
         * 通常到達しないが、防御的にチェックする。 */
        DET_LOGE(TAG, "Init: ChannelCount %u exceeds GPT_CHANNEL_COUNT %u",
                 (unsigned)ConfigPtr->ChannelCount, (unsigned)GPT_CHANNEL_COUNT);
        Det_ReportError(GPT_MODULE_ID, 0U, GPT_API_ID_INIT, GPT_E_INIT_FAILED);
        return;
    }

    Gpt_Hw_Init();

    for (uint8 i = 0U; i < ConfigPtr->ChannelCount; i++)
    {
        Gpt_ChannelState[i]        = GPT_CH_STATE_INITIALIZED;
        Gpt_NotificationEnabled[i] = 0U;
        Gpt_TargetValue[i]         = 0U;
        Gpt_ElapsedTicks[i]        = 0U;
    }

    Gpt_Cfg = ConfigPtr;

    DET_LOGI(TAG, "Init ok channels=%u", (unsigned)ConfigPtr->ChannelCount);
}

void Gpt_DeInit(void)
{
    if (Gpt_Cfg == NULL)
    {
        Det_ReportError(GPT_MODULE_ID, 0U, GPT_API_ID_DE_INIT, GPT_E_UNINIT);
        return;
    }

    for (uint8 i = 0U; i < Gpt_Cfg->ChannelCount; i++)
    {
        if (Gpt_ChannelState[i] == GPT_CH_STATE_RUNNING)
        {
            Det_ReportError(GPT_MODULE_ID, 0U, GPT_API_ID_DE_INIT, GPT_E_BUSY);
            return;
        }
    }

    Gpt_Hw_DeInit();
    Gpt_Cfg = NULL;

    DET_LOGI(TAG, "DeInit ok");
}

Gpt_ValueType Gpt_GetTimeElapsed(Gpt_ChannelType Channel)
{
    if (Gpt_Cfg == NULL)
    {
        Det_ReportError(GPT_MODULE_ID, 0U, GPT_API_ID_GET_TIME_ELAPSED, GPT_E_UNINIT);
        return 0U;
    }
    if (!Gpt_IsValidChannel(Channel))
    {
        Det_ReportError(GPT_MODULE_ID, 0U, GPT_API_ID_GET_TIME_ELAPSED, GPT_E_PARAM_CHANNEL);
        return 0U;
    }

    /* 状態 (Gpt_ChannelState) を見てから対応する値を読む一連の操作を
     * 割り込みに割り込まれると、食い違った組み合わせを返しかねない
     * (SchM.h の Gpt 排他エリアのコメント参照)。RUNNING を離れると
     * Gpt_ElapsedTicks はそのチャネルで凍結されるため、STOPPED/EXPIRED
     * でも同じ配列をそのまま読めばよい（INITIALIZED はゼロ初期化済み）。 */
    Gpt_ValueType result;
    SchM_Enter_Gpt_CHANNEL_EXCLUSIVE_AREA();
    result = Gpt_ElapsedTicks[Channel];
    SchM_Exit_Gpt_CHANNEL_EXCLUSIVE_AREA();
    return result;
}

Gpt_ValueType Gpt_GetTimeRemaining(Gpt_ChannelType Channel)
{
    if (Gpt_Cfg == NULL)
    {
        Det_ReportError(GPT_MODULE_ID, 0U, GPT_API_ID_GET_TIME_REMAINING, GPT_E_UNINIT);
        return 0U;
    }
    if (!Gpt_IsValidChannel(Channel))
    {
        Det_ReportError(GPT_MODULE_ID, 0U, GPT_API_ID_GET_TIME_REMAINING, GPT_E_PARAM_CHANNEL);
        return 0U;
    }

    /* RUNNING/STOPPED は TargetValue-ElapsedTicks の減算で正しい残り時間になる。
     * EXPIRED は ElapsedTicks==TargetValue で凍結済みのため自然に 0
     * ([SWS_Gpt_00305] が要求する値と一致)、INITIALIZED は両方 0 のため
     * これも自然に 0 になる。状態で分岐する必要はない。 */
    Gpt_ValueType result;
    SchM_Enter_Gpt_CHANNEL_EXCLUSIVE_AREA();
    result = Gpt_TargetValue[Channel] - Gpt_ElapsedTicks[Channel];
    SchM_Exit_Gpt_CHANNEL_EXCLUSIVE_AREA();
    return result;
}

void Gpt_StartTimer(Gpt_ChannelType Channel, Gpt_ValueType Value)
{
    if (Gpt_Cfg == NULL)
    {
        Det_ReportError(GPT_MODULE_ID, 0U, GPT_API_ID_START_TIMER, GPT_E_UNINIT);
        return;
    }
    if (!Gpt_IsValidChannel(Channel))
    {
        Det_ReportError(GPT_MODULE_ID, 0U, GPT_API_ID_START_TIMER, GPT_E_PARAM_CHANNEL);
        return;
    }

    const Gpt_ChannelConfigType* chCfg = &Gpt_Cfg->Channels[Channel];

    /* [SWS_Gpt_00218]: Value が 0、または TickValueMax（そのチャネルの HW
     * カウンタが表現できる最大 tick 数）を超える場合は不正。Channel 0 は
     * TickValueMax=0xFFFFFFFF（32-bit フルレンジ）のため現状は Value==0
     * 以外で引っかからないが、将来 16-bit HW カウンタ等の狭いチャネルを
     * 追加した際に必要になる（Gpt_PBCfg.c 参照）。 */
    if ((Value == 0U) || (Value > chCfg->TickValueMax))
    {
        Det_ReportError(GPT_MODULE_ID, 0U, GPT_API_ID_START_TIMER, GPT_E_PARAM_VALUE);
        return;
    }
    if (Gpt_ChannelState[Channel] == GPT_CH_STATE_RUNNING)
    {
        Det_ReportError(GPT_MODULE_ID, 0U, GPT_API_ID_START_TIMER, GPT_E_BUSY);
        return;
    }

    /* Gpt_Hw_StartTimer() が成功した時点で実 HW 割り込みが発生しうる。
     * 先に HW を起動してから状態を設定すると、その間に発生した tick を
     * Gpt_OnTick() が「running でない」として捨ててしまう（初回 tick
     * ロスト）ため、状態を先に "running" として確定させてから HW を
     * 起動する。まだ HW は動いていないため、この書き込み自体は
     * Gpt_OnTick() と競合しない（排他エリア不要）。 */
    Gpt_TargetValue[Channel]  = Value;
    Gpt_ElapsedTicks[Channel] = 0U;
    Gpt_ChannelState[Channel] = GPT_CH_STATE_RUNNING;

    if (Gpt_Hw_StartTimer(Channel, chCfg->TickFrequencyHz) != E_OK)
    {
        /* GPT_E_INIT_FAILED は仕様上 Gpt_Init 専用のエラーコード
         * ([SWS_Gpt_00404]) であり StartTimer には対応する開発エラーコードが
         * 存在しない（実 AUTOSAR は HW タイマが静的に予約済みという前提のため、
         * StartTimer 時点での HW 確保失敗を想定していない。本実装は
         * FspTimer::get_available_timer() で実行時に空きチャネルを選ぶため、
         * 理論上失敗しうる）。Wdg_Hw の HW 起動失敗と同様、DET_LOGE のみで
         * 通知し、Det_ReportError は呼ばない。HW が起動していないため
         * 状態はロールバックする。 */
        Gpt_ChannelState[Channel] = GPT_CH_STATE_STOPPED;
        DET_LOGE(TAG, "StartTimer: Gpt_Hw_StartTimer failed ch=%u", (unsigned)Channel);
        return;
    }

    DET_LOGI(TAG, "StartTimer ch=%u value=%lu", (unsigned)Channel, (unsigned long)Value);
}

void Gpt_StopTimer(Gpt_ChannelType Channel)
{
    if (Gpt_Cfg == NULL)
    {
        Det_ReportError(GPT_MODULE_ID, 0U, GPT_API_ID_STOP_TIMER, GPT_E_UNINIT);
        return;
    }
    if (!Gpt_IsValidChannel(Channel))
    {
        Det_ReportError(GPT_MODULE_ID, 0U, GPT_API_ID_STOP_TIMER, GPT_E_PARAM_CHANNEL);
        return;
    }

    /* 状態確認・状態遷移までを 1 つの排他エリアで囲う。状態を "stopped" に
     * 確定させてしまえば、以降 Gpt_OnTick() は running でないチャネルとして
     * 即座に無視し Gpt_ElapsedTicks もそのまま凍結されるため、実際の HW 停止
     * (Gpt_Hw_StopTimer、レジスタ書き込みのみ) は排他エリアの外で行っても
     * 安全（SchM.h の Gpt 排他エリアのコメント参照）。 */
    uint8 wasRunning;
    SchM_Enter_Gpt_CHANNEL_EXCLUSIVE_AREA();
    wasRunning = (Gpt_ChannelState[Channel] == GPT_CH_STATE_RUNNING) ? 1U : 0U;
    if (wasRunning != 0U)
    {
        Gpt_ChannelState[Channel] = GPT_CH_STATE_STOPPED;
    }
    SchM_Exit_Gpt_CHANNEL_EXCLUSIVE_AREA();

    /* initialized/stopped/expired での呼び出しは無害 (状態変化なし、[SWS_Gpt_00344]) */
    if (wasRunning == 0U) return;

    Gpt_Hw_StopTimer(Channel);

    DET_LOGI(TAG, "StopTimer ch=%u elapsed=%lu", (unsigned)Channel, (unsigned long)Gpt_ElapsedTicks[Channel]);
}

void Gpt_EnableNotification(Gpt_ChannelType Channel)
{
    if (Gpt_Cfg == NULL)
    {
        Det_ReportError(GPT_MODULE_ID, 0U, GPT_API_ID_ENABLE_NOTIFICATION, GPT_E_UNINIT);
        return;
    }
    if (!Gpt_IsValidChannel(Channel) || (Gpt_Cfg->Channels[Channel].Notification == NULL))
    {
        Det_ReportError(GPT_MODULE_ID, 0U, GPT_API_ID_ENABLE_NOTIFICATION, GPT_E_PARAM_CHANNEL);
        return;
    }

    Gpt_NotificationEnabled[Channel] = 1U;
}

void Gpt_DisableNotification(Gpt_ChannelType Channel)
{
    if (Gpt_Cfg == NULL)
    {
        Det_ReportError(GPT_MODULE_ID, 0U, GPT_API_ID_DISABLE_NOTIFICATION, GPT_E_UNINIT);
        return;
    }
    if (!Gpt_IsValidChannel(Channel) || (Gpt_Cfg->Channels[Channel].Notification == NULL))
    {
        Det_ReportError(GPT_MODULE_ID, 0U, GPT_API_ID_DISABLE_NOTIFICATION, GPT_E_PARAM_CHANNEL);
        return;
    }

    Gpt_NotificationEnabled[Channel] = 0U;
}

void Gpt_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    if (versioninfo == NULL)
    {
        Det_ReportError(GPT_MODULE_ID, 0U, GPT_API_ID_GET_VERSION_INFO, GPT_E_PARAM_POINTER);
        return;
    }

    versioninfo->vendorID         = GPT_VENDOR_ID;
    versioninfo->moduleID         = GPT_MODULE_ID;
    versioninfo->sw_major_version = GPT_SW_MAJOR_VERSION;
    versioninfo->sw_minor_version = GPT_SW_MINOR_VERSION;
    versioninfo->sw_patch_version = GPT_SW_PATCH_VERSION;
}
