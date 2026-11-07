// MCP2518FD(CAN FD モジュール)の動作確認用スケッチ(ループバック)。
//
// 目的: 組み込みの通信スタックとは切り離して、基板・配線・水晶(20MHz)・ライブラリが
//       動くかを、CAN バスにつながずに確かめる。MCP2518FD の内部ループバックを使うため、
//       MCP2562FD(トランシーバ)は使わない。
//
// 配線(UNO R4 → 基板 J3):
//   D13 → SCK / D11 → SI(MOSI) / D12 → SO(MISO) / D10 → CS / D2 → INT / 5V → 5V / GND → GND
//   MCP2515 のモジュールと同じピン。MCP2515 は外しておくこと(同じ SPI の線を共有するため)。
//   J1 のジャンパは 5V 側。
//
// 実行: pio run -e mcp2518fd_loopback -t upload / pio device monitor -b 115200

#include <Arduino.h>
#include <SPI.h>
#include <ACAN2517FD.h>

static const byte MCP2518FD_CS  = 10;
static const byte MCP2518FD_INT = 2;

static ACAN2517FD can(MCP2518FD_CS, SPI, MCP2518FD_INT);

static uint32_t passCount = 0;
static uint32_t failCount = 0;

static void report(const char* name, bool ok)
{
    Serial.print(ok ? "[PASS] " : "[FAIL] ");
    Serial.println(name);
    if (ok) { passCount++; } else { failCount++; }
}

static bool sendAndReceive(const char* name, CANFDMessage& tx)
{
    if (!can.tryToSend(tx))
    {
        Serial.print("  送信キューに入らない: ");
        Serial.println(name);
        return false;
    }
    const uint32_t start = millis();
    CANFDMessage rx;
    while ((millis() - start) < 200U)
    {
        can.poll();
        if (can.receive(rx))
        {
            bool same = (rx.id == tx.id) && (rx.len == tx.len);
            for (uint8_t i = 0; same && (i < tx.len); i++)
            {
                same = (rx.data[i] == tx.data[i]);
            }
            if (!same)
            {
                Serial.print("  受信内容が違う: id=0x");
                Serial.print(rx.id, HEX);
                Serial.print(" len=");
                Serial.println(rx.len);
            }
            return same;
        }
    }
    Serial.println("  200ms 以内に受信なし");
    return false;
}

void setup()
{
    Serial.begin(115200);
    const uint32_t t0 = millis();
    while (!Serial && ((millis() - t0) < 3000U)) { }
    Serial.println();
    Serial.println("=== MCP2518FD ループバック確認 ===");

    SPI.begin();

    // 20MHz の水晶、公称 500kbps、データ部は 4 倍(2Mbps)。
    ACAN2517FDSettings settings(ACAN2517FDSettings::OSC_20MHz, 500UL * 1000UL, DataBitRateFactor::x4);
    settings.mRequestedMode = ACAN2517FDSettings::InternalLoopBack;

    const uint32_t errorCode = can.begin(settings, [] { can.isr(); });
    Serial.print("can.begin() の結果: 0x");
    Serial.println(errorCode, HEX);
    report("初期化(SPI 通信・水晶・設定)", errorCode == 0U);
    if (errorCode != 0U)
    {
        Serial.println("  → SPI の配線(特に CS=D10)、5V/GND、水晶の周波数(20MHz)を確認する");
        Serial.println("=== 初期化に失敗したため、以降の試験は行わない ===");
        return;
    }

    // 1) クラシック CAN: 8 byte
    {
        CANFDMessage tx;
        tx.id  = 0x123;
        tx.len = 8;
        for (uint8_t i = 0; i < 8; i++) { tx.data[i] = (uint8_t)(0x10 + i); }
        report("クラシック 8 byte", sendAndReceive("classic8", tx));
    }

    // 2) CAN FD: 64 byte(BRS あり)
    {
        CANFDMessage tx;
        tx.id   = 0x456;
        tx.type = CANFDMessage::CANFD_WITH_BIT_RATE_SWITCH;
        tx.len  = 64;
        for (uint8_t i = 0; i < 64; i++) { tx.data[i] = (uint8_t)(0xA0 + i); }
        report("CAN FD 64 byte(BRS)", sendAndReceive("fd64brs", tx));
    }

    // 3) CAN FD: 12 byte(DLC の有効長の中間値)
    {
        CANFDMessage tx;
        tx.id   = 0x1AB;
        tx.type = CANFDMessage::CANFD_NO_BIT_RATE_SWITCH;
        tx.len  = 12;
        for (uint8_t i = 0; i < 12; i++) { tx.data[i] = (uint8_t)(0x30 + i); }
        report("CAN FD 12 byte(BRS なし)", sendAndReceive("fd12", tx));
    }

    Serial.print("=== 結果: PASS=");
    Serial.print(passCount);
    Serial.print(" FAIL=");
    Serial.print(failCount);
    Serial.println(" ===");
}

void loop()
{
}
