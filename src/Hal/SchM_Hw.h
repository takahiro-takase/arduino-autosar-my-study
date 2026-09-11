/**
 * \file    SchM_Hw.h
 * \brief   SchM ハードウェア依存層 内部インタフェース (グローバル割り込み制御)
 * \details SchM.c（純粋 C）から呼び出せる、Arduino のグローバル割り込み
 *          制御 (noInterrupts/interrupts) への唯一の窓口。
 *          Can_Hw.cpp・Port_Hw.cpp と同じ方針で C/C++ 境界を一箇所に集約する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef SCHM_HW_H
#define SCHM_HW_H

#ifdef __cplusplus
extern "C" {
#endif

/** グローバル割り込みを無効化する（排他エリア開始）。異なる排他エリアが
 *  ネストして呼ばれてもネストカウンタにより正しく動作する（最外周の
 *  進入時のみ実際に割り込みを禁止する、[SWS_Rte_07252]/
 *  [SWS_Rte_CONSTR_09047]。SchM_Hw.cpp 参照）。 */
void SchM_Hw_EnterExclusiveArea(void);

/** グローバル割り込みを有効化する（排他エリア終了）。ネストカウンタが
 *  0 に戻った時（最外周の退出時）のみ実際に割り込みを再有効化する
 *  （SchM_Hw.cpp 参照）。 */
void SchM_Hw_ExitExclusiveArea(void);

#ifdef __cplusplus
}
#endif

#endif /* SCHM_HW_H */
