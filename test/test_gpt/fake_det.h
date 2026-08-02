/**
 * \file    fake_det.h
 * \brief   Det.h（DET_LOGx / Det_ReportError）のテスト用スパイ実装の宣言
 * \details Gpt.c は Det.h をリンク時に必要とするが、実装（Log_Write/
 *          Det_ReportError、実体は src/Bsw/Det/Det.c）は Hal/Det_Hw.cpp
 *          経由で Arduino Serial に依存するため、native 環境ではリンクできない。
 *          本ファイルはその代わりに、直近の Det_ReportError() 呼び出しを
 *          記録するだけのスパイを提供し、テストから
 *          「期待したエラーコードが報告されたか」を検証できるようにする。
 */
#ifndef FAKE_DET_H
#define FAKE_DET_H

#include "Std_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

extern uint16 FakeDet_LastModuleId;
extern uint8  FakeDet_LastApiId;
extern uint8  FakeDet_LastErrorId;
extern uint32 FakeDet_ReportCount;

/** 各テストケースの開始時に呼び、直近の記録をクリアする。 */
void FakeDet_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* FAKE_DET_H */
