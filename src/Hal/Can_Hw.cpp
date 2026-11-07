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

/* MCP2518FD 版（Can_Hw_Mcp2518fd.cpp）を使うビルドでは、この MCP2515 版はコンパイルしない。 */
#ifndef CAN_HW_MCP2518FD

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <Arduino.h>
#include <SPI.h>
#include "Can_Hw.h"
#include <mcp_can.h>
#include <new>
#include "Det.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "Can_Hw"

/* MCP2515 EFLG レジスタのビット位置。Can_Hw_IsBusOff()/Can_Hw_GetErrorState()
 * の両方がここを参照することで、値の食い違い（重複定義の一方だけ更新漏れ）を
 * 防ぐ（/code-review 指摘）。 */
#define MCP_EFLG_TXBO  0x20U  /* bit5: Bus-Off             */
#define MCP_EFLG_TXEP  0x10U  /* bit4: TX Error-Passive    */
#define MCP_EFLG_RXEP  0x08U  /* bit3: RX Error-Passive    */
#define MCP_EFLG_RXOVR 0xC0U  /* bit7: RX1OVR / bit6: RX0OVR（受信バッファのオーバーフロー） */


/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * Global Variables
 * ====================================================================== */

static uint8_t  driverBuf[sizeof(MCP_CAN)];
static MCP_CAN* driver = nullptr;
static uint8_t  Can_Hw_CsPin  = 0xFFU;  /**< Can_Hw_Init() の csPin（EFLG のフラグを SPI で直接消すのに使う） */
static uint8_t  Can_Hw_IntPin = 0xFFU;  /**< Can_Hw_AttachRxIsr() が登録したピン番号 */
static void (*Can_Hw_Isr)(void) = nullptr;  /**< Can_Hw_EnableRxIsr() での再登録用に記憶する ISR */

/* ======================================================================
 * Function Prototypes
 * ====================================================================== */

/* ======================================================================
 * Functions
 * ====================================================================== */

Can_Hw_ReturnType Can_Hw_ConfigureFd(uint8_t fdEnabled, uint32_t fdBaudrate, uint8_t bitRateSwitch)
{
    (void)fdBaudrate;
    (void)bitRateSwitch;

    /* MCP2515 は CAN FD に対応しない。 */
    if (fdEnabled != 0U)
    {
        DET_LOGE(TAG, "ConfigureFd: MCP2515 does not support CAN FD");
        return CAN_HW_FAIL;
    }
    return CAN_HW_OK;
}

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
    Can_Hw_CsPin = csPin;

    return (driver->begin(MCP_STDEXT, baudrate, mcpClock) == CAN_OK)
           ? CAN_HW_OK : CAN_HW_FAIL;
}

/** MCP2515 のレジスタの指定したビットを、SPI で直接書き換える（BIT MODIFY）。mcp_can には、レジスタを書く公開 API が無いので、
 *  ライブラリの mcp2515_modifyRegister() と同じ手順（SPI の設定も同じ）を直接送る。
 *  協調スケジューラ上の単一の流れから呼ばれ、割り込みは SPI に触れない（フラグを立てるだけ）。 */
static void Can_Hw_BitModify(uint8_t address, uint8_t mask, uint8_t data)
{
    SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
    digitalWrite(Can_Hw_CsPin, LOW);
    SPI.transfer(MCP_BITMOD);
    SPI.transfer(address);
    SPI.transfer(mask);
    SPI.transfer(data);
    digitalWrite(Can_Hw_CsPin, HIGH);
    SPI.endTransaction();
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
    DET_LOGD(TAG, "TX OK id=0x%lX dlc=%u [%s]", (unsigned long)id, (unsigned)dlc, hexbuf);

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
    DET_LOGD(TAG, "RX OK id=0x%lX dlc=%u [%s]", (unsigned long)*id, (unsigned)len, hexbuf);

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
        DET_LOGV(TAG, "EFLG=0x%02X TXBO=%u TXEP=%u TXWAR=%u EWARN=%u",
                 (unsigned)eflg,
                 (unsigned)((eflg >> 5) & 1U),  /* bit5: Bus-Off */
                 (unsigned)((eflg >> 4) & 1U),  /* bit4: TX Error-Passive */
                 (unsigned)((eflg >> 2) & 1U),  /* bit2: TX Error-Warning */
                 (unsigned)((eflg >> 0) & 1U));  /* bit0: Error Warning */
    }

    /* MCP2515 EFLG bit5 = TXBO（Bus-Off Error Flag） */
    return ((eflg & MCP_EFLG_TXBO) != 0U) ? CAN_HW_OK : CAN_HW_FAIL;
}

