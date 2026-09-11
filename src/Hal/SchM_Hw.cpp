/**
 * \file    SchM_Hw.cpp
 * \brief   SchM ハードウェア依存層 実装 (グローバル割り込み制御)
 * \details Arduino の noInterrupts()/interrupts() を直接呼び出す唯一のファイル。
 *          Renesas RA では __enable_irq()/__disable_irq() に展開されるマクロで、
 *          コアの単一割り込み優先度レベルを前提に「これ以上プリエンプトされない
 *          区間」を作る、最も単純な排他制御プリミティブ（実車 AUTOSAR OS の
 *          SuspendAllInterrupts()/ResumeAllInterrupts() に相当）。
 *
 *          本プロジェクトで割り込みコンテキストとメインループの両方から
 *          アクセスされる共有変数（Can.c の Can_RxIrqPending 等）を保護する
 *          ために SchM.h の排他エリアマクロから呼び出される。
 *
 *          ネストカウンタ（2026-09 追加）: [SWS_Rte_07252]/
 *          [SWS_Rte_CONSTR_09047] は「異なる排他エリアであればネスト呼び出しを
 *          許容し、進入した順と逆順で退出すべき」と規定する。以前は単純な
 *          noInterrupts()/interrupts() のペアで、異なる排他エリアがネストして
 *          呼ばれた場合（例: A に進入中に B に進入・退出すると、B の退出が
 *          無条件で割り込みを再有効化してしまい、まだ有効なはずの A の保護が
 *          破られる）に対応できていなかった。全排他エリアが本ファイルの
 *          この 2 関数だけを経由する（ファイル冒頭の説明の通り、
 *          noInterrupts()/interrupts() を直接呼ぶのはここだけ）という
 *          既存の設計上の不変条件を利用し、進入深さを数えるだけの単純な
 *          カウンタで対応する（最外周の進入時のみ実際に割り込みを禁止し、
 *          最外周の退出時のみ再有効化する）。Enter/Exit は本プロジェクトでは
 *          メインループのタスクコンテキストからのみ呼ばれ（各排他エリアの
 *          定義コメント参照。ISR 自身は単一割り込み優先度のため自己再入せず
 *          Enter/Exit で囲む必要がない）、割り込みコンテキストとの競合は
 *          発生しないため、単純なインクリメント/デクリメントで安全に扱える。
 *          現状どのコードパスも異なる排他エリアを実際にネストしていないため
 *          潜在的な欠陥だったが、将来ネストが発生しても正しく動作するように
 *          修正した。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include <Arduino.h>
#include <stdint.h>
#include "SchM_Hw.h"

extern "C" {

static uint8_t SchM_Hw_NestDepth = 0U;

void SchM_Hw_EnterExclusiveArea(void)
{
    if (SchM_Hw_NestDepth == 0U)
    {
        noInterrupts();
    }
    SchM_Hw_NestDepth++;
}

void SchM_Hw_ExitExclusiveArea(void)
{
    SchM_Hw_NestDepth--;
    if (SchM_Hw_NestDepth == 0U)
    {
        interrupts();
    }
}

} /* extern "C" */
