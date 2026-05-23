/**
 * \file    Mcp2515_Wrapper.h
 * \brief   MCP2515 C++ ラッパー 公開インタフェース
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef MCP2515_WRAPPER_H
#define MCP2515_WRAPPER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * enum class → typedef enum に変換。
 * C では名前空間スコープ（::）がないため、
 * 衝突しないようプレフィックスを付ける。
 */
typedef enum
{
    MCP2515_WRAPPER_OK   = 0,
    MCP2515_WRAPPER_FAIL = 1
} Mcp2515_ReturnType;

typedef enum
{
    MCP2515_MODE_NORMAL      = 0, /* 通常動作（TX/RX 可）        */
    MCP2515_MODE_LISTEN_ONLY = 1, /* 受信専用（TX 不可）         */
    MCP2515_MODE_SLEEP       = 2, /* 低電力スリープ              */
    MCP2515_MODE_LOOPBACK    = 3  /* 内部ループバック（テスト用） */
} Mcp2515_Mode;

Mcp2515_ReturnType Mcp2515_Init(uint8_t csPin, uint32_t baudrate, uint8_t crystalFreqMhz);
Mcp2515_ReturnType Mcp2515_Send(uint32_t id, uint8_t dlc, const uint8_t* data);
Mcp2515_ReturnType Mcp2515_Read(uint32_t* id, uint8_t* dlc, uint8_t* data);
Mcp2515_ReturnType Mcp2515_InitMask(uint8_t num, uint8_t ext, uint32_t mask);
Mcp2515_ReturnType Mcp2515_InitFilter(uint8_t num, uint8_t ext, uint32_t filter);
Mcp2515_ReturnType Mcp2515_SetMode(Mcp2515_Mode mode);
Mcp2515_ReturnType Mcp2515_CheckReceive(void);

#ifdef __cplusplus
}
#endif

#endif
