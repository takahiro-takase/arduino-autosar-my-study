/**
 * \file    App_TestMsg.c
 * \brief   テスト用メッセージ (TestMsg, CAN 0x300) 送信 SW-C 実装
 * \details 1 秒ごとに +1 するカウンタを Rte_Write_TestMsg_TestCounter() で
 *          書き込む。詳細は App_TestMsg.h を参照。
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

#include "App_TestMsg.h"
#include "Rte.h"

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
void App_TestMsg_Run(void)
{
    /* TestMsg.TestCounter へ書き込む値（uint8 のため 255 の次は 0） */
    static uint8 s_TestCounter = 0U;

    s_TestCounter++;
    (void)Rte_Write_TestMsg_TestCounter(s_TestCounter);
}
