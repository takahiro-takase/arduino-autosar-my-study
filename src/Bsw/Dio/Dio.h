/**
 * \file    Dio.h
 * \brief   デジタル入出力 公開インタフェース (AUTOSAR SWS_Dio 準拠)
 * \details MCAL 層のデジタル I/O 抽象化 API を提供する。
 *          Arduino の pinMode / digitalWrite を直接呼び出す代わりに
 *          本インタフェースを経由することで、上位層をハードウェア依存から分離する。
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
 * \brief   指定チャネルを出力モードに設定する。
 *
 * \param[in]  channelId  初期化するチャネル ID (Arduino ピン番号)。
 *
 * \ServiceID      {0xE0}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Dio_InitChannel(Dio_ChannelType channelId);

/**
 * \brief   指定チャネルへ出力レベルを書き込む。
 *
 * \param[in]  channelId  書き込み先チャネル ID (Arduino ピン番号)。
 * \param[in]  level      出力レベル (DIO_HIGH / DIO_LOW)。
 *
 * \pre        Dio_InitChannel() で対象チャネルを出力モードに設定済みであること。
 *
 * \ServiceID      {0xE1}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Dio_WriteChannel(Dio_ChannelType channelId, Dio_LevelType level);

#ifdef __cplusplus
}
#endif

#endif /* DIO_H */
