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
#include <Arduino.h>
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
        uint8_t eflg = driver->getError();
        DET_LOGE(TAG, "TX FAIL id=0x%lX eflg=0x%02X (TXBO=%u TXEP=%u TXWAR=%u)",
                 (unsigned long)id,
                 (unsigned)eflg,
                 (unsigned)((eflg >> 5) & 1U),  /* bit5: Bus-Off */
                 (unsigned)((eflg >> 4) & 1U),  /* bit4: TX Error-Passive */
                 (unsigned)((eflg >> 2) & 1U));  /* bit2: TX Error-Warning */
        return CAN_HW_FAIL;
    }

    char hexbuf[25];
    Log_HexStr(hexbuf, sizeof(hexbuf), data, dlc);
    DET_LOGI(TAG, "TX OK id=0x%lX dlc=%u [%s]", (unsigned long)id, (unsigned)dlc, hexbuf);

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

    /* MCP2515 の DLC レジスタは 4bit (0-15) のため、バス異常やノイズで
     * フィールドが壊れると 8 を超える値を返し得る。クランプしないと
     * 直後の buf[8]/呼び出し元 data[8] への固定長コピーでスタックを
     * 破壊してしまう（過去に Com で同種のバッファ破壊バグを修正済み）。 */
    if (len > 8U)
    {
        DET_LOGE(TAG, "Read: DLC clamp %u->8 (corrupt frame?)", (unsigned)len);
        len = 8U;
    }

    *id  = (uint32_t)rxId;
    *dlc = len;
    for (int i = 0; i < len; i++) data[i] = buf[i];

    char hexbuf[25];
    Log_HexStr(hexbuf, sizeof(hexbuf), buf, len);
    DET_LOGI(TAG, "RX OK id=0x%lX dlc=%u [%s]", (unsigned long)*id, (unsigned)len, hexbuf);

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
    case CAN_HW_MODE_SLEEP:
        /* CAN バス活動でのウェイクアップ割り込み (WAKIF) を有効化してからスリープへ
         * 入る。MCP2515 はスリープ中にバス活動を検知すると、ソフトウェアの関与なしに
         * 自律的に Listen-Only モードへ遷移し INT ピンをアサートする（データシート
         * 仕様。mcp_can ライブラリの setSleepWakeup() が CANINTE.WAKIE ビットを
         * 制御する）。 */
        driver->setSleepWakeup(1);
        mcpMode = MCP_SLEEP;
        break;
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

Can_Hw_ReturnType Can_Hw_IsBusOff(void)
{
    if (driver == nullptr) return CAN_HW_FAIL;

    uint8_t eflg = driver->getError();
    static uint8_t prevEflg = 0xFFU;  /* 初回は必ずログ出力するため、前回値を存在し得ない値で初期化 */
    /* エラーフラグに変更がある場合のみログ出力（1ms 周期での呼び出しによる大量出力を防ぐ） */
    if (eflg != prevEflg)
    {
        prevEflg = eflg;
        DET_LOGD(TAG, "EFLG=0x%02X TXBO=%u TXEP=%u TXWAR=%u EWARN=%u",
                 (unsigned)eflg,
                 (unsigned)((eflg >> 5) & 1U),  /* bit5: Bus-Off */
                 (unsigned)((eflg >> 4) & 1U),  /* bit4: TX Error-Passive */
                 (unsigned)((eflg >> 2) & 1U),  /* bit2: TX Error-Warning */
                 (unsigned)((eflg >> 0) & 1U));  /* bit0: Error Warning */
    }

    /* MCP2515 EFLG bit5 = TXBO（Bus-Off Error Flag） */
    return ((eflg & 0x20U) != 0U) ? CAN_HW_OK : CAN_HW_FAIL;
}

Can_Hw_ReturnType Can_Hw_AttachRxIsr(uint8_t intPin, void (*isr)(void))
{
    /* MCP2515 の INT は動作中オープンドレイン・アクティブ LOW。
     * mcp_can ライブラリ自体は INT ピンの pinMode を設定しないため
     * （公式サンプルもスケッチ側で INPUT に設定する規約）、ここで行う。 */
    pinMode(intPin, INPUT);
    attachInterrupt(digitalPinToInterrupt(intPin), isr, FALLING);
    return CAN_HW_OK;
}
