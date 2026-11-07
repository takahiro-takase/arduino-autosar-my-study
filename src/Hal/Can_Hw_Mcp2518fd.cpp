/**
 * \file    Can_Hw_Mcp2518fd.cpp
 * \brief   Can ハードウェア依存層 実装 (MCP2518FD / ACAN2517FD ラッパー)
 * \details Can_Hw.h の関数を、MCP2518FD（CAN FD コントローラ）で実装する。
 *          MCP2515 版（Can_Hw.cpp）の代わりに、ビルドスイッチ CAN_HW_MCP2518FD を定義したとき
 *          だけコンパイルされる（platformio.ini の env:uno_r4_fd）。
 *
 *          段階 1（クラシック CAN 2.0、最大 8 byte）: 通常の送受信、Listen-Only、Loopback、Bus-Off と
 *          エラー状態の取得、送信の取り消し、受信オーバーフローの検出、INT ピンの割り込みを実装する。
 *          受信フィルタ（Can_Hw_InitMask/InitFilter）とスリープからの復帰は、まだ実装していない。
 *
 *          MCP2515 版との主な違い:
 *            - 送信は非同期。Can_Hw_Send() は、送信キューへ積めたら成功とする。ACK が返らないことは
 *              検出しない（送信できないフレームはキューに残り続ける。Can_Hw_AbortTx() で取り消す）。
 *            - INT ピンは、ライブラリには渡さない（割り込みは Can_Hw_AttachRxIsr() で、Can.c が使う）。
 *              受信フレームの取り込みは、Can_Hw_CheckReceive() の中で、ライブラリの poll() を呼んで行う。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#ifdef CAN_HW_MCP2518FD

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <Arduino.h>
#include <SPI.h>
#include <ACAN2517FD.h>
#include <new>
#include "Can_Hw.h"
#include "Det.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

#define TAG "Can_Hw"

/** 基板（MCP2518 + MCP2562FD CAN FD Bus モジュール）の水晶。Can_Config の crystalFreq は使わない。 */
#define CAN_HW_MCP2518FD_OSC  ACAN2517FDSettings::OSC_20MHz

/** MCP2518FD の CiCON レジスタ（0x000）のバイト 3。bit2:0 = REQOP、bit3 = ABAT（送信の中止要求）。 */
#define CON_REGISTER_BYTE3  0x003U
#define CON_ABAT_BIT        0x08U

/** MCP2518FD の CiTREC レジスタ（errorCounters() の戻り値）のビット。 */
#define TREC_RXBP  (1UL << 19)   /* 受信エラーパッシブ */
#define TREC_TXBP  (1UL << 20)   /* 送信エラーパッシブ */
#define TREC_TXBO  (1UL << 21)   /* Bus-Off            */

/**
 * Can_Cfg.h の CAN_BAUDRATE_* 値（mcp_can の CAN_*KBPS と同じ番号）から、bps への変換表。
 * ACAN2517FD は bps そのものを受け取るため、番号をそのまま渡すと 13bps などになり、
 * ビットレートを作れず begin() が kTooFarFromDesiredBitRate(0x4) で失敗する。
 */
static const uint32_t Can_Hw_BitRateTable[] = {
    4096UL,    /*  0: 4.096kbps */
    5000UL,    /*  1: 5kbps     */
    10000UL,   /*  2: 10kbps    */
    20000UL,   /*  3: 20kbps    */
    31250UL,   /*  4: 31.25kbps */
    33300UL,   /*  5: 33.3kbps  */
    40000UL,   /*  6: 40kbps    */
    50000UL,   /*  7: 50kbps    */
    80000UL,   /*  8: 80kbps    */
    100000UL,  /*  9: 100kbps   */
    125000UL,  /* 10: 125kbps   */
    200000UL,  /* 11: 200kbps   */
    250000UL,  /* 12: 250kbps   */
    500000UL,  /* 13: 500kbps   */
    1000000UL  /* 14: 1Mbps     */
};
#define CAN_HW_BITRATE_TABLE_SIZE  (sizeof(Can_Hw_BitRateTable) / sizeof(Can_Hw_BitRateTable[0]))

/** MCP2518FD の CiINT レジスタ（0x01C）。バイト 1 の bit6 = WAKIF（起床フラグ）、バイト 3 の bit6 = WAKIE（起床割り込み許可）。 */
#define INT_FLAGS_BYTE1   0x01DU
#define INT_ENABLE_BYTE3  0x01FU
#define INT_WAKE_BIT      0x40U

