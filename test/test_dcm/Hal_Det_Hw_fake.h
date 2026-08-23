/**
 * \file    Hal_Det_Hw_fake.h
 * \brief   Det_Hw.h（Arduino Serial 出力）のテスト用スパイ実装の宣言
 * \details test/test_chain/Hal_Det_Hw_fake.h と同一内容（env ごとに別
 *          バイナリのため複製）。Det.c のロジック（レベル抑制・vsnprintf
 *          でのメッセージ整形）はそのまま検証したいので、実 HW（Arduino
 *          Serial）に依存する Det_Hw.cpp のみをフェイクに差し替える。
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

/** 1 = DET_LOG_VERBOSE が有効でもログ出力を抑制する。Init()/DeInit() 等の
 *  ノイズを隠したい区間（SetUp/TearDown）で立てる。既定値は 1（抑制）で、
 *  各テストの実行 (Act) 区間だけ 0 にして使う想定。 */
extern uint8 FakeDetHw_LogSuppressed;

/** 各テストケースの開始時に呼び、直近の記録をクリアする。 */
void FakeDetHw_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_DET_HW_FAKE_H */
