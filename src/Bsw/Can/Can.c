#include "Can.h"
#include "Mcp2515_Wrapper.h"
#include "CanIf.h"
#include "Det.h"

/* Arduino wiring.c (C linkage) */
extern int digitalRead(uint8 pin);

static const Can_ConfigType*   Can_ConfigPtr = NULL;
static Can_ControllerStateType CanState      = CAN_CS_UNINIT;

void Can_Init(const Can_ConfigType* Config)
{
    Det_LogP(PSTR("[Can_Init] Initializing CAN..."));

    Can_ConfigPtr = Config;

    if (Mcp2515_Init(Config->csPin, Config->baudrate, Config->crystalFreq) != MCP2515_WRAPPER_OK)
    {
        Det_LogP(PSTR("[Can_Init] FAIL"));
        while (1)
            ;
    }

    Det_LogP(PSTR("[Can_Init] CAN Initialized successfully"));

    Mcp2515_InitMask(0, 0, Config->filter.mask << 16);
    Mcp2515_InitFilter(0, 0, Config->filter.filterId << 16);
    Mcp2515_InitFilter(1, 0, Config->filter.filterId << 16);
    Mcp2515_InitMask(1, 0, Config->filter.mask << 16);
    Mcp2515_InitFilter(2, 0, Config->filter.filterId << 16);
    Mcp2515_InitFilter(3, 0, Config->filter.filterId << 16);
    Mcp2515_InitFilter(4, 0, Config->filter.filterId << 16);
    Mcp2515_InitFilter(5, 0, Config->filter.filterId << 16);

    Mcp2515_SetMode(MCP2515_MODE_LISTEN_ONLY);
    CanState = CAN_CS_STOPPED;
}

void Can_SetControllerMode(Can_ControllerStateType mode)
{
    switch (mode)
    {
    case CAN_CS_STARTED:
        Mcp2515_SetMode(MCP2515_MODE_NORMAL);
        break;
    case CAN_CS_STOPPED:
        Mcp2515_SetMode(MCP2515_MODE_LISTEN_ONLY);
        break;
    case CAN_CS_SLEEP:
        Mcp2515_SetMode(MCP2515_MODE_SLEEP);
        break;
    default:
        return;
    }

    CanState = mode;
}

Can_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType* PduInfo)
{
    (void)Hth;

    if (CanState != CAN_CS_STARTED)
        return CAN_NOT_OK;

    if (Mcp2515_Send(PduInfo->id, PduInfo->length, PduInfo->sdu) != MCP2515_WRAPPER_OK)
        return CAN_NOT_OK;

    Det_PrintP(PSTR("[Can_Write] Sent ID=0x"));
    Det_PrintHex(PduInfo->id);
    Det_PrintP(PSTR(" Data="));
    for (int i = 0; i < PduInfo->length; i++)
    {
        Det_PrintHex(PduInfo->sdu[i]);
        Det_PrintP(PSTR(" "));
    }
    Det_Newline();

    CanIf_TxConfirmation(PduInfo->swPduHandle);

    return CAN_OK;
}

void Can_MainFunction_Read(void)
{
    if (CanState != CAN_CS_STARTED)
        return;

    if (Mcp2515_CheckReceive() == MCP2515_WRAPPER_OK)
    {
        uint32 rxId;
        uint8  len;
        uint8  buf[8];

        if (Mcp2515_Read(&rxId, &len, buf) == MCP2515_WRAPPER_OK)
        {
            CanIf_RxIndication(0, rxId, len, buf);
        }
    }
}

void Can_Isr(void)
{
    if (Can_ConfigPtr == NULL)
        return;

    if (!digitalRead(Can_ConfigPtr->intPin))
    {
        uint32 rxId;
        uint8  len;
        uint8  buf[8];

        if (Mcp2515_Read(&rxId, &len, buf) == MCP2515_WRAPPER_OK)
        {
            CanIf_RxIndication(0, rxId, len, buf);
        }
    }
}
