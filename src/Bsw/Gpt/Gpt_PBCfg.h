/**
 * \file    Gpt_PBCfg.h
 * \brief   GPT Driver ポストビルドコンフィグ 型定義・外部宣言
 * \details Gpt_Init() に渡すコンフィグ構造体の型定義と
 *          Gpt_Config インスタンスの外部宣言を提供する。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツールが生成する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef GPT_PBCFG_H
#define GPT_PBCFG_H

#include "Std_Types.h"
#include "Gpt_Cfg.h"

/** 管理チャネル総数 (Gpt_PBCfg.c の Gpt_ChannelTable 要素数と一致させること) */
#define GPT_CHANNEL_COUNT  1U

/** チャネル ID（Gpt_PBCfg.c の Gpt_ChannelTable 配列インデックスと一致させること） */
#define GPT_CHANNEL_0  0U

/**
 * \brief   GPT チャネル 1 本の設定（AUTOSAR GptChannelConfiguration コンテナ相当）。
 */
typedef struct
{
    Gpt_ChannelType          ChannelId;        /**< GptChannelId（配列インデックスと一致させること） */
    Gpt_ChannelMode          Mode;              /**< GptChannelMode */
    uint32                   TickFrequencyHz;   /**< GptChannelTickFrequency [Hz] */
    Gpt_ValueType            TickValueMax;      /**< GptChannelTickValueMax */
    Gpt_NotificationPtrType  Notification;      /**< GptNotification（0..1。未使用チャネルは NULL） */
} Gpt_ChannelConfigType;

/**
 * \brief   GPT ポストビルドコンフィグ型。Gpt_Init() に渡す最上位構造体。
 */
typedef struct
{
    const Gpt_ChannelConfigType* Channels;      /**< チャネル設定配列の先頭 */
    uint8                        ChannelCount;  /**< チャネル数 */
} Gpt_ConfigType;

/** Gpt_PBCfg.c で定義されるポストビルドコンフィグインスタンス */
extern const Gpt_ConfigType Gpt_Config;

#endif /* GPT_PBCFG_H */
