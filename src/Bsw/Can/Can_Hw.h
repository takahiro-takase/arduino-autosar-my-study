/**
 * \file    Can_Hw.h
 * \brief   Can ハードウェア依存層 内部インタフェース (MCP2515 向け)
 * \details Can.c（純粋 C）と Can_Hw.cpp（mcp_can C++ ラッパー）の境界を定義する。
 *          このヘッダは Can モジュール内部専用であり、上位層から直接参照しない。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef CAN_HW_H
#define CAN_HW_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    CAN_HW_OK   = 0,
    CAN_HW_FAIL = 1
} Can_Hw_ReturnType;

typedef enum
{
    CAN_HW_MODE_NORMAL      = 0, /* 通常動作（TX/RX 可）        */
    CAN_HW_MODE_LISTEN_ONLY = 1, /* 受信専用（TX 不可）         */
    CAN_HW_MODE_SLEEP       = 2, /* 低電力スリープ              */
    CAN_HW_MODE_LOOPBACK    = 3  /* 内部ループバック（テスト用） */
} Can_Hw_Mode;

Can_Hw_ReturnType Can_Hw_Init(uint8_t csPin, uint32_t baudrate, uint8_t crystalFreqMhz);
Can_Hw_ReturnType Can_Hw_Send(uint32_t id, uint8_t dlc, const uint8_t* data);
Can_Hw_ReturnType Can_Hw_Read(uint32_t* id, uint8_t* dlc, uint8_t* data);
Can_Hw_ReturnType Can_Hw_InitMask(uint8_t num, uint8_t ext, uint32_t mask);
Can_Hw_ReturnType Can_Hw_InitFilter(uint8_t num, uint8_t ext, uint32_t filter);
Can_Hw_ReturnType Can_Hw_SetMode(Can_Hw_Mode mode);
Can_Hw_ReturnType Can_Hw_CheckReceive(void);
Can_Hw_ReturnType Can_Hw_IsBusOff(void);

#ifdef __cplusplus
}
#endif

#endif /* CAN_HW_H */
