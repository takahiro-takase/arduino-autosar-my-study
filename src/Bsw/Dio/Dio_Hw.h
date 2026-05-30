/**
 * \file    Dio_Hw.h
 * \brief   Dio ハードウェア依存層 内部インタフェース
 * \details Dio.c（純粋 C）と Dio_Hw.cpp（Arduino ラッパー）の境界を定義する。
 *          このヘッダは Dio モジュール内部専用であり、上位層から直接参照しない。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef DIO_HW_H
#define DIO_HW_H

#include "Dio.h"

#ifdef __cplusplus
extern "C" {
#endif

void Dio_Hw_WriteChannel(Dio_ChannelType channelId, Dio_LevelType level);

#ifdef __cplusplus
}
#endif

#endif /* DIO_HW_H */
