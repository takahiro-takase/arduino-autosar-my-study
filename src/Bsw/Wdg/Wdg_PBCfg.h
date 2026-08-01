/**
 * \file    Wdg_PBCfg.h
 * \brief   Watchdog Driver ポストビルドコンフィグ 型定義・外部宣言
 * \details Wdg_Init() に渡すコンフィグ構造体の型定義と Wdg_Config
 *          インスタンスの外部宣言を提供する。実際の AUTOSAR 環境では
 *          コンフィギュレーションツールが生成する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef WDG_PBCFG_H
#define WDG_PBCFG_H

#include "Std_Types.h"

/**
 * \brief   Wdg ポストビルドコンフィグ型。
 * \details AUTOSAR の WdgSettingsConfig コンテナ（WDGIF_FAST_MODE 用の
 *          タイムアウト等）の簡略版。本プロジェクトの HW（Renesas RA4M1
 *          IWDT）は単一のタイムアウト設定しか持たないため、フィールドは
 *          DefaultTimeoutMs 1 個のみ。
 */
typedef struct
{
    uint16 DefaultTimeoutMs;  /**< WDGIF_FAST_MODE 選択時に HW へ設定するタイムアウト [ms] */
} Wdg_ConfigType;

/** Wdg_PBCfg.c で定義されるポストビルドコンフィグインスタンス */
extern const Wdg_ConfigType Wdg_Config;

#endif /* WDG_PBCFG_H */
