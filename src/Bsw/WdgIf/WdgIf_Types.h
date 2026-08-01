/**
 * \file    WdgIf_Types.h
 * \brief   Watchdog Interface 型定義 (AUTOSAR SWS_WdgIf 準拠)
 * \details Wdg（下位ドライバ）と WdgIf/WdgM が共通で使う型を 1 か所にまとめる
 *          (MemIf_Types.h / Com_Types.h と同じ「複数モジュールが参照する
 *          共通型は専用ヘッダに分離する」プロジェクトの慣例)。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef WDGIF_TYPES_H
#define WDGIF_TYPES_H

#include "Std_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   ウォッチドッグの動作モード ([SWS_WdgIf_00061] WdgIf_ModeType)。
 * \details Wdg_SetMode()/WdgIf_SetMode() の引数として渡す。
 *          本プロジェクトの Renesas RA4M1 IWDT は単一のタイムアウト設定
 *          しか持たないため、実質的に使用するのは WDGIF_FAST_MODE
 *          （有効化）と WDGIF_OFF_MODE（無効化要求）の 2 値のみ
 *          （詳細は Wdg.c の Wdg_SetMode() 冒頭のコメント参照）。
 *          WDGIF_SLOW_MODE は本プロジェクトが構成しない未使用モードとして
 *          型定義のみ残す（AUTOSAR 仕様に合わせた完全性のため）。
 */
typedef enum
{
    WDGIF_OFF_MODE = 0U,   /**< ウォッチドッグを無効化するモード */
    WDGIF_SLOW_MODE,       /**< 長いタイムアウト周期のモード（本プロジェクト未使用） */
    WDGIF_FAST_MODE        /**< 短いタイムアウト周期のモード。本プロジェクトが唯一使用する有効化モード */
} WdgIf_ModeType;

/**
 * \brief   WdgIf が振り分ける下位ドライバ (Wdg) の識別子。
 * \details 実 AUTOSAR は複数の Wdg インスタンスを構成でき、WdgIf はこの
 *          Device 引数でどのインスタンスへ振り分けるかを決める
 *          ([SWS_WdgIf_00018])。本プロジェクトは物理ウォッチドッグが
 *          1 個のみのため、有効な値は WDGIF_DEVICE_0 のみ
 *          （MemIf_DeviceType と同じ設計。詳細は WdgIf.c 冒頭のコメント参照）。
 */
typedef uint8 WdgIf_DeviceType;

#define WDGIF_DEVICE_0  0U

#ifdef __cplusplus
}
#endif

#endif /* WDGIF_TYPES_H */
