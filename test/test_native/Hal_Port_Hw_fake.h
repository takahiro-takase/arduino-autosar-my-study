/**
 * \file    Hal_Port_Hw_fake.h
 * \brief   Port_Hw.h（Arduino pinMode 境界）のテスト用フェイク実装の宣言
 * \details Port.c のロジック（Port_RefreshPortDirection が全ピンへ設定方向を
 *          再適用すること）を検証するため、ピンごとに直近に設定された方向と
 *          呼び出し回数を記録する。
 */
#ifndef HAL_PORT_HW_FAKE_H
#define HAL_PORT_HW_FAKE_H

#include "Port.h"

#ifdef __cplusplus
extern "C" {
#endif

extern uint32 FakePortHw_SetPinDirectionCount;

/** 各テストケースの開始時に呼び、記録をすべてクリアする。 */
void FakePortHw_Reset(void);

/** 指定ピンへ直近に設定された方向を返す（一度も設定されていなければ 0xFF）。 */
Port_PinDirectionType FakePortHw_GetLastDirection(Port_PinType pin);

#ifdef __cplusplus
}
#endif

#endif /* HAL_PORT_HW_FAKE_H */
