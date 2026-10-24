/**
 * \file    App_Test.h
 * \brief   汎用のテスト用 SW-C 公開インタフェース
 * \details 動作確認や練習のためのテスト専用 SW-C。次の 2 つを持つ。
 *          1. テスト用メッセージ (TestMsg, CAN 0x300) の送信（App_Test_Run）。
 *             信号表（config/data/can_signals.json）へ信号を追加する手順を試すための最小の例で、
 *             1 秒ごとに +1 するカウンタを TestMsg.TestCounter へ書き込む。CAN Tool の受信モニタに
 *             信号名つきで表示されるため、追加した信号の値を目視で確認できる。
 *             手順は tools/configurator/README.md の「信号を追加する練習」を参照。
 *          2. MISRA C の指摘修正の練習用パターン集（MISRA_PRACTICE_ENABLE が有効のときだけ）。
 *             手順は docs/misra/README.md を参照。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef APP_TEST_H
#define APP_TEST_H

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Std_Types.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

/* MISRA 練習用の有効化スイッチ。勉強するときだけ、次の行のコメントを外す。
 * コメントアウトしたまま（既定）なら、App_Test.c の練習用パターン集は #ifdef で除かれ、
 * ファームウェアにも通常の misra_check（tools/misra/run_misra.py）にも影響しない。
 * 有効にすると、指摘が出やすい書き方のソースが misra_check の解析対象に入る
 * （ファームウェアからは呼ばれない。実機向けのビルドで有効にするとエラーになる）。
 * 使い方は docs/misra/README.md を参照。 */
/* #define MISRA_PRACTICE_ENABLE */

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================
 * Global Variables
 * ====================================================================== */

/* ======================================================================
 * Functions
 * ====================================================================== */

/**
 * \brief   TestMsg.TestCounter を 1 つ進めて Rte 経由で書き込む Runnable。
 *
 * \details OS の周期タスク（1000 ms）から呼び出すこと。カウンタは uint8 で、
 *          255 の次は 0 に戻る。TestMsg は Com で PERIODIC 送信に設定して
 *          あるため、値の書き込みとは独立に周期（1000 ms）で送信される。
 *
 * \note    テスト専用。ECU の本来の機能には関与しない。
 */
void App_Test_Run(void);

#ifdef MISRA_PRACTICE_ENABLE

/**
 * \brief   MISRA C 練習用: uint8 / uint16 の型変換（int への拡張）パターン集
 *
 * \details C では uint8 同士の加減算・比較・ビット演算・シフトは、いったん int に拡張して計算される。
 *          そのため「int の結果を uint8 へ入れる」「int と unsigned を混ぜて比較する」書き方が
 *          MISRA C:2012 の essential type 系の規則（10.1 / 10.3 / 10.4 / 10.8）で大量に指摘される
 *          （定数に U を付ける指摘も、多くはこの 10.x として出る。ツールによって規則番号は変わる）。
 *          1 パターン = 1 関数。各関数に「指摘なし（正解）」と「指摘あり」の 2 通りの書き方を並べてある。
 *          模範解答は docs/misra/answer/、解説は docs/misra/ANSWERS.md を参照。
 *
 * \note    テスト専用。ECU の本来の機能には関与しない。
 */
uint8  Pat_Add(uint8 a, uint8 b);               /* P1  加算の結果を uint8 へ */
uint8  Pat_Sub(uint8 a, uint8 b);               /* P1  減算 */
uint8  Pat_Mul(uint8 a, uint8 b);               /* P1  乗算 */
uint8  Pat_CompoundAssign(uint8 a, uint8 b);    /* P2  += -= <<= などの複合代入 */
uint8  Pat_CompareConst(uint8 a);               /* P3  uint8 と整数定数（接尾辞なし）の比較 */
uint8  Pat_CompareSigned(uint8 a, sint16 s);    /* P4  uint8 と符号付きの比較 */
uint8  Pat_MaskLow(uint8 a);                    /* P5  ビット AND（マスクの定数） */
uint8  Pat_Invert(uint8 a);                     /* P6  ビット反転 ~ */
uint8  Pat_NotCompare(uint8 a);                 /* P6b ~ の結果を比較（潜在バグ） */
uint8  Pat_ShiftUp(uint8 a);                    /* P7  左シフト */
uint16 Pat_Combine(uint8 hi, uint8 lo);         /* P8  2 バイトの結合 */
uint8  Pat_Average(uint8 a, uint8 b);           /* P9  加算してから除算 */
uint8  Pat_Ternary(uint8 a, uint8 b);           /* P10 条件演算子の定数と戻り値 */
uint8  Pat_ArgExpr(uint8 a);                    /* P11 関数の引数に式を渡す */
uint8  Pat_ReturnExpr(uint8 a);                 /* P12 return の式 */
sint16 Pat_Diff(uint8 a, uint8 b);              /* P13 uint8 の差を符号付きへ */
uint8  Pat_BitTest(uint8 a);                    /* P14 ビット判定を条件に使う */
uint32 Pat_Scale(uint8 a);                      /* P15 uint8 に定数を掛けて広い型へ */
uint8  Pat_SetClearBit(uint8 a);                /* P16 ビットのセット/クリア */
uint8  Pat_Rotate(uint8 a);                     /* P17 ローテート */

/* uint8 と uint16 / uint32 を混ぜるパターン */
uint8  Pat_Narrow16To8(uint16 x);                /* P18 uint16 を uint8 へ（狭い型へ代入） */
uint16 Pat_Add8To16(uint8 a, uint8 b);           /* P19 uint8 の加算を uint16 へ（広い型へ代入） */
uint16 Pat_Sub8To16(uint8 a, uint8 b);           /* P20 uint8 の減算を uint16 へ */
uint16 Pat_Add8Add16(uint8 a, uint8 b, uint16 c);/* P21 uint8 同士の加算に uint16 を足す */
uint32 Pat_Mul16To32(uint16 a, uint16 b);        /* P22 uint16 の乗算を uint32 へ */
uint32 Pat_Add16To32(uint16 a, uint16 b);        /* P23 uint16 の加算を uint32 へ */
uint8  Pat_Compare16Const(uint16 x);             /* P24 uint16 と整数定数（接尾辞なし）の比較 */
uint16 Pat_Mask16(uint16 x);                     /* P25 uint16 のビット AND（マスクの定数） */
uint16 Pat_CastWideSum(uint8 a, uint8 b);        /* P26 加算の結果だけを後から uint16 へキャスト */
uint8  Pat_Sum8Narrow(uint8 a, uint16 b);        /* P27 uint8 + uint16 を uint8 へ */

#endif /* MISRA_PRACTICE_ENABLE */

#ifdef __cplusplus
}
#endif

#endif /* APP_TEST_H */
