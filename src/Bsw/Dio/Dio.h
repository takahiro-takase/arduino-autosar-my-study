/**
 * \file    Dio.h
 * \brief   デジタル入出力 公開インタフェース (AUTOSAR SWS_Dio 準拠)
 * \details MCAL 層のデジタル I/O 抽象化 API を提供する。
 *          ピン値の読み書き（DIO_HIGH / DIO_LOW）のみを担当する。
 *          ピン方向（INPUT / OUTPUT）の設定は Port モジュールの責務であり、
 *          Port_Init() が事前に完了していることを前提とする。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef DIO_H
#define DIO_H

#include "Std_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** チャネル ID 型 (Arduino ピン番号に対応) */
typedef uint8 Dio_ChannelType;

/** チャネル出力レベル型 */
typedef uint8 Dio_LevelType;

#define DIO_HIGH  1U  /**< 出力 HIGH (3.3V / 5V) */
#define DIO_LOW   0U  /**< 出力 LOW  (GND) */

/**
 * \brief   指定チャネルへ出力レベルを書き込む。
 *
 * \param[in]  channelId  書き込み先チャネル ID (Arduino ピン番号)。
 * \param[in]  level      出力レベル (DIO_HIGH / DIO_LOW)。
 *
 * \pre        Port_Init() で対象チャネルを出力モードに設定済みであること。
 *
 * \ServiceID      {0xE1}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Dio_WriteChannel(Dio_ChannelType channelId, Dio_LevelType level);

/**
 * \brief   指定チャネルの入力レベルを読み取る。
 *
 * \param[in]  channelId  読み取り元チャネル ID (Arduino ピン番号)。
 *
 * \return  DIO_HIGH または DIO_LOW。
 *
 * \pre        Port_Init() で対象チャネルを入力モード (PORT_PIN_IN / PORT_PIN_IN_PULLUP)
 *             に設定済みであること。
 *
 * \ServiceID      {0xE2}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Dio_LevelType Dio_ReadChannel(Dio_ChannelType channelId);

#ifdef __cplusplus
}
#endif

#endif /* DIO_H */
