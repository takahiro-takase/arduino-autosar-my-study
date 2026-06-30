/**
 * \file    NvM_Hw.cpp
 * \brief   NvM ハードウェア依存層 実装 (AVR eeprom_* / Renesas RA EEPROM.h)
 * \details Renesas RA 側は EEPROM.h が C++ クラス (EEPROMClass) のため、
 *          本ファイルは .cpp とし、C リンケージの関数として公開する
 *          (Can_Hw.cpp が mcp_can ライブラリを包む手法と同じパターン)。
 *          AVR 側は avr-libc の eeprom_* 関数 (C 関数) をそのまま使う。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include "NvM_Hw.h"

#if defined(__AVR__)

#include <avr/eeprom.h>
#include <stdint.h>

extern "C" void NvM_Hw_ReadBlock(void* DstRam, uint16 EepromAddr, uint16 Length)
{
    eeprom_read_block(DstRam, (const void*)(uintptr_t)EepromAddr, Length);
}

extern "C" void NvM_Hw_WriteBlock(const void* SrcRam, uint16 EepromAddr, uint16 Length)
{
    eeprom_update_block(SrcRam, (void*)(uintptr_t)EepromAddr, Length);
}

extern "C" uint8 NvM_Hw_ReadByte(uint16 EepromAddr)
{
    return eeprom_read_byte((const uint8*)(uintptr_t)EepromAddr);
}

extern "C" void NvM_Hw_WriteByte(uint16 EepromAddr, uint8 Value)
{
    eeprom_update_byte((uint8*)(uintptr_t)EepromAddr, Value);
}

#else /* Renesas RA */

#include <EEPROM.h>

/* EEPROM.h は仮想 EEPROM (Flash エミュレーション) を提供する。
 * ブロック単位の読み書きAPIは無いため、バイト単位のループで実装する。
 * EEPROM.update() は AVR の eeprom_update_byte と同様、既存値と異なる
 * 場合のみ物理書き込みを行う。 */

extern "C" void NvM_Hw_ReadBlock(void* DstRam, uint16 EepromAddr, uint16 Length)
{
    uint8* dst = (uint8*)DstRam;
    for (uint16 i = 0U; i < Length; i++)
    {
        dst[i] = EEPROM.read((int)(EepromAddr + i));
    }
}

extern "C" void NvM_Hw_WriteBlock(const void* SrcRam, uint16 EepromAddr, uint16 Length)
{
    const uint8* src = (const uint8*)SrcRam;
    for (uint16 i = 0U; i < Length; i++)
    {
        EEPROM.update((int)(EepromAddr + i), src[i]);
    }
}

extern "C" uint8 NvM_Hw_ReadByte(uint16 EepromAddr)
{
    return EEPROM.read((int)EepromAddr);
}

extern "C" void NvM_Hw_WriteByte(uint16 EepromAddr, uint8 Value)
{
    EEPROM.update((int)EepromAddr, Value);
}

#endif
