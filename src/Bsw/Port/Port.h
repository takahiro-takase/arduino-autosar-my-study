/**
 * \file    Port.h
 * \brief   ポートドライバ 公開インタフェース (AUTOSAR SWS_Port 準拠)
 * \details MCAL ポートドライバ API を提供する。
 *          Dio モジュールがピン値の読み書きを担うのに対し、
 *          Port モジュールはピンの方向（INPUT / OUTPUT）設定を担う。
 *
 *          AUTOSAR における責務分担:
 *            Port — ピン方向・初期値・モードの設定
 *            Dio  — ピン値の読み書き（方向設定は Port が担う）
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef PORT_H
#define PORT_H

#include "Std_Types.h"
#include "Port_Cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/** ピン番号型（Arduino ピン番号に対応）*/
typedef uint8 Port_PinType;

/** ピン方向型 */
typedef uint8 Port_PinDirectionType;

#define PORT_PIN_IN          0U  /**< 入力方向 (フローティング) */
#define PORT_PIN_OUT         1U  /**< 出力方向 */
#define PORT_PIN_IN_PULLUP   2U  /**< 入力方向（内部プルアップ有効）ボタン等に使用 */

/**
 * \brief   Port_Init() の設定引数型（不透明型）。
 *
 * \details SWS_Port_00140 は post-build 設定データ（各ピンの方向等）への
 *          ポインタを要求する。本プロジェクトはそのデータを `Port_Cfg.h` の
 *          静的テーブルとして直接参照する簡略設計のため（注入されたポインタ
 *          経由では読まない）、中身を定義しない不透明型とし、ポインタとしてのみ
 *          扱う（`KeyM_ConfigType` と同じパターン。KeyM.h 冒頭コメント参照）。
 */
typedef struct Port_ConfigType_Tag Port_ConfigType;

/**
 * \brief   Port モジュールを初期化する。
 *
 * \details Port_Cfg.h で定義されたすべてのピンを設定方向に初期化する。
 *          EcuM_Init() の最初期（Dio 操作より前）に呼び出すこと。
 *
 * \param[in]  ConfigPtr  常に NULL を渡すこと（本プロジェクトは Port_Cfg.h の
 *                        静的テーブルを直接参照するため、注入された設定は
 *                        使わない）。
 *
 * \pre        Arduino ランタイムが初期化済みであること。
 *
 * \AUTOSARReq     {SWS_Port_00140}
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Port_Init(const Port_ConfigType* ConfigPtr);

/**
 * \brief   指定ピンの方向を動的に変更する。
 *
 * \details Port_Init() 後にピン方向を動的変更したい場合に使用する。
 *          通常は Port_Init() 一度で全ピンを設定する。
 *
 * \param[in]  Pin        変更対象のピン番号。
 * \param[in]  Direction  新しいピン方向 (PORT_PIN_IN / PORT_PIN_OUT)。
 *
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Port_SetPinDirection(Port_PinType Pin, Port_PinDirectionType Direction);

/**
 * \brief   Port ドライバのバージョン情報を取得する。
 *
 * \details 他 BSW モジュールと共通の慣例により、未初期化時でもエラー報告
 *          しない例外 API のため、初期化状態は確認せず NULL ポインタ
 *          チェックのみ行う。
 *
 * \param[out]  versioninfo  バージョン情報の格納先。NULL 禁止。
 *
 * \ServiceID      {0x03}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Port_GetVersionInfo(Std_VersionInfoType* versioninfo);

#ifdef __cplusplus
}
#endif

#endif /* PORT_H */
