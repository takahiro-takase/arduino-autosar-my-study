/**
 * \file    Gpt_Cfg.h
 * \brief   GPT Driver プリコンパイル設定 (AUTOSAR SWS_Gpt 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef GPT_CFG_H
#define GPT_CFG_H

#include "Std_Types.h"

/* -----------------------------------------------------------------------
 * 基本型定義
 *
 * Gpt_PBCfg.h（ポストビルドコンフィグ）が Gpt_ChannelConfigType のフィールド
 * 型として必要とするため、Gpt.h ではなくここに置く。Gpt.h は
 * Gpt_Cfg.h → Gpt_PBCfg.h の順にインクルードするので、Gpt_PBCfg.h から
 * Gpt.h を逆参照する循環インクルードを避けられる（WdgM_Cfg.h/WdgM_PBCfg.h
 * と同じ構造）。
 * ----------------------------------------------------------------------- */

/** GPT チャネル番号型。実装は Gpt_PBCfg.c の Channels 配列インデックスに
 *  そのまま対応する（[SWS_Gpt_00358]: 実装依存の numeric ID）。 */
typedef uint8 Gpt_ChannelType;

/** タイマ値型（tick 単位）。[SWS_Gpt_00359] */
typedef uint32 Gpt_ValueType;

/** チャネル動作モード。ECUC_Gpt_00309 GptChannelMode */
typedef enum
{
    GPT_CH_MODE_CONTINUOUS = 0U,  /**< 目標時間到達後、0 から再カウントを継続する */
    GPT_CH_MODE_ONESHOT    = 1U   /**< 目標時間到達後、自動的に停止する */
} Gpt_ChannelMode;

/** GptNotification（ECUC_Gpt_00312）関数ポインタ型。
 *  ISR コンテキストから直接呼ばれるため、実装は ISR セーフな処理に限定すること
 *  （Gpt.h 冒頭のコメント参照）。 */
typedef void (*Gpt_NotificationPtrType)(void);

/* -----------------------------------------------------------------------
 * DET（Default Error Tracer）関連定数
 *
 * ModuleId は AUTOSAR_TR_BSWModuleList（Release 4.3.1、docs/ 配下）の
 * 「List of Basic Software Modules」表で GPT Driver (Gpt) に割り当てられた
 * 固定値 100 を使う。
 *
 * 開発エラーコード・ApiId は docs/4.3.1/AUTOSAR_SWS_GPTDriver.pdf
 * (7.4 Error classification 表、8.4 章 各関数の Service ID[hex]) を
 * 実測して確認済み。
 *
 * 以下は未使用（本プロジェクトの対応範囲外のため。Gpt.h 冒頭のコメント参照）:
 *   GPT_E_MODE / GPT_E_PARAM_MODE  : Gpt_SetMode（Sleep/Wakeup 系）用。
 *                                    本プロジェクトの EcuM は SLEEP モードを
 *                                    持たないため GptWakeupFunctionalityApi
 *                                    相当の機能を丸ごと未実装。
 *   GPT_E_PARAM_PREDEF_TIMER       : Gpt_GetPredefTimerValue 用。未実装。
 * ----------------------------------------------------------------------- */

/** AUTOSAR GPT Driver の ModuleId（AUTOSAR_TR_BSWModuleList 参照、固定値 100） */
#define GPT_MODULE_ID  100U

/** 開発エラーコード（7.4.1 Development Errors 表より） */
#define GPT_E_UNINIT               0x0AU  /**< 未初期化時の API 呼び出し */
#define GPT_E_ALREADY_INITIALIZED  0x0DU  /**< 初期化済みでの Gpt_Init 再呼び出し */
#define GPT_E_INIT_FAILED          0x0EU  /**< Gpt_Init の HW 初期化失敗（Gpt_Hw 層参照） */
#define GPT_E_PARAM_CHANNEL        0x14U  /**< Channel が範囲外、または通知未設定チャネルへの Enable/DisableNotification */
#define GPT_E_PARAM_VALUE          0x15U  /**< Value が 0、または許容範囲外 */
#define GPT_E_PARAM_POINTER        0x16U  /**< NULL ポインタ（ConfigPtr/versioninfo） */

/** 実行時エラーコード（7.4.2 Runtime Errors 表より） */
#define GPT_E_BUSY                 0x0BU  /**< running 状態のチャネルへの StartTimer、
                                            *   または running チャネルが残る状態での DeInit */

/** ApiId（各関数の Doxygen \ServiceID タグ、および Gpt_Init への実測値と一致させること。
 *  [SWS_Gpt_00279]〜[00287] の Service ID[hex] と一致。
 *  0x09（Gpt_SetMode）〜0x0d（Gpt_GetPredefTimerValue）は Wakeup/PredefTimer 系のため欠番のまま
 *  （Gpt.h 冒頭のコメント参照）。 */
#define GPT_API_ID_GET_VERSION_INFO      0x00U
#define GPT_API_ID_INIT                  0x01U
#define GPT_API_ID_DE_INIT               0x02U
#define GPT_API_ID_GET_TIME_ELAPSED      0x03U
#define GPT_API_ID_GET_TIME_REMAINING    0x04U
#define GPT_API_ID_START_TIMER           0x05U
#define GPT_API_ID_STOP_TIMER            0x06U
#define GPT_API_ID_ENABLE_NOTIFICATION   0x07U
#define GPT_API_ID_DISABLE_NOTIFICATION  0x08U

/** バージョン情報（Wdg/Mcu 等の既存モジュールと同じ命名規則） */
#define GPT_VENDOR_ID          0U
#define GPT_SW_MAJOR_VERSION   1U
#define GPT_SW_MINOR_VERSION   0U
#define GPT_SW_PATCH_VERSION   0U

#endif /* GPT_CFG_H */