/** OSC レジスタ（0xE00）の bit2 = OSCDIS（発振停止 = スリープ中）、バイト 1 の bit2 = OSCRDY（発振安定）。 */
#define OSC_REGISTER_ADDR  0xE00U
#define OSC_OSCDIS_BIT     0x04U
#define OSC_OSCRDY_BIT     0x04U

/** CiCON レジスタの先頭アドレス（診断の読み出し用）。 */
#define CON_REGISTER_ADDR  0x000U

/** SPI 命令の上位 4 bit（命令 + 12 bit アドレスの 16 bit 命令）。 */
#define SPI_CMD_RESET  0x00U
#define SPI_CMD_WRITE  0x20U
#define SPI_CMD_READ   0x30U

/** 初期化の試行回数（SPI 応答の一時的な乱れに備える）。 */
#define CAN_HW_INIT_RETRY  3U

/** 標準 ID（11 bit）で表せる最大値。これを超える ID は拡張フレームとして送る。 */
#define CAN_HW_STD_ID_MAX  0x7FFUL

/* ======================================================================
 * Global Variables
 * ====================================================================== */

alignas(ACAN2517FD) static uint8_t driverBuf[sizeof(ACAN2517FD)];
static ACAN2517FD* driver = nullptr;
static uint8_t  Can_Hw_CsPin  = 0xFFU;
static uint8_t  Can_Hw_IntPin = 0xFFU;
static void (*Can_Hw_Isr)(void) = nullptr;
static bool     Can_Hw_Started = false;   /**< begin() が成功したか */
static uint32_t Can_Hw_BaudId  = 0U;      /**< Can_Hw_Init() の baudrate（CAN_BAUDRATE_* の番号） */
static bool     Can_Hw_SleepRequested = false;  /**< スリープを要求した（起床待ち） */
static bool     Can_Hw_SleepEntered   = false;  /**< チップが実際にスリープに入ったことを確認した */

/** ライブラリ（begin() 内）と同じ 800kHz。ライブラリを介さずレジスタを直接読み書きするときに使う。 */
static const SPISettings Can_Hw_SpiSettings(800UL * 1000UL, MSBFIRST, SPI_MODE0);

/* ======================================================================
 * Functions
 * ====================================================================== */

/** SPI の 1 回分の転送。命令（上位 4 bit）+ 12 bit アドレスに続けて、n byte を読む/書く（n=0 は RESET 命令）。 */
static void Can_Hw_Spi(uint8_t cmd, uint16_t addr, uint8_t* buf, uint8_t n)
{
    SPI.beginTransaction(Can_Hw_SpiSettings);
    digitalWrite(Can_Hw_CsPin, LOW);
    (void)SPI.transfer((uint8_t)(cmd | ((addr >> 8) & 0x0FU)));
    (void)SPI.transfer((uint8_t)(addr & 0xFFU));
    for (uint8_t i = 0U; i < n; i++)
    {
        const uint8_t v = SPI.transfer((cmd == SPI_CMD_WRITE) ? buf[i] : 0x00U);
        if (cmd == SPI_CMD_READ) { buf[i] = v; }
    }
    digitalWrite(Can_Hw_CsPin, HIGH);
    SPI.endTransaction();
}

/** 1 byte のレジスタを、ライブラリを介さず読む（スリープ中でも読める OSC レジスタなどに使う）。 */
static uint8_t Can_Hw_Read8(uint16_t addr)
{
    uint8_t v = 0U;
    Can_Hw_Spi(SPI_CMD_READ, addr, &v, 1U);
    return v;
}

/** 1 byte のレジスタを、ライブラリを介さず書く。 */
static void Can_Hw_Write8(uint16_t addr, uint8_t value)
{
    Can_Hw_Spi(SPI_CMD_WRITE, addr, &value, 1U);
}

/** 1 byte のレジスタの、指定ビットを立てる/消す。 */
static void Can_Hw_Modify8(uint16_t addr, uint8_t setMask, uint8_t clearMask)
{
    Can_Hw_Write8(addr, (uint8_t)((Can_Hw_Read8(addr) | setMask) & (uint8_t)~clearMask));
}

/** SPI の RESET 命令（0x0000）を送り、チップを初期状態（構成モード）へ戻す。
 *  スリープ中のチップは発振が止まっており RESET を受け付けないため、先に発振を再開させて起こす。 */
