/**
 * \file    Std_Types.h
 * \brief   標準型定義 (AUTOSAR Std_Types)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef STD_TYPES_H
#define STD_TYPES_H

#include "Platform_Types.h"

// AUTOSAR SWS_Std_00005
typedef uint8 Std_ReturnType;

#define E_OK     ((Std_ReturnType)0x00U)
#define E_NOT_OK ((Std_ReturnType)0x01U)

#endif
