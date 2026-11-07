// MCP2518FD の SPI 応答を、生の値で確かめる診断用スケッチ(ライブラリは使わない)。
//
// 見ること:
//   1) MISO(D12)の待機中のレベル
//   2) リセット後の CiCON(0x000)。正常なら 60 07 98 04(コンフィギュレーションモード)
//   3) OSC レジスタ(0xE00)。水晶が動いていれば 2 バイト目に 0x04(OSCRDY)が立つ
//   4) RAM(0x400)への書き込みと読み戻し
//
// 配線(UNO R4 → 基板 J3):
//   D13 → SCK / D11 → SI(MOSI) / D12 → SO(MISO) / D10 → CS / D2 → INT / 5V / GND
//   MCP2515 のモジュールと同じピン。MCP2515 は外しておくこと(同じ SPI の線を共有するため)。

#include <Arduino.h>
#include <SPI.h>

static const byte CS_PIN = 10;

static SPISettings spiSettings(800UL * 1000UL, MSBFIRST, SPI_MODE0);

static void csLow()  { digitalWrite(CS_PIN, LOW); }
static void csHigh() { digitalWrite(CS_PIN, HIGH); }

static void chipReset()
{
    SPI.beginTransaction(spiSettings);
    csLow();
    SPI.transfer(0x00);
    SPI.transfer(0x00);
    csHigh();
    SPI.endTransaction();
    delay(5);
}

static void readBytes(uint16_t addr, uint8_t* buf, uint8_t n)
{
    SPI.beginTransaction(spiSettings);
    csLow();
    SPI.transfer((uint8_t)(0x30 | ((addr >> 8) & 0x0F)));
    SPI.transfer((uint8_t)(addr & 0xFF));
    for (uint8_t i = 0; i < n; i++) { buf[i] = SPI.transfer(0x00); }
    csHigh();
    SPI.endTransaction();
}

static void writeBytes(uint16_t addr, const uint8_t* buf, uint8_t n)
{
    SPI.beginTransaction(spiSettings);
    csLow();
    SPI.transfer((uint8_t)(0x20 | ((addr >> 8) & 0x0F)));
    SPI.transfer((uint8_t)(addr & 0xFF));
    for (uint8_t i = 0; i < n; i++) { SPI.transfer(buf[i]); }
    csHigh();
    SPI.endTransaction();
}

static void printHex(const char* label, const uint8_t* b, uint8_t n)
{
    Serial.print(label);
    for (uint8_t i = 0; i < n; i++)
    {
        if (b[i] < 0x10) { Serial.print('0'); }
        Serial.print(b[i], HEX);
        Serial.print(' ');
    }
    Serial.println();
}

void setup()
{
    pinMode(CS_PIN, OUTPUT);
    csHigh();

    Serial.begin(115200);
    const uint32_t t0 = millis();
    while (!Serial && ((millis() - t0) < 3000U)) { }
    Serial.println();
    Serial.println("=== MCP2518FD SPI 診断 ===");

    // 1) MISO(D12)のレベル。CS=HIGH の間は、チップは MISO を駆動しない(ハイインピーダンス)。
    //    内部プルアップを有効にして読むと、駆動されていなければ HIGH になる。
    //    LOW のままなら、線が GND に落ちている(配線・はんだ・他の部品)。
    pinMode(12, INPUT);
    Serial.print("MISO(D12) 待機レベル(プルなし): ");
    Serial.println(digitalRead(12) ? "HIGH" : "LOW");
    pinMode(12, INPUT_PULLUP);
    delay(1);
    Serial.print("MISO(D12) 待機レベル(プルアップ): ");
    Serial.println(digitalRead(12) ? "HIGH (正常: 線は GND に落ちていない)" : "LOW (異常: 線が GND に落ちているか、何かが LOW を出している)");

    // CS を LOW にして、チップが MISO を駆動し始めるかを見る(ただし SPI クロックは出さない)
    csLow();
    delay(1);
    Serial.print("MISO(D12) CS=LOW のとき(プルアップ): ");
    Serial.println(digitalRead(12) ? "HIGH" : "LOW");
    csHigh();

    // 注意: SPI.begin() は、上のピン操作の「あと」に呼ぶ。
    // (先に呼んだあとで pinMode(12, ...) をすると、D12 が SPI の MISO ではなくなる)
    SPI.begin();

    chipReset();

    // 2) CiCON
    uint8_t b[4] = {0, 0, 0, 0};
    readBytes(0x000, b, 4);
    printHex("CiCON(0x000) 読み: ", b, 4);
    Serial.println("  期待値: 60 07 98 04 (全部 00 か FF なら、チップが SPI に応答していない)");

    // 3) OSC
    readBytes(0xE00, b, 4);
    printHex("OSC(0xE00)   読み: ", b, 4);
    Serial.println("  期待値: 60 04 00 00 (2 バイト目の bit2=0x04 は OSCRDY。0x00 なら水晶が動いていない)");

    // 4) RAM 読み戻し
    const uint8_t pats[3][4] = {
        {0xA5, 0xA5, 0xA5, 0xA5},
        {0x5A, 0x5A, 0x5A, 0x5A},
        {0x01, 0x02, 0x04, 0x08}
    };
    uint8_t ok = 0;
    for (uint8_t k = 0; k < 3; k++)
    {
        writeBytes(0x400, pats[k], 4);
        uint8_t r[4] = {0, 0, 0, 0};
        readBytes(0x400, r, 4);
        const bool same = (r[0] == pats[k][0]) && (r[1] == pats[k][1]) && (r[2] == pats[k][2]) && (r[3] == pats[k][3]);
        printHex(same ? "RAM 読み戻し OK: " : "RAM 読み戻し NG: ", r, 4);
        if (same) { ok++; }
    }
    Serial.print("RAM 読み戻し: ");
    Serial.print(ok);
    Serial.println(" / 3 回 一致");
    Serial.println("=== 終わり ===");
}

void loop()
{
}