static void Can_Hw_RawReset(void)
{
    if ((Can_Hw_Read8(OSC_REGISTER_ADDR) & OSC_OSCDIS_BIT) != 0U)
    {
        Can_Hw_Modify8(OSC_REGISTER_ADDR, 0U, OSC_OSCDIS_BIT);

        /* 発振が安定する（OSCRDY=1）まで待つ。最大 20ms。 */
        const uint32_t start = millis();
        while (((Can_Hw_Read8(OSC_REGISTER_ADDR + 1U) & OSC_OSCRDY_BIT) == 0U) && ((millis() - start) < 20U)) { }
    }

    Can_Hw_Spi(SPI_CMD_RESET, 0x000U, nullptr, 0U);
    delay(5);
}

/** スリープ中は発振が止まっていて、レジスタを読み書きできない。それ以外で begin() が成功しているか。 */
static bool Can_Hw_Usable(void)
{
    return Can_Hw_Started && !Can_Hw_SleepRequested;
}

/**
 * チップを RESET してから begin() で設定し直す（最大 CAN_HW_INIT_RETRY 回）。成功すると Listen-Only になる。
 * 起動時（Can_Hw_Init）と、スリープから起きたとき（Can_Hw_SetMode）に使う。
 * 失敗のたびに、CiCON と OSC の生の読み値を出す（原因の切り分け用）。
 */
static bool Can_Hw_BeginChip(void)
{
    ACAN2517FDSettings settings(CAN_HW_MCP2518FD_OSC, Can_Hw_BitRateTable[Can_Hw_BaudId], DataBitRateFactor::x1);
    settings.mRequestedMode = ACAN2517FDSettings::ListenOnly;

    for (uint8_t attempt = 1U; attempt <= CAN_HW_INIT_RETRY; attempt++)
    {
        Can_Hw_RawReset();
        const uint32_t errorCode = driver->begin(settings, nullptr);
        if (errorCode == 0U)
        {
            if (attempt > 1U) { DET_LOGW(TAG, "MCP2518FD begin() ok on attempt %u", (unsigned)attempt); }
            return true;
        }

        uint8_t con[4];
        uint8_t osc[4];
        Can_Hw_Spi(SPI_CMD_READ, CON_REGISTER_ADDR, con, 4U);
        Can_Hw_Spi(SPI_CMD_READ, OSC_REGISTER_ADDR, osc, 4U);
        DET_LOGE(TAG, "MCP2518FD begin() failed: 0x%lX (attempt %u) CiCON=%02X %02X %02X %02X OSC=%02X %02X %02X %02X",
                 (unsigned long)errorCode, (unsigned)attempt,
                 con[0], con[1], con[2], con[3], osc[0], osc[1], osc[2], osc[3]);
    }
    return false;
}

Can_Hw_ReturnType Can_Hw_Init(uint8_t csPin, uint32_t baudrate, uint8_t crystalFreqMhz)
{
    (void)crystalFreqMhz;   /* 基板の水晶は 20MHz 固定（CAN_HW_MCP2518FD_OSC） */

    /* INT ピンは 255（使わない）としてライブラリに渡す。割り込みは Can_Hw_AttachRxIsr() が受ける。 */
    driver = new (driverBuf) ACAN2517FD(csPin, SPI, 255U);
    Can_Hw_CsPin = csPin;

    SPI.begin();

    /* 前回のモードが残っていても begin() が失敗しないよう、Can_Hw_BeginChip() が RESET 命令から始める。 */
    pinMode(csPin, OUTPUT);
    digitalWrite(csPin, HIGH);

    if (baudrate >= CAN_HW_BITRATE_TABLE_SIZE)
    {
        DET_LOGE(TAG, "unsupported baudrate id: %lu", (unsigned long)baudrate);
        Can_Hw_Started = false;
        return CAN_HW_FAIL;
    }
    Can_Hw_BaudId = baudrate;

    /* クラシック CAN 2.0（データ部のビットレートは公称と同じ）。まず Listen-Only で始める（Can.c と同じ順序）。 */
    Can_Hw_Started = Can_Hw_BeginChip();
    return Can_Hw_Started ? CAN_HW_OK : CAN_HW_FAIL;
}

