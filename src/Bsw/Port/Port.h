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

#define PORT_PIN_IN   0U  /**< 入力方向 */
#define PORT_PIN_OUT  1U  /**< 出力方向 */

/**
 * \brief   Port モジュールを初期化する。
 *
 * \details Port_Cfg.h で定義されたすべてのピンを設定方向に初期化する。
 *          EcuM_Init() の最初期（Dio 操作より前）に呼び出すこと。
 *
 * \pre        Arduino ランタイムが初期化済みであること。
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Port_Init(void);

/**
 * \brief   指定ピンの方向を動的に変更する。
 *
 * \details Port_Init() 後にピン方向を動的変更したい場合に使用する。
 *          通常は Port_Init() 一度で全ピンを設定する。
 *
 * \param[in]  Pin        変更対象のピン番号。
 * \param[in]  Direction  新しいピン方向 (PORT_PIN_IN / PORT_PIN_OUT)。
 *
 * \ServiceID      {0x03}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Port_SetPinDirection(Port_PinType Pin, Port_PinDirectionType Direction);

#ifdef __cplusplus
}
#endif

#endif /* PORT_H */
