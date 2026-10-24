/**
 * \file    App_Test.c
 * \brief   汎用のテスト用 SW-C 実装（TestMsg の送信、MISRA 練習用パターン集）
 * \details 1 秒ごとに +1 するカウンタを Rte_Write_TestMsg_TestCounter() で
 *          書き込む。末尾に MISRA 練習用のパターン集がある（MISRA_PRACTICE_ENABLE が有効のときだけ）。
 *          詳細は App_Test.h を参照。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "App_Test.h"
#include "Rte.h"

#if defined(MISRA_PRACTICE_ENABLE) && defined(ARDUINO)
#error "MISRA_PRACTICE_ENABLE は解析専用。実機向けのビルド（ARDUINO が定義される）では有効にしないこと"
#endif

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * Global Variables
 * ====================================================================== */

/* ======================================================================
 * Function Prototypes
 * ====================================================================== */

/* ======================================================================
 * Functions
 * ====================================================================== */

/* ----------------------------------------------------------------------
 * App_Test_Run
 * ---------------------------------------------------------------------- */

/**
 * \brief   TestMsg.TestCounter を 1 つ進めて Rte 経由で書き込む Runnable。
 *
 * \details OS の周期タスク（1000 ms）から呼び出すこと。カウンタは uint8 で、
 *          255 の次は 0 に戻る。TestMsg は Com で PERIODIC 送信に設定して
 *          あるため、値の書き込みとは独立に周期（1000 ms）で送信される。
 *
 * \note    テスト専用。ECU の本来の機能には関与しない。
 */
void App_Test_Run(void)
{
    /* TestMsg.TestCounter へ書き込む値（uint8 のため 255 の次は 0） */
    static uint8 s_TestCounter = 0U;

    s_TestCounter++;
    (void)Rte_Write_TestMsg_TestCounter(s_TestCounter);
}

/* ----------------------------------------------------------------------
 * MISRA C 練習用: uint8 の整数拡張（int への型変換）パターン集
 * ---------------------------------------------------------------------- */

#ifdef MISRA_PRACTICE_ENABLE
static uint8 Pass(uint8 x)
{
    return x;
}

uint8 Pat_Add(uint8 a, uint8 b)
{
    uint8 r;

    /* 指摘なし（正解）: 結果を (uint8) へ明示変換 */
    r = (uint8)(a + b);

    /* 指摘あり: a + b は int。int を uint8 へ入れている（10.3） */
    r = a + b;

    return r;
}

uint8 Pat_Sub(uint8 a, uint8 b)
{
    uint8 r;

    /* 指摘なし（正解）: 結果を (uint8) へ明示変換 */
    r = (uint8)(a - b);

    /* 指摘あり: a - b は int（a < b のとき負）。int を uint8 へ入れている（cppcheck では出ない。int への変換を細かく見るツール向け） */
    r = a - b;

    return r;
}

uint8 Pat_Mul(uint8 a, uint8 b)
{
    uint8 r;

    /* 指摘なし（正解）: 結果を (uint8) へ明示変換（上位ビットを捨てる意図を示す） */
    r = (uint8)(a * b);

    /* 指摘あり: a * b は int。int を uint8 へ入れている（cppcheck では出ない。int への変換を細かく見るツール向け） */
    r = a * b;

    return r;
}

uint8 Pat_CompoundAssign(uint8 a, uint8 b)
{
    uint8 r;

    /* 指摘なし（正解）: 複合代入をやめ、式に直して (uint8) へ明示変換。定数は U */
    r = a;
    r = (uint8)(r + b);
    r = (uint8)(r - 1U);
    r = (uint8)(r << 1U);

    /* 指摘あり: 複合代入は int で計算される。定数 1 が符号付き（10.4） */
    r = a;
    r += b;
    r -= 1;
    r <<= 1;

    return r;
}

uint8 Pat_CompareConst(uint8 a)
{
    uint8 r = 0U;

    /* 指摘なし（正解）: 定数に U */
    if (a > 10U)
    {
        r = 1U;
    }
    if (a == 200U)
    {
        r = 2U;
    }

    /* 指摘あり: uint8 と符号付き定数の比較（10.4） */
    if (a > 10)
    {
        r = 1U;
    }
    if (a == 200)
    {
        r = 2U;
    }

    return r;
}

uint8 Pat_CompareSigned(uint8 a, sint16 s)
{
    uint8 r = 0U;

    /* 指摘なし（正解）: 符号付き同士にそろえる */
    if ((sint16)a < s)
    {
        r = 1U;
    }

    /* 指摘あり: uint8 と符号付きの比較（10.4） */
    if (a < s)
    {
        r = 1U;
    }

    return r;
}

