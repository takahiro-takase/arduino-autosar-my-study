#include "Can.h"
#include <mcp_can.h>
#include <SPI.h>
#include <HardwareSerial.h>

extern MCP_CAN CAN;

const int CAN_INT_PIN = 2;

// -----------------------------
// AUTOSAR API：Can_Init(Config)
// CAN ドライバの初期化
// -----------------------------
void Can_Init(const Can_ConfigType* Config)
{
    Serial.println("[Can_Init] Initializing CAN...");

    // CAN.begin(mode, speed, clock) の mode の意味
    // MCP_ANY    : 標準ID/拡張IDを全受信（フィルタ無効になるので注意）
    // MCP_STDEXT : 標準ID/拡張IDを受信（フィルタ有効） ← フィルタ使用時はこれ
    // MCP_STD    : 標準IDのみ受信（フィルタ有効） ← [Can_Init] FAIL
    // MCP_EXT    : 拡張IDのみ受信（フィルタ有効）
    if (CAN.begin(MCP_STDEXT, CAN_500KBPS, MCP_8MHZ) != CAN_OK) {
        Serial.println("[Can_Init] FAIL");
        while (1);
    }

    // -----------------------------
    // Day5：フィルタ設定（AUTOSAR の CanHwFilter）
    // -----------------------------
    uint32_t shiftedId = (uint32_t)Config->filterId << 16;
    uint32_t shiftedMask = (uint32_t)Config->mask << 16;

    CAN.init_Mask(0, 0, shiftedMask);   // Mask0
    CAN.init_Filt(0, 0, shiftedId);  // Filter0
    CAN.init_Filt(1, 0, shiftedId);  // Filter1
    
    CAN.init_Mask(1, 0, shiftedMask);   // Mask1
    CAN.init_Filt(2, 0, shiftedId);  // Filter2
    CAN.init_Filt(3, 0, shiftedId);  // Filter3
    CAN.init_Filt(4, 0, shiftedId);  // Filter4
    CAN.init_Filt(5, 0, shiftedId);  // Filter5

    Serial.println("[Can_Init] Filter set: ID=0x123 only");

    // CAN.setMode(MCP_NORMAL)
    // begin() の後に NORMAL を再設定しても問題なし（NORMAL→NORMAL）
    // CONFIG モードでフィルタ設定した後に NORMAL に戻す用途でも使える
    CAN.setMode(MCP_NORMAL);
}

// -----------------------------
// AUTOSAR API：Can_SetControllerMode()
// Normal / Sleep / Stop の切替
// -----------------------------
void Can_SetControllerMode(uint8_t mode)
{
    if (mode == 1) {  // 1 = Normal
        CAN.setMode(MCP_NORMAL);
        Serial.println("[Can_SetControllerMode] NORMAL mode");
    }
    else {
        Serial.println("[Can_SetControllerMode] Unsupported mode");
    }
}

// -----------------------------
// AUTOSAR API：Can_Write()
// -----------------------------
void Can_Write(uint32_t id, uint8_t dlc, uint8_t* data)
{
    CAN.sendMsgBuf(id, 0, dlc, data);

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
    if (!digitalRead(CAN_INT_PIN)) {  // INT が LOW → 受信あり
        long unsigned int rxId;
        unsigned char len = 0;
        unsigned char buf[8];

        CAN.readMsgBuf(&rxId, &len, buf);

        Serial.print("[Can_MainFunction_Read] Recv ID=0x");
        Serial.print(rxId, HEX);
        Serial.print(" Data=");

        for (int i = 0; i < len; i++) {
            Serial.print(buf[i], HEX);
            Serial.print(" ");
        }
        Serial.println();
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

        CAN.readMsgBuf(&rxId, &len, buf);

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