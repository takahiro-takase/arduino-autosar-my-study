#ifndef MCP2515_WRAPPER_H
#define MCP2515_WRAPPER_H

#include <stdint.h>

enum class Mcp2515_ReturnType : uint8_t
{
    OK = 0,
    FAIL = 1
};

enum class Mcp2515_Mode : uint8_t
{
    NORMAL      = 0, // 通常動作（TX/RX 可）
    LISTEN_ONLY = 1, // 受信専用（TX 不可）← AUTOSAR CAN_CS_STOPPED に対応
    SLEEP       = 2  // 低電力スリープ     ← AUTOSAR CAN_CS_SLEEP に対応
};

Mcp2515_ReturnType Mcp2515_Init(uint8_t csPin, uint32_t baudrate, uint8_t crystalFreqMhz);
Mcp2515_ReturnType Mcp2515_Send(uint32_t id, uint8_t dlc, const uint8_t* data);
Mcp2515_ReturnType Mcp2515_Read(uint32_t* id, uint8_t* dlc, uint8_t* data);
Mcp2515_ReturnType Mcp2515_InitMask(uint8_t num, uint8_t ext, uint32_t mask);
Mcp2515_ReturnType Mcp2515_InitFilter(uint8_t num, uint8_t ext, uint32_t filter);
Mcp2515_ReturnType Mcp2515_SetMode(Mcp2515_Mode mode);
Mcp2515_ReturnType Mcp2515_CheckReceive(void);

#endif
