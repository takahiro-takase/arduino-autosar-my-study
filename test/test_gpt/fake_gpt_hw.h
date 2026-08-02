/**
 * \file    fake_gpt_hw.h
 * \brief   Gpt_Hw.h（Renesas RA FspTimer 境界）のテスト用フェイク実装の宣言
 * \details Gpt.c のロジックだけを検証したいので、実 HW（FspTimer）は使わず、
 *          呼び出し回数・引数を記録するだけのフェイクに差し替える。
 *          Gpt_OnTick() は Gpt.c 自身が実装する側であり、本フェイクでは
 *          扱わない（テストコードから直接呼んで「1 tick 経過」をシミュレート
 *          する。Gpt.c が Gpt_OnTick() を普通の呼び出し可能関数として公開して
 *          いる設計のおかげで、実割り込みなしに状態機械を駆動できる）。
 */
#ifndef FAKE_GPT_HW_H
#define FAKE_GPT_HW_H

#include "Gpt_PBCfg.h"

#ifdef __cplusplus
extern "C" {
#endif

extern uint32          FakeGptHw_StartCount;
extern uint32          FakeGptHw_StopCount;
extern Gpt_ChannelType FakeGptHw_LastStartChannel;
extern uint32          FakeGptHw_LastTickFrequencyHz;

/** 1 にすると次回以降の Gpt_Hw_StartTimer() が E_NOT_OK を返す
 *  （HW 側の確保失敗をシミュレートする）。 */
extern uint8 FakeGptHw_StartShouldFail;

/** 各テストケースの開始時に呼び、記録・フラグをすべてクリアする。 */
void FakeGptHw_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* FAKE_GPT_HW_H */
