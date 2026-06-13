/**
 * \file    Rte_Type.h
 * \brief   RTE アプリケーション型定義 (AUTOSAR SWS_RTE 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef RTE_TYPE_H
#define RTE_TYPE_H

#include "Platform_Types.h"

// -------------------------------------------------------
// アプリケーション Signal の型エイリアス
//
// AUTOSAR では ARXML から自動生成される。
// SW-C は COM_SIGNAL_* の ID や uint16/uint8 の生の型を知らず、
// これらのアプリ型だけを使う。
// -------------------------------------------------------
typedef uint16 EngineSpeed_t;    /* rpm (0-15000) */
typedef uint8  CoolantTemp_t;    /* ℃  (0-255) */
typedef uint8  EngineOnFlag_t;   /* 0=OFF, 1=ON (1bit) */

/* ABS ECU シグナル型（ARXML で定義する ApplicationDataType に相当）*/
typedef uint16 VehicleSpeed_t;   /* 0.01 km/h (0-655.35 km/h) */
typedef uint8  BrakeActive_t;    /* 0=解除, 1=作動 (1bit) */
typedef uint8  AbsActive_t;      /* 0=非作動, 1=ABS 作動中 (1bit) */

/* ローカルボタン入力型（IoHwAb 経由で読み取る GPIO 入力）*/
typedef uint8  ButtonState_t;    /* 0=解放, 1=押下 */

typedef enum
{
    ENGINE_STATE_OFF      = 0,
    ENGINE_STATE_STARTING = 1,
    ENGINE_STATE_RUNNING  = 2,
    ENGINE_STATE_FAULT    = 3
} EngineState_t;

#endif
