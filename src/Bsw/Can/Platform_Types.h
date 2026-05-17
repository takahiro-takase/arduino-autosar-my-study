/**
 * \file    Platform_Types.h
 * \brief   プラットフォーム依存型定義 (AUTOSAR Platform_Types)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef PLATFORM_TYPES_H
#define PLATFORM_TYPES_H

#include <stdint.h>
#include <stddef.h>  /* NULL */

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;

typedef int8_t   sint8;
typedef int16_t  sint16;
typedef int32_t  sint32;

#endif
