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
    if (lvl > DET_LOG_LEVEL) return;  /* DET_LOG_LEVEL より重要度が低いログは抑制 */

    char buf[LOG_BUF_SIZE];
    va_list args;
    va_start(args, fmt_P);
    /* Renesas RA (ARM Cortex-M) はフラッシュがメモリ空間にマップされており、
     * PROGMEM ポインタを通常ポインタとして読めるため、AVR 専用の
     * vsnprintf_P ではなく通常の vsnprintf を使う。 */
    vsnprintf(buf, sizeof(buf), fmt_P, args);
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

Std_ReturnType Det_ReportError(uint16 ModuleId, uint8 InstanceId, uint8 ApiId, uint8 ErrorId)
{
    /* DET_LOG_LEVEL によるレベル抑制は行わない（開発エラーは常に最重要度）。
     * DET_LOGE(...) と同じ書式のヘルパー（例: 2桁 0 埋め 16 進）を独自に持たず、
     * Serial の HEX 出力（Print::print(v, HEX)）をそのまま使う。0x0 のような
     * 1 桁出力になる場合があるが、ApiId/ErrorId は本プロジェクトの規模では
     * いずれも 1 バイト範囲に収まるため実用上の判読性は損なわない。 */
    Serial.print('[');
    Serial.print(millis());
    Serial.print(F("ms] DET M="));
    Serial.print(ModuleId);
    Serial.print(F(" I="));
    Serial.print(InstanceId);
    Serial.print(F(" API=0x"));
    Serial.print(ApiId, HEX);
    Serial.print(F(" ERR=0x"));
    Serial.println(ErrorId, HEX);

    /* [SWS_Det_00009]: 戻り値は互換性のためだけに存在し、実際には使われない。 */
    return E_OK;
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
