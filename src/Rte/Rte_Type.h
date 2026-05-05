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
typedef uint16 EngineSpeed_t;   // rpm (0-15000)
typedef uint8  CoolantTemp_t;   // ℃  (0-255)
typedef uint8  EngineOnFlag_t;  // 0=OFF, 1=ON (1bit)

#endif
