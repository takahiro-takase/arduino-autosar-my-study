/**
 * \file    Fee_Hw.h
 * \brief   Fee ハードウェア依存層 内部インタフェース
 * \details Fee.c (純粋 C, AUTOSAR API 層) と、Renesas RA (Arduino UNO R4) の
 *          フラッシュエミュレーション EEPROM (EEPROM.h) への実際のバイト/
 *          ブロック読み書きとの境界を定義する。Fee.c はこのヘッダ経由でのみ
 *          EEPROM にアクセスし、EEPROM.h を直接知らない。
 *          本ヘッダは Fee.c と Fee_Hw.cpp 以外からインクルードしないこと。
 *          旧 NvM_Hw.h/.cpp を Fee_Hw へ改名したもの（詳細は MemIf.c 冒頭の
 *          コメント参照）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef FEE_HW_H
#define FEE_HW_H

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
void Fee_Hw_ReadBlock(void* DstRam, uint16 EepromAddr, uint16 Length);

/**
 * \brief   RAM の内容を不揮発メモリの指定アドレスへ書き込む（ブロッキング）。
 *
 * \details 既存値と異なるバイトのみ物理書き込みする（EEPROM.update() が
 *          既に持つ性質）。Fee_WriteImmediate() （Os スケジューラ開始前の
 *          同期書き込み）専用。実行中の非同期ジョブ（Fee_MainFunction()）は
 *          Fee_Hw_WriteByte() を 1 バイトずつ使う。
 *
 * \param[in]  SrcRam      書き込み元 RAM バッファ。
 * \param[in]  EepromAddr  不揮発メモリ上の開始アドレス。
 * \param[in]  Length      書き込むバイト数。
 */
void Fee_Hw_WriteBlock(const void* SrcRam, uint16 EepromAddr, uint16 Length);

/**
 * \brief   不揮発メモリの指定アドレスへ 1 バイト書き込む。
 *
 * \param[in]  EepromAddr  不揮発メモリ上のアドレス。
 * \param[in]  Value       書き込む値。
 */
void Fee_Hw_WriteByte(uint16 EepromAddr, uint8 Value);

#ifdef __cplusplus
}
#endif

#endif /* FEE_HW_H */