Can_Hw_ReturnType Can_Hw_TakeRxOverflow(void)
{
    if (driver == nullptr) return CAN_HW_FAIL;

    const uint8_t observed = (uint8_t)(driver->getError() & MCP_EFLG_RXOVR);
    if (observed == 0U) return CAN_HW_FAIL;

    /* RXnOVR は MCU が消さないと立ったままになる。読み取ったビットだけを消し、読んだ後に立った別のバッファの分は残す。 */
    Can_Hw_BitModify(MCP_EFLG, observed, 0x00U);
    return CAN_HW_OK;
}

Can_Hw_ReturnType Can_Hw_AbortTx(void)
{
    if (driver == nullptr) return CAN_HW_FAIL;

    /* 送信バッファごとに TXREQ だけを消して、未送信のフレームを取り消す（ABAT ビットは使わない。Can_Hw.h 参照）。 */
    static const uint8_t txCtrl[] = { MCP_TXB0CTRL, MCP_TXB1CTRL, MCP_TXB2CTRL };
    for (uint8_t i = 0U; i < (uint8_t)(sizeof(txCtrl) / sizeof(txCtrl[0])); i++)
    {
        Can_Hw_BitModify(txCtrl[i], MCP_TXB_TXREQ_M, 0x00U);
    }
    DET_LOGD(TAG, "TX buffers aborted (pending frames cancelled)");
    return CAN_HW_OK;
}

Can_Hw_ReturnType Can_Hw_GetErrorState(uint8_t* errorStateOut)
{
    if (driver == nullptr) return CAN_HW_FAIL;

    /* Bus-Off 判定は Can_Hw_IsBusOff() に委譲し、条件式を二重定義しない
     * （/simplify reuse 指摘）。getError() を都度読み直す分 SPI 読み出しが
     * 1 回増えるが、本関数は Os_MainFunction からは呼ばれない診断用の
     * オンデマンド API のため負荷への影響はない（/simplify efficiency 確認済み）。 */
    if (Can_Hw_IsBusOff() == CAN_HW_OK)
    {
        *errorStateOut = 2U;        /* CAN_ERRORSTATE_BUSOFF */
        return CAN_HW_OK;
    }

    /* Error-Passive は TEC/REC のどちらか一方でも閾値超過すれば成立する
     * （TXEP/RXEP は独立にセットされうる。/code-review 指摘: RXEP 側の
     * チェック漏れがあった。バス端の受信のみが劣化する環境ノイズ等で
     * REC だけが上がるケースを見逃していた）。 */
    uint8_t eflg = driver->getError();
    if ((eflg & (MCP_EFLG_TXEP | MCP_EFLG_RXEP)) != 0U)
        *errorStateOut = 1U;        /* CAN_ERRORSTATE_PASSIVE */
    else
        *errorStateOut = 0U;        /* CAN_ERRORSTATE_ACTIVE */

    return CAN_HW_OK;
}

Can_Hw_ReturnType Can_Hw_AttachRxIsr(uint8_t intPin, void (*isr)(void))
{
    /* MCP2515 の INT は動作中オープンドレイン・アクティブ LOW。
     * mcp_can ライブラリ自体は INT ピンの pinMode を設定しないため
     * （公式サンプルもスケッチ側で INPUT に設定する規約）、ここで行う。 */
    pinMode(intPin, INPUT);
    Can_Hw_IntPin = intPin;
    Can_Hw_Isr    = isr;
    attachInterrupt(digitalPinToInterrupt(intPin), isr, FALLING);
    return CAN_HW_OK;
}

Can_Hw_ReturnType Can_Hw_DisableRxIsr(void)
{
    detachInterrupt(digitalPinToInterrupt(Can_Hw_IntPin));
    return CAN_HW_OK;
}

Can_Hw_ReturnType Can_Hw_EnableRxIsr(void)
{
    attachInterrupt(digitalPinToInterrupt(Can_Hw_IntPin), Can_Hw_Isr, FALLING);
    return CAN_HW_OK;
}

Can_Hw_ReturnType Can_Hw_IsWakeupPending(void)
{
    return (digitalRead(Can_Hw_IntPin) == LOW) ? CAN_HW_OK : CAN_HW_FAIL;
}

#endif /* !CAN_HW_MCP2518FD */