Can_Hw_ReturnType Can_Hw_Send(uint32_t id, uint8_t dlc, const uint8_t* data)
{
    if (!Can_Hw_Started) return CAN_HW_FAIL;
    if (dlc > 8U)
    {
        DET_LOGE(TAG, "TX FAIL id=0x%lX dlc=%u (classic CAN: max 8)", (unsigned long)id, (unsigned)dlc);
        return CAN_HW_FAIL;
    }

    CANFDMessage msg;
    msg.id  = id;
    msg.ext = (id > CAN_HW_STD_ID_MAX);
    msg.len = dlc;
    for (uint8_t i = 0U; i < dlc; i++) { msg.data[i] = data[i]; }

    if (!driver->tryToSend(msg))
    {
        DET_LOGE(TAG, "TX FAIL id=0x%lX (transmit queue full)", (unsigned long)id);
        return CAN_HW_FAIL;
    }

    char hexbuf[25];
    Log_HexStr(hexbuf, sizeof(hexbuf), data, dlc);
    DET_LOGD(TAG, "TX OK id=0x%lX dlc=%u [%s]", (unsigned long)id, (unsigned)dlc, hexbuf);
    return CAN_HW_OK;
}

Can_Hw_ReturnType Can_Hw_Read(uint32_t* id, uint8_t* dlc, uint8_t* data)
{
    if (!Can_Hw_Started) return CAN_HW_FAIL;

    CANFDMessage msg;
    if (!driver->receive(msg))
    {
        DET_LOGW(TAG, "Read: no msg");
        return CAN_HW_FAIL;
    }

    uint8_t len = msg.len;
    if (len > 8U)
    {
        /* 段階 1 はクラシック CAN だけ。FD のフレームが来ても、先頭の 8 byte だけを渡す。 */
        DET_LOGE(TAG, "Read: len clamp %u->8", (unsigned)len);
        len = 8U;
    }

    *id  = msg.id;
    *dlc = len;
    for (uint8_t i = 0U; i < len; i++) { data[i] = msg.data[i]; }

    char hexbuf[25];
    Log_HexStr(hexbuf, sizeof(hexbuf), data, len);
    DET_LOGD(TAG, "RX OK id=0x%lX dlc=%u [%s]", (unsigned long)*id, (unsigned)len, hexbuf);
    return CAN_HW_OK;
}

Can_Hw_ReturnType Can_Hw_InitMask(uint8_t num, uint8_t ext, uint32_t mask)
{
    /* 段階 1 では、ハードウェアの受信フィルタは使わない（全フレームを受信し、CanIf が振り分ける）。 */
    (void)num; (void)ext; (void)mask;
    return CAN_HW_OK;
}

Can_Hw_ReturnType Can_Hw_InitFilter(uint8_t num, uint8_t ext, uint32_t filter)
{
    (void)num; (void)ext; (void)filter;
    return CAN_HW_OK;
}

Can_Hw_ReturnType Can_Hw_SetMode(Can_Hw_Mode mode)
{
    if (!Can_Hw_Started) return CAN_HW_FAIL;

    /* 副作用（チップの初期化など）の前に、モードの値を確かめる。 */
    ACAN2517FDSettings::OperationMode target;
    switch (mode)
    {
    case CAN_HW_MODE_NORMAL:      target = ACAN2517FDSettings::Normal20B;         break;  /* クラシック CAN 2.0 */
    case CAN_HW_MODE_LISTEN_ONLY: target = ACAN2517FDSettings::ListenOnly;        break;
    case CAN_HW_MODE_SLEEP:       target = ACAN2517FDSettings::Sleep;             break;
    case CAN_HW_MODE_LOOPBACK:    target = ACAN2517FDSettings::InternalLoopBack;  break;
    default:
        return CAN_HW_FAIL;
    }

    /* スリープから起きた直後は、構成モードにいる。起床後は、起動時と同じくチップを初期化し直す
     * （起床のままでは、実機で送信が ACK されない状態が続いたため）。
     * 失敗しても Can_Hw_Started は落とさない（SleepRequested が残るので、次の SetMode() で再試行できる）。 */
    if (Can_Hw_SleepRequested && (mode != CAN_HW_MODE_SLEEP) && !Can_Hw_BeginChip())
    {
        return CAN_HW_FAIL;
    }
    (void)driver->performSleepModeToConfigurationMode();

    /* スリープ中にバスの優性エッジを検出したら、チップは自動で起床して構成モードへ戻る。
     * その起床で INT ピンを LOW にするため、スリープ要求時は起床割り込み（WAKIE）を許可する。
     * スリープから出るときは、起床フラグを消し、起床割り込みを止める。 */
    if (mode == CAN_HW_MODE_SLEEP)
    {
        Can_Hw_Modify8(INT_ENABLE_BYTE3, INT_WAKE_BIT, 0U);
    }
    else
    {
        Can_Hw_Modify8(INT_FLAGS_BYTE1,  0U, INT_WAKE_BIT);
        Can_Hw_Modify8(INT_ENABLE_BYTE3, 0U, INT_WAKE_BIT);
    }
    Can_Hw_SleepRequested = (mode == CAN_HW_MODE_SLEEP);
    Can_Hw_SleepEntered   = false;

    driver->setOperationMode(target);
    return CAN_HW_OK;
}

