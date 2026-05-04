#include <Arduino.h>
#include "Can.h"
#include <mcp_can_dfs.h>

// 送信周期（5秒）
unsigned long sendInterval = 5000;
unsigned long lastSendTime = 0;

const Can_ConfigType CanConfig = {
    .filter = {0x123, 0x7FF},
    .csPin = 10,
    .baudrate = CAN_500KBPS
};

// -----------------------------
// Arduino setup()
// -----------------------------
void setup()
{
    Serial.begin(115200);

    // AUTOSAR 風初期化（フィルタ設定含む）
    Can_Init(&CanConfig);

    // AUTOSAR: Normal モード
    Can_SetControllerMode(CAN_CS_STARTED);
}

// -----------------------------
// Arduino loop()
// -----------------------------
void loop()
{

    // ① 5秒周期で送信（AUTOSAR: Can_Write）
    if (millis() - lastSendTime >= sendInterval) {
        lastSendTime = millis();

        uint8_t data[8] = {0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88};
        (void)Can_Write(0x123, 8, data);
    }

    // ② 割り込みによる受信処理
    Can_Isr();
}
