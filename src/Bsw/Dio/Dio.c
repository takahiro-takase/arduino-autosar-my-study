/**
 * \file    Dio.c
 * \brief   デジタル入出力 MCAL 実装 (AUTOSAR SWS_Dio 準拠)
 * \details AUTOSAR Dio モジュールの実装。Arduino 依存コードを持たない純粋 C ファイル。
 *          ハードウェア操作は Dio_Hw.cpp（Arduino GPIO ラッパー）へ委譲する。
 *          上位層は本ファイルの存在を知らず、Dio.h の API のみを使用する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "Dio.h"
#include "Dio_Hw.h"

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
void Dio_WriteChannel(Dio_ChannelType channelId, Dio_LevelType level)
{
    Dio_Hw_WriteChannel(channelId, level);
}