uint8 Pat_MaskLow(uint8 a)
{
    uint8 r;

    /* 指摘なし（正解）: マスクの定数に U。結果は (uint8) へ明示変換 */
    r = (uint8)(a & 0x0FU);

    /* 指摘あり: マスクの定数が符号付き（10.4） */
    r = a & 0x0F;

    return r;
}

uint8 Pat_Invert(uint8 a)
{
    uint8 r;

    /* 指摘なし（正解）: ~ の結果（int）を (uint8) へ戻す */
    r = (uint8)(~a);

    /* 指摘あり: ~a は int（上位ビットが 1 の負数）。int を uint8 へ入れている（cppcheck では出ない。int への変換を細かく見るツール向け） */
    r = ~a;

    return r;
}

uint8 Pat_NotCompare(uint8 a)
{
    uint8 r = 0U;

    /* 指摘なし（正解）: ~a（int）を (uint8) へ戻してから比較 */
    if ((uint8)(~a) == 0xF0U)
    {
        r = 1U;
    }

    /* 指摘あり: ~a は int（上位ビットが 1 の負数）で、0xF0U とは一致しないため、この条件は常に偽（潜在バグ。10.4） */
    if (~a == 0xF0U)
    {
        r = 1U;
    }

    return r;
}

uint8 Pat_ShiftUp(uint8 a)
{
    uint8 r;

    /* 指摘なし（正解）: シフト量に U。結果は (uint8) へ明示変換 */
    r = (uint8)(a << 4U);

    /* 指摘あり: 左シフトは int で行われる。int を uint8 へ入れている（cppcheck では出ない。int への変換を細かく見るツール向け） */
    r = a << 4;

    return r;
}

uint16 Pat_Combine(uint8 hi, uint8 lo)
{
    uint16 w;

    /* 指摘なし（正解）: 先に uint16 へ広げてからシフトし、全体を (uint16) へ */
    w = (uint16)(((uint16)hi << 8U) | (uint16)lo);

    /* 指摘あり: uint8 のまま 8 ビットシフトしている（10.6、12.2） */
    w = (hi << 8) | lo;

    return w;
}

uint8 Pat_Average(uint8 a, uint8 b)
{
    uint8 r;

    /* 指摘なし（正解）: 途中を uint16 に固定し、除数に U */
    r = (uint8)(((uint16)a + (uint16)b) / 2U);

    /* 指摘あり: 除数 2 が符号付き（10.4） */
    r = (a + b) / 2;

    return r;
}

uint8 Pat_Ternary(uint8 a, uint8 b)
{
    uint8 m = (a > b) ? a : b;
    uint8 f;
    uint8 r;

    /* 指摘なし（正解）: 三項演算子の定数に U。足し算の結果は (uint8) へ */
    f = (a == b) ? 1U : 0U;
    r = (uint8)(m + f);

    /* 指摘あり: 定数に U がない。m + f は int（代入・return するとき）（cppcheck では出ない。int への変換を細かく見るツール向け） */
    f = (a == b) ? 1 : 0;
    r = m + f;

    return r;
}

uint8 Pat_ArgExpr(uint8 a)
{
    uint8 r;

    /* 指摘なし（正解）: 引数の式を (uint8) へ明示変換して渡す */
    r = Pass((uint8)(a + 1U));

    /* 指摘あり: 引数の式 a + 1 は int。定数 1 が符号付き（10.4） */
    r = Pass(a + 1);

    return r;
}

uint8 Pat_ReturnExpr(uint8 a)
{
    uint8 r;

    /* 指摘なし（正解）: 式を (uint8) へ明示変換（return に書くときも同じ） */
    r = (uint8)(a + 1U);

    /* 指摘あり: 定数 1 が符号付き（10.4）。return a + 1; と書いた場合も同じ指摘 */
    r = a + 1;

    return r;
}

sint16 Pat_Diff(uint8 a, uint8 b)
{
    sint16 d;

    /* 指摘なし（正解）: 引く前に符号付きへ広げる */
    d = (sint16)((sint16)a - (sint16)b);

    /* 指摘あり: uint8 の引き算（int）を sint16 へ入れている（10.6） */
    d = a - b;

    return d;
}

uint8 Pat_BitTest(uint8 a)
{
    uint8 r = 0U;

    /* 指摘なし（正解）: 条件は「真偽値を返す式」にする。マスクの定数に U */
    if ((a & 0x01U) != 0U)
    {
        r = (uint8)(r | 1U);
    }
    if ((a & 0x80U) == 0U)
    {
        r = (uint8)(r | 2U);
    }

    /* 指摘あり: 整数をそのまま条件に使っている（14.4）。マスクの定数が符号付き（10.4） */
    if (a & 0x01)
    {
        r |= 1;
    }
    if (!(a & 0x80))
    {
        r |= 2;
    }

    return r;
}

