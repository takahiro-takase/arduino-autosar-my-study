#include <Arduino.h>
#include "Can.h"
#include <mcp_can.h>

const int SPI_CS_PIN = 10;
MCP_CAN CAN(SPI_CS_PIN);

const Can_ConfigType CanConfig = {0x123, 0x7FF};

// 送信周期（5秒）
unsigned long sendInterval = 5000;
unsigned long lastSendTime = 0;


// -----------------------------
// Arduino setup()
// -----------------------------
void setup() {
    Serial.begin(115200);

    Can_Init(&CanConfig);       // AUTOSAR 風初期化（フィルタ設定含む）
    Can_SetControllerMode(1);   // AUTOSAR: Normal モード
}

// -----------------------------
// Arduino loop()
// -----------------------------
void loop() {

    // ① 5秒周期で送信（AUTOSAR: Can_Write）
    if (millis() - lastSendTime >= sendInterval) {
        lastSendTime = millis();

        uint8_t data[8] = {0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88};
        Can_Write(0x123, 8, data);
    }

    // ② 割り込みによる受信処理
    Can_Isr();
}
