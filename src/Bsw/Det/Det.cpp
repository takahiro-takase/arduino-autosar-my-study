/**
 * \file    Det.cpp
 * \brief   Default Error Tracer 実装 (Arduino Serial 出力)
 *
 * \details Arduino API を呼ぶ唯一の場所。BSW の .c ファイルは
 *          Arduino API を直接参照しない。
 *
 *          出力フォーマット:
 *            [<ms>ms] LEVEL TAG/func: message\r\n
 *            LEVEL は 5 文字固定 (ERROR/WARN /INFO /DEBUG) で列が揃う。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include <Arduino.h>
#include <stdarg.h>
#include "Det.h"

extern unsigned long millis(void);

static void Log_PrintLevel(LogLevel lvl)
{
    switch (lvl)
    {
        case LOG_E: Serial.print(F("ERROR")); break;
        case LOG_W: Serial.print(F("WARN ")); break;
        case LOG_I: Serial.print(F("INFO ")); break;
        default:    Serial.print(F("DEBUG")); break;
    }
}

void Log_Write(LogLevel lvl, PGM_P tag_P, PGM_P fmt_P, ...)
{
    char buf[LOG_BUF_SIZE];
    va_list args;
    va_start(args, fmt_P);
#if defined(__AVR__)
    vsnprintf_P(buf, sizeof(buf), fmt_P, args);
#else
    /* 非 AVR (例: Renesas RA, ARM Cortex-M) はフラッシュがメモリ空間に
     * マップされており、PROGMEM ポインタを通常ポインタとして読める。
     * vsnprintf_P 相当の可変引数版が無いコアもあるため、通常の
     * vsnprintf にフォールバックする。 */
    vsnprintf(buf, sizeof(buf), fmt_P, args);
#endif
    va_end(args);

    Serial.print('[');
    Serial.print(millis());
    Serial.print(F("ms] "));
    Log_PrintLevel(lvl);
    Serial.print(' ');
    Serial.print((__FlashStringHelper*)tag_P);
    Serial.print(F(": "));
    Serial.println(buf);
}

void Log_HexStr(char* dst, uint8_t dstSize,
                const uint8_t* src, uint8_t srcLen)
{
    static const char hex[] = "0123456789ABCDEF";
    uint8_t pos = 0U;
    for (uint8_t i = 0U; i < srcLen && (pos + 3U) < dstSize; i++)
    {
        if (i > 0U) dst[pos++] = ' ';
        dst[pos++] = hex[src[i] >> 4U];
        dst[pos++] = hex[src[i] & 0x0FU];
    }
    dst[pos] = '\0';
}
