/**
 * \file    Mcp2515_Wrapper.cpp
 * \brief   MCP2515 C++ ラッパー実装 (mcp_can ライブラリ使用)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include "Mcp2515_Wrapper.h"
#include <mcp_can.h>
#include <new>
#include "Det.h"

/*
 * MCP_CAN は C++ クラスのため、このファイルは .cpp のまま。
 * placement new でヒープを使わず静的バッファ上にインスタンスを構築する。
 * 上位層（Can.c）はこのファイルの内部構造を一切知らない。
 */
static uint8_t  driverBuf[sizeof(MCP_CAN)];
static MCP_CAN* driver = nullptr;

Mcp2515_ReturnType Mcp2515_Init(uint8_t csPin, uint32_t baudrate, uint8_t crystalFreqMhz)
{
    uint8_t mcpClock;
    switch (crystalFreqMhz)
    {
    case 8U:  mcpClock = MCP_8MHZ;  break;
    case 16U: mcpClock = MCP_16MHZ; break;
    case 20U: mcpClock = MCP_20MHZ; break;
    default:
        return MCP2515_WRAPPER_FAIL;
    }

    driver = new (driverBuf) MCP_CAN(csPin);

    return (driver->begin(MCP_STDEXT, baudrate, mcpClock) == CAN_OK)
           ? MCP2515_WRAPPER_OK : MCP2515_WRAPPER_FAIL;
}

Mcp2515_ReturnType Mcp2515_Send(uint32_t id, uint8_t dlc, const uint8_t* data)
{
    INT8U ret;
    ret = driver->sendMsgBuf(id, 0, dlc, (uint8_t*)data);
    if (ret != CAN_OK)
    {
        Serial.print((__FlashStringHelper*)PSTR("[Mcp2515_Send] FAIL to send message"));
        Serial.println();
        return MCP2515_WRAPPER_FAIL;
    }

    Serial.print((__FlashStringHelper*)PSTR("[Mcp2515_Send] OK >>>> ID=0x"));
    Serial.print(id, HEX);
    Serial.print((__FlashStringHelper*)PSTR(" DLC="));
    Serial.print(dlc, DEC);
    Serial.print((__FlashStringHelper*)PSTR(" DATA=["));
    for (uint8_t i = 0; i < dlc; i++)
    {
        if (i > 0)
        {
            Serial.print((__FlashStringHelper*)PSTR(" "));
        }
        if (data[i] < 0x10)
        {
            Serial.print((__FlashStringHelper*)PSTR("0"));
        }
        Serial.print(data[i], HEX);
    }
    Serial.print((__FlashStringHelper*)PSTR("]"));
    Serial.println();

    return MCP2515_WRAPPER_OK;
}

Mcp2515_ReturnType Mcp2515_Read(uint32_t* id, uint8_t* dlc, uint8_t* data)
{
    INT8U ret;
    ret = driver->checkReceive();
    if (ret != CAN_MSGAVAIL)
    {
        Serial.print((__FlashStringHelper*)PSTR("[Mcp2515_Read] No message available"));
        Serial.println();
        return MCP2515_WRAPPER_FAIL;
    }

    long unsigned int rxId = 0;
    unsigned char     len = 0;
    unsigned char     buf[8];
    ret = driver->readMsgBuf(&rxId, &len, buf);
    if (ret != CAN_OK)
    {
        Serial.print((__FlashStringHelper*)PSTR("[Mcp2515_Read] Read message failure"));
        Serial.println();
        return MCP2515_WRAPPER_FAIL;
    }

    *id  = (uint32_t)rxId;
    *dlc = len;
    for (int i = 0; i < len; i++) data[i] = buf[i];

    Serial.print((__FlashStringHelper*)PSTR("[Mcp2515_Read] OK >>>> ID=0x"));
    Serial.print(*id, HEX);
    Serial.print((__FlashStringHelper*)PSTR(" DLC="));
    Serial.print(len, DEC);
    Serial.print((__FlashStringHelper*)PSTR(" DATA=["));
    for (unsigned char i = 0; i < len; i++)
    {
        if (i > 0)
        {
            Serial.print((__FlashStringHelper*)PSTR(" "));
        }
        if (buf[i] < 0x10)
        {
            Serial.print((__FlashStringHelper*)PSTR("0"));
        }
        Serial.print(buf[i], HEX);
    }
    Serial.print((__FlashStringHelper*)PSTR("]"));
    Serial.println();

    return MCP2515_WRAPPER_OK;
}

Mcp2515_ReturnType Mcp2515_InitMask(uint8_t num, uint8_t ext, uint32_t mask)
{
    return (driver->init_Mask(num, ext, mask) == CAN_OK)
           ? MCP2515_WRAPPER_OK : MCP2515_WRAPPER_FAIL;
}

Mcp2515_ReturnType Mcp2515_InitFilter(uint8_t num, uint8_t ext, uint32_t filter)
{
    return (driver->init_Filt(num, ext, filter) == CAN_OK)
           ? MCP2515_WRAPPER_OK : MCP2515_WRAPPER_FAIL;
}

Mcp2515_ReturnType Mcp2515_SetMode(Mcp2515_Mode mode)
{
    uint8_t mcpMode;
    switch (mode)
    {
    case MCP2515_MODE_NORMAL:      mcpMode = MCP_NORMAL;     break;
    case MCP2515_MODE_LISTEN_ONLY: mcpMode = MCP_LISTENONLY; break;
    case MCP2515_MODE_SLEEP:       mcpMode = MCP_SLEEP;      break;
    default:
        return MCP2515_WRAPPER_FAIL;
    }
    return (driver->setMode(mcpMode) == CAN_OK)
           ? MCP2515_WRAPPER_OK : MCP2515_WRAPPER_FAIL;
}

Mcp2515_ReturnType Mcp2515_CheckReceive(void)
{
    return (driver->checkReceive() == CAN_MSGAVAIL)
           ? MCP2515_WRAPPER_OK : MCP2515_WRAPPER_FAIL;
}
