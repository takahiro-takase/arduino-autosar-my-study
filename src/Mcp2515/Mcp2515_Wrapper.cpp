#include "Mcp2515_Wrapper.h"
#include <mcp_can.h>

static MCP_CAN* driver = nullptr;

Mcp2515_ReturnType Mcp2515_Init(uint8_t csPin, uint32_t baudrate)
{
    driver = new MCP_CAN(csPin);

    if (driver->begin(MCP_STDEXT, baudrate, MCP_8MHZ) == CAN_OK)
    {
        driver->setMode(MCP_NORMAL);
        return Mcp2515_ReturnType::OK;
    }
    return Mcp2515_ReturnType::FAIL;
}

Mcp2515_ReturnType Mcp2515_Send(uint32_t id, uint8_t dlc, const uint8_t* data)
{
    return (driver->sendMsgBuf(id, 0, dlc, (uint8_t*)data) == CAN_OK)
           ? Mcp2515_ReturnType::OK : Mcp2515_ReturnType::FAIL;
}

Mcp2515_ReturnType Mcp2515_Read(uint32_t* id, uint8_t* dlc, uint8_t* data)
{
    if (driver->checkReceive() == CAN_MSGAVAIL)
    {
        long unsigned int rxId;
        unsigned char len;
        unsigned char buf[8];
        driver->readMsgBuf(&rxId, &len, buf);
        *id = rxId;
        *dlc = len;
        for (int i = 0; i < len; i++)
        {
            data[i] = buf[i];
        }
        return Mcp2515_ReturnType::OK;
    }
    return Mcp2515_ReturnType::FAIL;
}

// -----------------------------
// Mask 設定ラッパー
// -----------------------------
Mcp2515_ReturnType Mcp2515_InitMask(uint8_t num, uint8_t ext, uint32_t mask)
{
    return (driver->init_Mask(num, ext, mask) == CAN_OK)
           ? Mcp2515_ReturnType::OK : Mcp2515_ReturnType::FAIL;
}

// -----------------------------
// Filter 設定ラッパー
// -----------------------------
Mcp2515_ReturnType Mcp2515_InitFilter(uint8_t num, uint8_t ext, uint32_t filter)
{
    return (driver->init_Filt(num, ext, filter) == CAN_OK)
           ? Mcp2515_ReturnType::OK : Mcp2515_ReturnType::FAIL;
}

Mcp2515_ReturnType Mcp2515_SetMode(Mcp2515_Mode mode)
{
    uint8_t mcpMode;
    switch (mode)
    {
    case Mcp2515_Mode::NORMAL:
        mcpMode = MCP_NORMAL;
        break;
    case Mcp2515_Mode::SLEEP:
        mcpMode = MCP_SLEEP;
        break;
    default:
        break;
    }
    return (driver->setMode(mcpMode) == CAN_OK)
           ? Mcp2515_ReturnType::OK : Mcp2515_ReturnType::FAIL;
}

Mcp2515_ReturnType Mcp2515_CheckReceive(void)
{
    return (driver->checkReceive() == CAN_MSGAVAIL)
           ? Mcp2515_ReturnType::OK : Mcp2515_ReturnType::FAIL;
}