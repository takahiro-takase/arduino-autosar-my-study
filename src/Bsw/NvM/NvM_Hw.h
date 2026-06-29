/**
 * \file    NvM_Hw.h
 * \brief   NvM ハードウェア依存層 内部インタフェース
 * \details NvM.c (純粋 C, AUTOSAR API 層) と、実際の不揮発メモリへの
 *          バイト/ブロック読み書きとの境界を定義する。
 *          NvM.c はこのヘッダ経由でのみ EEPROM にアクセスし、
 *          MCU 固有のヘッダ (avr/eeprom.h 等) を直接知らない。
 *          本ヘッダは NvM.c と NvM_Hw.c 以外からインクルードしないこと。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef NVM_HW_H
#define NVM_HW_H

#include "Std_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   不揮発メモリの指定アドレスから RAM へバイト列を読み込む。
 *
 * \param[out] DstRam      読み込み先 RAM バッファ。
 * \param[in]  EepromAddr  不揮発メモリ上の開始アドレス。
 * \param[in]  Length      読み込むバイト数。
 */
void NvM_Hw_ReadBlock(void* DstRam, uint16 EepromAddr, uint16 Length);

/**
 * \brief   RAM の内容を不揮発メモリの指定アドレスへ書き込む。
 *
 * \details 実装は「既存値と異なるバイトのみ物理書き込みする」ことが望ましい
 *          (EEPROM 消耗低減、AVR の eeprom_update_* と同等の振る舞い)。
 *
 * \param[in]  SrcRam      書き込み元 RAM バッファ。
 * \param[in]  EepromAddr  不揮発メモリ上の開始アドレス。
 * \param[in]  Length      書き込むバイト数。
 */
void NvM_Hw_WriteBlock(const void* SrcRam, uint16 EepromAddr, uint16 Length);

/**
 * \brief   不揮発メモリの指定アドレスから 1 バイト読み込む。
 *
 * \param[in]  EepromAddr  不揮発メモリ上のアドレス。
 *
 * \return  読み込んだ値。
 */
uint8 NvM_Hw_ReadByte(uint16 EepromAddr);

/**
 * \brief   不揮発メモリの指定アドレスへ 1 バイト書き込む。
 *
 * \param[in]  EepromAddr  不揮発メモリ上のアドレス。
 * \param[in]  Value       書き込む値。
 */
void NvM_Hw_WriteByte(uint16 EepromAddr, uint8 Value);

#ifdef __cplusplus
}
#endif

#endif /* NVM_HW_H */
