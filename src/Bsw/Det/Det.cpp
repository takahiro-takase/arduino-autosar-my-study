/**
 * \file    Det.cpp
 * \brief   Default Error Tracer 実装 (Arduino Serial 出力)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include <Arduino.h>
#include "Det.h"

/*
 * Det.cpp が Arduino API を呼ぶ唯一の場所。
 * 関数は Det.h の extern "C" ブロックで宣言されているため、
 * ここで定義すると自動的に C リンケージになる。
 */

void Det_LogP(PGM_P msg)
{
    Serial.println((__FlashStringHelper*)msg);
}

void Det_PrintP(PGM_P msg)
{
    Serial.print((__FlashStringHelper*)msg);
}

void Det_Log(const char* msg)
{
    Serial.println(msg);
}

void Det_Print(const char* msg)
{
    Serial.print(msg);
}

void Det_PrintDec(unsigned long val)
{
    Serial.print(val, DEC);
}

void Det_PrintHex(unsigned long val)
{
    Serial.print(val, HEX);
}

void Det_Newline(void)
{
    Serial.println();
}