uint32 Pat_Scale(uint8 a)
{
    uint32 ms;

    /* 指摘なし（正解）: 掛ける前に広い型へ。定数に U */
    ms = (uint32)a * 1000U;

    /* 指摘あり: uint8 と符号付き定数の乗算（int）を uint32 へ入れている（10.4、10.6） */
    ms = a * 1000;

    return ms;
}

uint8 Pat_SetClearBit(uint8 a)
{
    uint8 r;

    /* 指摘なし（正解）: 式に直して (uint8) へ明示変換。~ の結果も (uint8) へ戻す */
    r = a;
    r = (uint8)(r | 0x80U);
    r = (uint8)(r & (uint8)(~0x01U));

    /* 指摘あり: 複合代入と ~ は int で計算される（10.3） */
    r = a;
    r |= 0x80;
    r &= ~0x01;

    return r;
}

uint8 Pat_Rotate(uint8 a)
{
    uint8 r;

    /* 指摘なし（正解）: 両辺を (uint8) へ戻してから OR */
    r = (uint8)((uint8)(a << 1U) | (uint8)(a >> 7U));

    /* 指摘あり: シフトの結果（int）を OR して uint8 へ入れている（cppcheck では出ない。int への変換を細かく見るツール向け） */
    r = (a << 1) | (a >> 7);

    return r;
}

uint8 Pat_Narrow16To8(uint16 x)
{
    uint8 r;

    /* 指摘なし（正解）: 上位を捨てる意図をマスクと明示変換で示す */
    r = (uint8)(x & 0xFFU);

    /* 指摘あり: uint16 を uint8 へ入れている（10.3） */
    r = x;

    return r;
}

uint16 Pat_Add8To16(uint8 a, uint8 b)
{
    uint16 r;

    /* 指摘なし（正解）: 足す前に広い型へ */
    r = (uint16)((uint16)a + (uint16)b);

    /* 指摘あり: uint8 の足し算を uint16 へ入れている（10.6） */
    r = a + b;

    return r;
}

uint16 Pat_Sub8To16(uint8 a, uint8 b)
{
    uint16 d;

    /* 指摘なし（正解）: 引く前に広い型へ */
    d = (uint16)((uint16)a - (uint16)b);

    /* 指摘あり: uint8 の引き算を uint16 へ入れている（10.6） */
    d = a - b;

    return d;
}

uint16 Pat_Add8Add16(uint8 a, uint8 b, uint16 c)
{
    uint16 r;

    /* 指摘なし（正解）: 全部 uint16 にそろえてから足す */
    r = (uint16)(((uint16)a + (uint16)b) + c);

    /* 指摘あり: 途中の式（a + b）より広い c を足している（10.7） */
    r = (a + b) + c;

    return r;
}

uint32 Pat_Mul16To32(uint16 a, uint16 b)
{
    uint32 r;

    /* 指摘なし（正解）: 掛ける前に uint32 へ */
    r = (uint32)a * (uint32)b;

    /* 指摘あり: uint16 の掛け算を uint32 へ入れている（10.6）。65535 * 65535 は int であふれる */
    r = a * b;

    return r;
}

uint32 Pat_Add16To32(uint16 a, uint16 b)
{
    uint32 r;

    /* 指摘なし（正解）: 足す前に uint32 へ */
    r = (uint32)a + (uint32)b;

    /* 指摘あり: uint16 の足し算を uint32 へ入れている（10.6） */
    r = a + b;

    return r;
}

uint8 Pat_Compare16Const(uint16 x)
{
    uint8 r = 0U;

    /* 指摘なし（正解）: 定数に U */
    if (x > 1000U)
    {
        r = 1U;
    }

    /* 指摘あり: uint16 と符号付き定数の比較（10.4） */
    if (x > 1000)
    {
        r = 1U;
    }

    return r;
}

uint16 Pat_Mask16(uint16 x)
{
    uint16 r;

    /* 指摘なし（正解）: マスクの定数に U。結果は明示変換 */
    r = (uint16)(x & 0x00FFU);

    /* 指摘あり: マスクの定数が符号付き（10.4） */
    r = x & 0x00FF;

    return r;
}

uint16 Pat_CastWideSum(uint8 a, uint8 b)
{
    uint16 w;

    /* 指摘なし（正解）: キャストは計算の前（オペランド）に付ける */
    w = (uint16)((uint16)a + (uint16)b);

    /* 指摘あり: 足し算の後にキャストしても、広い型での計算にならない（10.8） */
    w = (uint16)(a + b);

    return w;
}

uint8 Pat_Sum8Narrow(uint8 a, uint16 b)
{
    uint8 r;

    /* 指摘なし（正解）: 結果を狭い型へ入れる意図を明示変換で示す */
    r = (uint8)((uint16)a + b);

    /* 指摘あり: uint8 と uint16 の和を uint8 へ入れている（10.3） */
    r = a + b;

    return r;
}

#endif /* MISRA_PRACTICE_ENABLE */
