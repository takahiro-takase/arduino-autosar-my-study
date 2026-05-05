#ifndef DET_H
#define DET_H

/*
 * Det: Default Error Tracer（簡易実装）
 *
 * C ソースから Arduino の Serial を使うためのブリッジ。
 * Det.cpp が Serial を呼ぶ唯一の場所となり、
 * BSW の .c ファイルは Arduino API を直接参照しない。
 *
 * Flash 文字列: PSTR("...") でラップして Det_LogP / Det_PrintP に渡す。
 *              → 文字列が SRAM を消費しない。
 */

#include <avr/pgmspace.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Flash 文字列版（PSTR("...") を渡す） */
void Det_LogP(PGM_P msg);    /* msg + 改行 */
void Det_PrintP(PGM_P msg);  /* msg（改行なし）*/

/* RAM 文字列版（短命な文字列や数値変換結果など） */
void Det_Log(const char* msg);
void Det_Print(const char* msg);

/* 数値出力（改行なし）*/
void Det_PrintDec(unsigned long val);
void Det_PrintHex(unsigned long val);

/* 改行のみ */
void Det_Newline(void);

#ifdef __cplusplus
}
#endif

#endif
