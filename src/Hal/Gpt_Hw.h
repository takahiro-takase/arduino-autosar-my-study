/**
 * \file    Gpt_Hw.h
 * \brief   Gpt ハードウェア依存層 内部インタフェース
 * \details Gpt.c (純粋 C, AUTOSAR API 層) と、実際の HW タイマ（Renesas RA
 *          FspTimer）との境界を定義する。Gpt.c はこのヘッダ経由でのみ
 *          HW タイマを操作し、FspTimer / Arduino 固有のヘッダを直接知らない。
 *          本ヘッダは Gpt.c と Gpt_Hw.cpp 以外からインクルードしないこと。
 *
 *          呼び出し方向:
 *            Gpt.c        -> Gpt_Hw_Init/DeInit/StartTimer/StopTimer
 *                             （Wdg.c -> Wdg_Hw と同じ、Bsw から Hal への委譲）
 *            Gpt_Hw.cpp   -> Gpt_OnTick
 *                             （実 HW 割り込みから Bsw の状態機械へ通知する
 *                             逆方向の呼び出し）。
 *
 *          逆方向の呼び出しが必要な理由（Can_Isr() 方式との違い）:
 *            Can.c は attachInterrupt() のコールバック型がもともと
 *            `void(void)` という素の C 関数ポインタのため、ISR 本体を
 *            Can.c 側に置いて Can_Hw_AttachRxIsr() へ渡す設計にできる
 *            （Can_Hw.h/Can_Hw.cpp 参照）。しかし FspTimer のコールバック型
 *            `void(*)(timer_callback_args_t*)` は FSP 固有の構造体を
 *            引数に取るため、Gpt.c がこの型を直接実装すると Arduino API を
 *            知ってしまうことになり境界規約に反する。そのため ISR 本体
 *            （FSP 型を扱うトランポリン）は Gpt_Hw.cpp 側に置き、
 *            そこから素の `void Gpt_OnTick(Gpt_ChannelType)` 経由で
 *            Gpt.c の状態機械を呼び戻す。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef GPT_HW_H
#define GPT_HW_H

#include "Gpt_PBCfg.h"  /* GPT_CHANNEL_COUNT, Gpt_ChannelType 等 (Gpt_Cfg.h を再エクスポート) */

#ifdef __cplusplus
extern "C" {
#endif

/** GPT_CHANNEL_COUNT 本分の内部状態をクリアする。実 HW はまだ何も起動しない
 *  （実際の起動は Gpt_Hw_StartTimer() 呼び出し時）。 */
void Gpt_Hw_Init(void);

/** 全チャネルの HW タイマを停止する。Gpt_DeInit() から呼ばれる時点では
 *  running のチャネルは存在しない前提（Gpt.c 側で GPT_E_BUSY ガード済み）。 */
void Gpt_Hw_DeInit(void);

/**
 * \brief   指定チャネルの HW タイマを TickFrequencyHz で周期起動する。
 *
 * \details 目標時間 (Value) との比較は行わない。TickFrequencyHz どおりの
 *          周期で Gpt_OnTick(Channel) を呼び続けるだけの単純な周期タイマ
 *          として動作する（目標到達判定は Gpt.c 側のソフトウェア比較。
 *          Gpt.c 冒頭のコメント参照）。
 *
 * \retval  E_OK      HW タイマの確保・起動に成功。
 * \retval  E_NOT_OK  空きの HW タイマチャネルが無い、または FspTimer の
 *                     begin/open/start のいずれかが失敗した。
 */
Std_ReturnType Gpt_Hw_StartTimer(Gpt_ChannelType Channel, uint32 TickFrequencyHz);

/** 指定チャネルの HW タイマを停止する（Gpt_OnTick() の ONESHOT 自動停止からも
 *  呼ばれるため、ISR コンテキストでも安全に呼べる処理に留めること）。 */
void Gpt_Hw_StopTimer(Gpt_ChannelType Channel);

/**
 * \brief   Gpt.c が実装する、チャネルの 1 tick 経過ごとの通知関数。
 *
 * \details ISR コンテキストから呼ばれる（本ヘッダ冒頭のコメント参照）。
 */
void Gpt_OnTick(Gpt_ChannelType Channel);

#ifdef __cplusplus
}
#endif

#endif /* GPT_HW_H */
