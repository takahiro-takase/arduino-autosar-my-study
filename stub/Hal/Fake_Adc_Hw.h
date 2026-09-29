/**
 * \file    Fake_Adc_Hw.h
 * \brief   Adc_Hw.h（Arduino analogRead() 境界）のテスト用フェイク実装の宣言
 * \details Adc.c/IoHwAb.c のロジックだけを検証したいので、実 HW（analogRead()）
 *          は使わず、呼び出し回数・戻り値を記録するだけのフェイクに差し替える。
 */
#ifndef FAKE_ADC_HW_H
#define FAKE_ADC_HW_H

#include "Std_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

extern uint32 FakeAdcHw_ReadChannelCount;
extern uint8  FakeAdcHw_LastChannel;

/** 次回の Adc_Hw_ReadChannel() が返す値（テストが差し替える）。 */
extern uint16 FakeAdcHw_ReturnValue;

/** 各テストケースの開始時に呼び、記録・戻り値設定をすべて初期状態に戻す。 */
void FakeAdcHw_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* FAKE_ADC_HW_H */