Can_Hw_ReturnType Can_Hw_CheckReceive(void)
{
    if (!Can_Hw_Usable()) return CAN_HW_FAIL;

    /* INT ピンをライブラリへ渡していないため、ここで、チップの割り込み要因を読んで、
     * 受信したフレームを、ライブラリの受信バッファへ取り込む。 */
    driver->poll();
    return driver->available() ? CAN_HW_OK : CAN_HW_FAIL;
}

Can_Hw_ReturnType Can_Hw_IsBusOff(void)
{
    if (!Can_Hw_Usable()) return CAN_HW_FAIL;

    const uint32_t trec = driver->errorCounters();
    return ((trec & TREC_TXBO) != 0UL) ? CAN_HW_OK : CAN_HW_FAIL;
}

Can_Hw_ReturnType Can_Hw_TakeRxOverflow(void)
{
    if (!Can_Hw_Usable()) return CAN_HW_FAIL;

    if (driver->hardwareReceiveBufferOverflowCount() == 0U) return CAN_HW_FAIL;
    driver->resetHardwareReceiveBufferOverflowCount();
    return CAN_HW_OK;
}

Can_Hw_ReturnType Can_Hw_AbortTx(void)
{
    if (!Can_Hw_Usable()) return CAN_HW_FAIL;

    /* CiCON の ABAT ビットを立てて、送信中・待機中のフレームをすべて取り消す
     * （ABAT は、取り消しが終わると、ハードウェアが自動的に 0 に戻す）。
     * REQOP の下位 3 bit は、そのまま残す。
     * 注意: ライブラリの送信バッファ（ソフトウェアのキュー）に残るフレームは、ここでは消せない。 */
    Can_Hw_Modify8(CON_REGISTER_BYTE3, CON_ABAT_BIT, 0U);
    DET_LOGD(TAG, "TX aborted (ABAT)");
    return CAN_HW_OK;
}

Can_Hw_ReturnType Can_Hw_GetErrorState(uint8_t* errorStateOut)
{
    if (!Can_Hw_Usable()) return CAN_HW_FAIL;

    const uint32_t trec = driver->errorCounters();
    if ((trec & TREC_TXBO) != 0UL)
    {
        *errorStateOut = 2U;        /* CAN_ERRORSTATE_BUSOFF */
    }
    else if ((trec & (TREC_TXBP | TREC_RXBP)) != 0UL)
    {
        *errorStateOut = 1U;        /* CAN_ERRORSTATE_PASSIVE */
    }
    else
    {
        *errorStateOut = 0U;        /* CAN_ERRORSTATE_ACTIVE */
    }
    return CAN_HW_OK;
}

Can_Hw_ReturnType Can_Hw_AttachRxIsr(uint8_t intPin, void (*isr)(void))
{
    /* MCP2518FD の INT（nINT）は、アクティブ LOW。MCP2515 版と同じく、立ち下がりエッジで登録する。 */
    pinMode(intPin, INPUT_PULLUP);
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
    if (!Can_Hw_SleepRequested) return CAN_HW_FAIL;

    /* スリープ中は発振が止まり（OSC.OSCDIS=1）、起床すると自動で再開する（OSCDIS=0）。
     * スリープに入る前（要求の直後）を起床と誤認しないよう、OSCDIS=1 を一度確認してから判定する。 */
    const bool oscDisabled = ((Can_Hw_Read8(OSC_REGISTER_ADDR) & OSC_OSCDIS_BIT) != 0U);
    if (oscDisabled)
    {
        Can_Hw_SleepEntered = true;
        return CAN_HW_FAIL;
    }
    /* 起床済み。スリープ入りを確認できていなくても（要求の直後に起床して、一度も OSCDIS=1 を見られなかった場合）、
     * 起床フラグ（WAKIF）が立っていれば、起床と判定する。 */
    const bool wakeFlag = ((Can_Hw_Read8(INT_FLAGS_BYTE1) & INT_WAKE_BIT) != 0U);
    return (Can_Hw_SleepEntered || wakeFlag) ? CAN_HW_OK : CAN_HW_FAIL;
}

#endif /* CAN_HW_MCP2518FD */
