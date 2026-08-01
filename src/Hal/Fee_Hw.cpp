/**
 * \file    Fee_Hw.cpp
 * \brief   Fee ハードウェア依存層 実装 (Renesas RA EEPROM.h)
 * \details EEPROM.h は C++ クラス (EEPROMClass) のため、本ファイルは .cpp とし
 *          C リンケージの関数として公開する (Can_Hw.cpp が mcp_can ライブラリを
 *          包む手法と同じパターン)。
 *
 *          本プロジェクトが対応する MCU は Renesas RA (Arduino UNO R4) のみ
 *          （AVR/UNO 無印は初代のプログラムサイズ制限により移行済み。対になる
 *          Ea_Hw.c は削除済み）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include "Fee_Hw.h"
#include <EEPROM.h>

/* EEPROM.h は仮想 EEPROM (Flash エミュレーション) を提供する。
 * ブロック単位の読み書き API は無いため、バイト単位のループで実装する。
 * EEPROM.update() は AVR の eeprom_update_byte と同様、既存値と異なる
 * 場合のみ物理書き込みを行う。 */

extern "C" void Fee_Hw_ReadBlock(void* DstRam, uint16 EepromAddr, uint16 Length)
{
    uint8* dst = (uint8*)DstRam;
    for (uint16 i = 0U; i < Length; i++)
    {
        dst[i] = EEPROM.read((int)(EepromAddr + i));
    }
}

extern "C" void Fee_Hw_WriteBlock(const void* SrcRam, uint16 EepromAddr, uint16 Length)
{
    const uint8* src = (const uint8*)SrcRam;
    for (uint16 i = 0U; i < Length; i++)
    {
        EEPROM.update((int)(EepromAddr + i), src[i]);
    }
}

extern "C" void Fee_Hw_WriteByte(uint16 EepromAddr, uint8 Value)
{
    EEPROM.update((int)EepromAddr, Value);
}
