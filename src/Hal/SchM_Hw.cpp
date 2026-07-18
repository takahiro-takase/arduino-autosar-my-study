/**
 * \file    SchM_Hw.cpp
 * \brief   SchM ハードウェア依存層 実装 (グローバル割り込み制御)
 * \details Arduino の noInterrupts()/interrupts() を直接呼び出す唯一のファイル。
 *          AVR では sei()/cli()、Renesas RA では __enable_irq()/__disable_irq()
 *          に展開されるマクロで、いずれもコアの単一割り込み優先度レベルを
 *          前提に「これ以上プリエンプトされない区間」を作る、最も単純な
 *          排他制御プリミティブ（実車 AUTOSAR OS の
 *          SuspendAllInterrupts()/ResumeAllInterrupts() に相当）。
 *
 *          本プロジェクトで割り込みコンテキストとメインループの両方から
 *          アクセスされる共有変数（Can.c の Can_RxIrqPending 等）を保護する
 *          ために SchM.h の排他エリアマクロから呼び出される。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include <Arduino.h>
#include "SchM_Hw.h"

extern "C" {

void SchM_Hw_EnterExclusiveArea(void)
{
    noInterrupts();
}

void SchM_Hw_ExitExclusiveArea(void)
{
    interrupts();
}

} /* extern "C" */
