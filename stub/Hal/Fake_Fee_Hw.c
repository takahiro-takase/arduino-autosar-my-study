/**
 * \file    Fake_Fee_Hw.c
 * \brief   Fee_Hw.h の RAM バッファによるフェイク実装（Fake_Fee_Hw.h 参照）。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Fee_Hw.h"
#include "Fake_Fee_Hw.h"
#include <string.h>

/* ======================================================================
 * Definitions
 * ====================================================================== */

/** 本プロジェクトが実際に使用する EEPROM アドレス範囲（NvM_Cfg.h 参照）に
 *  十分な余裕を持たせたバッファサイズ。 */
#define FAKE_FEE_HW_SIZE 4096U

/* ======================================================================
 * Global Variables
 * ====================================================================== */

/** 消去済みフラッシュを模した 0xFF 初期値（Fake_Fee_Hw.h 冒頭コメント参照）。 */
static uint8 FakeFeeHw_Buffer[FAKE_FEE_HW_SIZE];

/* ======================================================================
 * Functions
 * ====================================================================== */

void FakeFeeHw_Reset(void)
{
    memset(FakeFeeHw_Buffer, 0xFF, sizeof(FakeFeeHw_Buffer));
}

void Fee_Hw_ReadBlock(void* DstRam, uint16 EepromAddr, uint16 Length)
{
    memcpy(DstRam, &FakeFeeHw_Buffer[EepromAddr], Length);
}

void Fee_Hw_WriteBlock(const void* SrcRam, uint16 EepromAddr, uint16 Length)
{
    memcpy(&FakeFeeHw_Buffer[EepromAddr], SrcRam, Length);
}

void Fee_Hw_WriteByte(uint16 EepromAddr, uint8 Value)
{
    FakeFeeHw_Buffer[EepromAddr] = Value;
}
