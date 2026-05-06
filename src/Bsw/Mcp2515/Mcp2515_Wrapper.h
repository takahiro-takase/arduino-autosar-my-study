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
    MCP2515_MODE_NORMAL      = 0, /* 通常動作（TX/RX 可）  */
    MCP2515_MODE_LISTEN_ONLY = 1, /* 受信専用（TX 不可）   */
    MCP2515_MODE_SLEEP       = 2  /* 低電力スリープ        */
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
