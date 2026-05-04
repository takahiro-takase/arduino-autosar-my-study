#include "Can.h"
#include <mcp_can.h>
#include <SPI.h>
//#include <HardwareSerial.h>

static const Can_ConfigType* Can_ConfigPtr = nullptr;
//extern MCP_CAN CAN;
static MCP_CAN* CanDriver = nullptr;
static Can_ControllerStateType CanState = CAN_CS_UNINIT;

const int CAN_INT_PIN = 2;

// -----------------------------
// AUTOSAR API：Can_Init(Config)
// CAN ドライバの初期化
// -----------------------------
void Can_Init(const Can_ConfigType* Config)
{
    Serial.println("[Can_Init] Initializing CAN...");

    Can_ConfigPtr = Config;

    CanDriver = new MCP_CAN(Config->csPin);

    // CONFIG モードで初期化
    // CAN.begin(mode, speed, clock) の mode の意味
    // MCP_ANY    : 標準ID/拡張IDを全受信（フィルタ無効になるので注意）
    // MCP_STDEXT : 標準ID/拡張IDを受信（フィルタ有効） ← フィルタ使用時はこれ
    // MCP_STD    : 標準IDのみ受信（フィルタ有効） ← [Can_Init] FAIL
    // MCP_EXT    : 拡張IDのみ受信（フィルタ有効）
    if (CanDriver->begin(MCP_STDEXT, Config->baudrate, MCP_8MHZ) == CAN_OK)
    {
        Serial.println("[Can_Init] CAN Initialized successfully");

        // フィルタ設定
        CanDriver->init_Mask(0, 0, Config->filter.mask << 16);
        CanDriver->init_Filt(0, 0, Config->filter.filterId << 16);

        // NORMAL モードへ
        CanDriver->setMode(MCP_NORMAL);

        CanState = CAN_CS_STARTED;
    }
    else
    {
        Serial.println("[Can_Init] FAIL");
        while(1);
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
        CanDriver->setMode(MCP_NORMAL);
    }
    else if (mode == CAN_CS_STOPPED)
    {
        CanDriver->setMode(MCP_SLEEP);
    }

    CanState = mode;
}

// -----------------------------
// AUTOSAR API：Can_Write()
// -----------------------------
void Can_Write(uint32_t id, uint8_t dlc, const uint8_t* data)
{
    if (CanState != CAN_CS_STARTED) return;

    CanDriver->sendMsgBuf(id, 0, dlc, (uint8_t*)data);

    Serial.print("[Can_Write] Sent ID=0x");
    Serial.print(id, HEX);
    Serial.print(" Data=");
    for (int i = 0; i < dlc; i++) {
        Serial.print(data[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
}

// -----------------------------
// AUTOSAR API：Can_MainFunction_Read()
// 受信ポーリング処理
// -----------------------------
void Can_MainFunction_Read(void)
{
    if (CanState != CAN_CS_STARTED) return;

    if (CanDriver->checkReceive() == CAN_MSGAVAIL)
    {
        long unsigned int rxId;
        unsigned char len;
        unsigned char buf[8];

        CanDriver->readMsgBuf(&rxId, &len, buf);

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
    if (!digitalRead(CAN_INT_PIN)) {
        long unsigned int rxId;
        unsigned char len;
        unsigned char buf[8];

        CanDriver->readMsgBuf(&rxId, &len, buf);

        Serial.print("[Can_Isr] Recv ID=0x");
        Serial.print(rxId, HEX);
        Serial.print(" Data=");

        for (int i = 0; i < len; i++) {
            Serial.print(buf[i], HEX);
            Serial.print(" ");
        }
        Serial.println();
    }
}