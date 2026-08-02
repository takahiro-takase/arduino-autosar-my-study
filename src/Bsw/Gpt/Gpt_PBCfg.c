/**
 * \file    Gpt_PBCfg.c
 * \brief   GPT Driver ポストビルドコンフィグ 定義
 * \details プロジェクトで使用する GPT チャネルテーブルを定義する。
 *          AUTOSAR 環境ではコンフィギュレーションツールが自動生成するファイル
 *          に相当する。
 *
 *          チャネル一覧:
 *            Channel 0: TickFrequencyHz=1000（1ms 分解能）、CONTINUOUS、
 *                       目標 1000 tick（= 1000 ms 周期）で
 *                       App_GptDemo_OnTick() を通知する（src/Asw/App_GptDemo.c
 *                       Init() が Gpt_StartTimer(GPT_CHANNEL_0, 1000U) +
 *                       Gpt_EnableNotification(GPT_CHANNEL_0) を実行して起動する）。
 *                       1000 という周期値は、既存の DET ログのタイムスタンプ
 *                       (millis() 由来) と目視で突き合わせやすい丸い数値として
 *                       選んだだけで、仕様上の制約はない。
 *            Channel 1: TickFrequencyHz=1000（1ms 分解能）、CONTINUOUS、
 *                       Notification なし。Os (src/Os/Os.c) が
 *                       Os_Init() で Gpt_StartTimer(GPT_CHANNEL_1, 0xFFFFFFFFU)
 *                       により起動し、以後 Gpt_GetTimeElapsed(GPT_CHANNEL_1) を
 *                       millis() の代わりにスケジューラの時間源として使う。
 *                       目標値を 32-bit フルレンジにしているのは millis() と
 *                       同じ「アンサインド演算で自然にラップアラウンドする」
 *                       挙動に揃えるため（Os.c 冒頭のオーバフロー安全性の
 *                       コメント参照）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "Gpt_PBCfg.h"

/* 通知関数の前方宣言 (Gpt が個々の Asw モジュールに依存しないよう、
 * App_GptDemo.h をインクルードする代わりに extern 宣言を使う。
 * Os_PBCfg.c がタスク関数を extern 宣言するのと同じ考え方)。 */
extern void App_GptDemo_OnTick(void);

/* -----------------------------------------------------------------------
 * チャネルテーブル
 * インデックスがそのままチャネル番号 (Gpt_ChannelType) に対応する。
 * ----------------------------------------------------------------------- */
static const Gpt_ChannelConfigType Gpt_ChannelTable[GPT_CHANNEL_COUNT] =
{
    /* Channel 0 */
    {
        GPT_CHANNEL_0,          /* ChannelId        */
        GPT_CH_MODE_CONTINUOUS, /* Mode             */
        1000U,                  /* TickFrequencyHz  : 1ms 分解能 */
        0xFFFFFFFFU,            /* TickValueMax     : 32-bit フルレンジ */
        App_GptDemo_OnTick      /* Notification     */
    },
    /* Channel 1: Os 専用のスケジューラティック（Notification 不要） */
    {
        GPT_CHANNEL_1,          /* ChannelId        */
        GPT_CH_MODE_CONTINUOUS, /* Mode             */
        1000U,                  /* TickFrequencyHz  : 1ms 分解能 */
        0xFFFFFFFFU,            /* TickValueMax     : 32-bit フルレンジ (millis() 同様に自然ラップ) */
        NULL                    /* Notification     : Os は Gpt_GetTimeElapsed() をポーリングするだけ */
    }
};

/* -----------------------------------------------------------------------
 * ポストビルドコンフィグインスタンス (EcuM が Gpt_Init に渡す)
 * ----------------------------------------------------------------------- */
const Gpt_ConfigType Gpt_Config =
{
    Gpt_ChannelTable,
    GPT_CHANNEL_COUNT
};
