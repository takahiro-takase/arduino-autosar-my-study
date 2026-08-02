/**
 * \file    Det_Hw.cpp
 * \brief   Det ハードウェア依存層 実装 (Arduino Serial 出力)
 * \details Arduino API を呼ぶ唯一の場所。BSW の .c ファイルは Arduino API を
 *          直接参照しない（Det.c はこのファイルが提供する Det_Hw.h 経由でのみ
 *          Serial に触れる）。
 *
 *          本ファイルが .cpp である理由:
 *          Serial / F() / (__FlashStringHelper*) キャストなど Arduino の
 *          出力 API が C++ のため、Can_Hw.cpp / Wdg_Hw.cpp 等と同じ理由で
 *          C++ として実装する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include <Arduino.h>
#include "Det_Hw.h"

static void Det_Hw_PrintLevel(LogLevel lvl)
{
    switch (lvl)
    {
        case LOG_E: Serial.print(F("ERROR")); break;
        case LOG_W: Serial.print(F("WARN ")); break;
        case LOG_I: Serial.print(F("INFO ")); break;
        default:    Serial.print(F("DEBUG")); break;
    }
}

void Det_Hw_PrintLogLine(LogLevel lvl, PGM_P tag_P, const char* msg)
{
    Serial.print('[');
    Serial.print(millis());
    Serial.print(F("ms] "));
    Det_Hw_PrintLevel(lvl);
    Serial.print(' ');
    Serial.print((__FlashStringHelper*)tag_P);
    Serial.print(F(": "));
    Serial.println(msg);
}

void Det_Hw_PrintDetError(uint16 ModuleId, uint8 InstanceId, uint8 ApiId, uint8 ErrorId)
{
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
}
