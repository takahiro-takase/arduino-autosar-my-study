/**
 * \file    Hal_Dio_Hw_fake.h
 * \brief   Dio_Hw.h（Arduino digitalRead/digitalWrite 境界）のテスト用フェイク実装の宣言
 * \details Dio.c のロジック（Dio_FlipChannel の読み取り→反転→書き込み）を
 *          検証するには、書き込んだ値がそのまま読み取れる実 GPIO 相当の状態を
 *          持つ必要がある。そのため呼び出し回数だけを記録する他モジュールの
 *          フェイクとは異なり、チャネルごとの現在レベルを保持する簡易メモリ
 *          モデルとして実装する。
 */
#ifndef HAL_DIO_HW_FAKE_H
#define HAL_DIO_HW_FAKE_H

#include "Dio.h"

#ifdef __cplusplus
extern "C" {
#endif

extern uint32 FakeDioHw_WriteCount;

/** 各テストケースの開始時に呼び、全チャネルを DIO_LOW にリセットする。 */
void FakeDioHw_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_DIO_HW_FAKE_H */
