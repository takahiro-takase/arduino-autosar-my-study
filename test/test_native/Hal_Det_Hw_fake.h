/**
 * \file    Hal_Det_Hw_fake.h
 * \brief   Det_Hw.h（Arduino Serial 出力）のテスト用スパイ実装の宣言
 * \details Det.c のロジック（レベル抑制・vsnprintf でのメッセージ整形）は
 *          そのまま検証したいので、実 HW（Arduino Serial）に依存する
 *          Det_Hw.cpp のみをフェイクに差し替える。直近の
 *          Det_Hw_PrintDetError() 呼び出しを記録するスパイを提供し、
 *          テストから「期待したエラーコードが報告されたか」を検証できる
 *          ようにする。
 */
#ifndef HAL_DET_HW_FAKE_H
#define HAL_DET_HW_FAKE_H

#include "Std_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

extern uint16 FakeDetHw_LastModuleId;
extern uint8  FakeDetHw_LastApiId;
extern uint8  FakeDetHw_LastErrorId;
extern uint32 FakeDetHw_ReportCount;

/** 各テストケースの開始時に呼び、直近の記録をクリアする。 */
void FakeDetHw_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_DET_HW_FAKE_H */
