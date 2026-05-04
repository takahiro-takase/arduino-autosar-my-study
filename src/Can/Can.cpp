#include <SPI.h>
#include "Can.h"
#include "Mcp2515_Wrapper.h"

static const Can_ConfigType* Can_ConfigPtr = nullptr;
static Can_ControllerStateType CanState = CAN_CS_UNINIT;

// -----------------------------
// AUTOSAR API：Can_Init(Config)
// CAN ドライバの初期化
// -----------------------------
void Can_Init(const Can_ConfigType* Config)
{
    Serial.println("[Can_Init] Initializing CAN...");

    Can_ConfigPtr = Config;

    // CONFIG モードで初期化
    // CAN.begin(mode, speed, clock) の mode の意味
    // MCP_ANY    : 標準ID/拡張IDを全受信（フィルタ無効になるので注意）
    // MCP_STDEXT : 標準ID/拡張IDを受信（フィルタ有効） ← フィルタ使用時はこれ
    // MCP_STD    : 標準IDのみ受信（フィルタ有効） ← [Can_Init] FAIL
    // MCP_EXT    : 拡張IDのみ受信（フィルタ有効）
    if (Mcp2515_Init(Config->csPin, Config->baudrate) == Mcp2515_ReturnType::OK)
    {
        Serial.println("[Can_Init] CAN Initialized successfully");

        // フィルタ設定
        Mcp2515_InitMask(0, 0, Config->filter.mask << 16);
        Mcp2515_InitFilter(0, 0, Config->filter.filterId << 16);
        Mcp2515_InitFilter(1, 0, Config->filter.filterId << 16);
        Mcp2515_InitMask(1, 0, Config->filter.mask << 16);
        Mcp2515_InitFilter(2, 0, Config->filter.filterId << 16);
        Mcp2515_InitFilter(3, 0, Config->filter.filterId << 16);
        Mcp2515_InitFilter(4, 0, Config->filter.filterId << 16);
        Mcp2515_InitFilter(5, 0, Config->filter.filterId << 16);

        // NORMAL モードへ
        Mcp2515_SetMode(Mcp2515_Mode::NORMAL);

        CanState = CAN_CS_STARTED;
    }
    else
    {
        Serial.println("[Can_Init] FAIL");
        while (1)
            ;
    }
}

// -----------------------------
// AUTOSAR API：Can_SetControllerMode()
// Normal / Sleep / Stop の切替
// -----------------------------
void Can_SetControllerMode(Can_ControllerStateType mode)
{
    if (mode == CAN_CS_STARTED)
    {
        Mcp2515_SetMode(Mcp2515_Mode::NORMAL);
    }
    else if (mode == CAN_CS_STOPPED)
    {
        Mcp2515_SetMode(Mcp2515_Mode::SLEEP);
    }

    CanState = mode;
}

// -----------------------------
// AUTOSAR API：Can_Write()
// -----------------------------
Can_ReturnType Can_Write(uint32_t id, uint8_t dlc, const uint8_t* data)
{
    if (CanState != CAN_CS_STARTED)
        return Can_ReturnType::CAN_NOT_OK;

    Mcp2515_Send(id, dlc, data);

    Serial.print("[Can_Write] Sent ID=0x");
    Serial.print(id, HEX);
    Serial.print(" Data=");
    for (int i = 0; i < dlc; i++)
    {
        Serial.print(data[i], HEX);
        Serial.print(" ");
    }
    Serial.println();

    return Can_ReturnType::CAN_OK;
}

// -----------------------------
// AUTOSAR API：Can_MainFunction_Read()
// 受信ポーリング処理
// -----------------------------
void Can_MainFunction_Read(void)
{
    if (CanState != CAN_CS_STARTED)
        return;

    if (Mcp2515_CheckReceive() == Mcp2515_ReturnType::OK)
    {
        uint32_t rxId;
        uint8_t len;
        uint8_t buf[8];
        Mcp2515_Read(&rxId, &len, buf);

        // AUTOSAR ならここで CanIf_RxIndication() を呼ぶ
        Serial.print("[RX] ID=0x");
        Serial.println(rxId, HEX);
    }
}

// -----------------------------
// AUTOSAR API：Can_Isr()
// （受信割り込み処理）
// -----------------------------
void Can_Isr(void)
{
    if (!digitalRead(2)) // MCP2515の割り込みピン（INT）を接続しているピンを読み取る
    {
        uint32_t rxId;
        uint8_t len;
        uint8_t buf[8];
        Mcp2515_Read(&rxId, &len, buf);

        Serial.print("[Can_Isr] Recv ID=0x");
        Serial.print(rxId, HEX);
        Serial.print(" Data=");

        for (int i = 0; i < len; i++)
        {
            Serial.print(buf[i], HEX);
            Serial.print(" ");
        }
        Serial.println();
    }
}