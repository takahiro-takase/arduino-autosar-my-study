/**
 * \file    Can_Hw.cpp
 * \brief   Can ハードウェア依存層 実装 (MCP2515 / mcp_can ラッパー)
 * \details mcp_can C++ ライブラリをラップし、Can.c（純粋 C）から
 *          呼び出せる C リンケージ関数を提供する。
 *          MCP_CAN クラスは C++ のため、このファイルは .cpp のまま。
 *          placement new でヒープを使わず静的バッファ上にインスタンスを構築する。
 *          上位層（Can.c）はこのファイルの内部構造を一切知らない。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include "Can_Hw.h"
#include <mcp_can.h>
#include <new>
#include "Det.h"

#define TAG "Can_Hw"

static uint8_t  driverBuf[sizeof(MCP_CAN)];
static MCP_CAN* driver = nullptr;

Can_Hw_ReturnType Can_Hw_Init(uint8_t csPin, uint32_t baudrate, uint8_t crystalFreqMhz)
{
    uint8_t mcpClock;
    switch (crystalFreqMhz)
    {
    case 8U:  mcpClock = MCP_8MHZ;  break;
    case 16U: mcpClock = MCP_16MHZ; break;
    case 20U: mcpClock = MCP_20MHZ; break;
    default:
        return CAN_HW_FAIL;
    }

    driver = new (driverBuf) MCP_CAN(csPin);

    return (driver->begin(MCP_STDEXT, baudrate, mcpClock) == CAN_OK)
           ? CAN_HW_OK : CAN_HW_FAIL;
}

Can_Hw_ReturnType Can_Hw_Send(uint32_t id, uint8_t dlc, const uint8_t* data)
{
    INT8U ret;
    ret = driver->sendMsgBuf(id, 0, dlc, (uint8_t*)data);
    if (ret != CAN_OK)
    {
        DET_LOGE(TAG, "TX FAIL id=0x%lX dlc=%u", (unsigned long)id, (unsigned)dlc);
        return CAN_HW_FAIL;
    }

    char hexbuf[25];
    Log_HexStr(hexbuf, sizeof(hexbuf), data, dlc);
    DET_LOGD(TAG, "TX OK id=0x%lX dlc=%u [%s]", (unsigned long)id, (unsigned)dlc, hexbuf);

    return CAN_HW_OK;
}

Can_Hw_ReturnType Can_Hw_Read(uint32_t* id, uint8_t* dlc, uint8_t* data)
{
    INT8U ret;
    ret = driver->checkReceive();
    if (ret != CAN_MSGAVAIL)
    {
        DET_LOGW(TAG, "Read: no msg");
        return CAN_HW_FAIL;
    }

    long unsigned int rxId = 0;
    unsigned char     len = 0;
    unsigned char     buf[8];
    ret = driver->readMsgBuf(&rxId, &len, buf);
    if (ret != CAN_OK)
    {
        DET_LOGE(TAG, "Read: msg read fail");
        return CAN_HW_FAIL;
    }

    *id  = (uint32_t)rxId;
    *dlc = len;
    for (int i = 0; i < len; i++) data[i] = buf[i];

    char hexbuf[25];
    Log_HexStr(hexbuf, sizeof(hexbuf), buf, len);
    DET_LOGD(TAG, "RX OK id=0x%lX dlc=%u [%s]", (unsigned long)*id, (unsigned)len, hexbuf);

    return CAN_HW_OK;
}

Can_Hw_ReturnType Can_Hw_InitMask(uint8_t num, uint8_t ext, uint32_t mask)
{
    return (driver->init_Mask(num, ext, mask) == CAN_OK)
           ? CAN_HW_OK : CAN_HW_FAIL;
}

Can_Hw_ReturnType Can_Hw_InitFilter(uint8_t num, uint8_t ext, uint32_t filter)
{
    return (driver->init_Filt(num, ext, filter) == CAN_OK)
           ? CAN_HW_OK : CAN_HW_FAIL;
}

Can_Hw_ReturnType Can_Hw_SetMode(Can_Hw_Mode mode)
{
    uint8_t mcpMode;
    switch (mode)
    {
    case CAN_HW_MODE_NORMAL:      mcpMode = MCP_NORMAL;     break;
    case CAN_HW_MODE_LISTEN_ONLY: mcpMode = MCP_LISTENONLY; break;
    case CAN_HW_MODE_SLEEP:       mcpMode = MCP_SLEEP;      break;
    case CAN_HW_MODE_LOOPBACK:    mcpMode = MCP_LOOPBACK;   break;
    default:
        return CAN_HW_FAIL;
    }
    return (driver->setMode(mcpMode) == CAN_OK)
           ? CAN_HW_OK : CAN_HW_FAIL;
}

Can_Hw_ReturnType Can_Hw_CheckReceive(void)
{
    return (driver->checkReceive() == CAN_MSGAVAIL)
           ? CAN_HW_OK : CAN_HW_FAIL;
}
