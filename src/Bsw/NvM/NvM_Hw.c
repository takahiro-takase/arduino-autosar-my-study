/**
 * \file    NvM_Hw.c
 * \brief   NvM ハードウェア依存層 実装 (AVR eeprom_* ラッパー)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include "NvM_Hw.h"
#include <avr/eeprom.h>
#include <stdint.h>

void NvM_Hw_ReadBlock(void* DstRam, uint16 EepromAddr, uint16 Length)
{
    eeprom_read_block(DstRam, (const void*)(uintptr_t)EepromAddr, Length);
}

void NvM_Hw_WriteBlock(const void* SrcRam, uint16 EepromAddr, uint16 Length)
{
    eeprom_update_block(SrcRam, (void*)(uintptr_t)EepromAddr, Length);
}

uint8 NvM_Hw_ReadByte(uint16 EepromAddr)
{
    return eeprom_read_byte((const uint8*)(uintptr_t)EepromAddr);
}

void NvM_Hw_WriteByte(uint16 EepromAddr, uint8 Value)
{
    eeprom_update_byte((uint8*)(uintptr_t)EepromAddr, Value);
}
