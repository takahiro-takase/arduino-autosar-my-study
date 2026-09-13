/**
 * \file    Bsw_BswM_fake.h
 * \brief   BswM.h（ComM のモード変化通知先）のテスト用スパイ実装の宣言
 * \details ComM.c はチャネルモードが変化するたびに BswM_ComM_CurrentMode()
 *          を呼ぶ。本テストの対象は Nm↔CanSM↔ComM の協調スリープ状態遷移
 *          ロジックであり、BswM 側のルールエンジンまでは対象に含めないため、
 *          呼び出し回数・引数を記録するだけのフェイクに差し替える
 *          （Bsw_ComM_fake.h と同じ境界の考え方）。
 */
#ifndef BSW_BSWM_FAKE_H
#define BSW_BSWM_FAKE_H

#include "Std_Types.h"
#include "ComM.h"

#ifdef __cplusplus
extern "C" {
#endif

extern uint32 FakeBswM_ComM_CurrentModeCount;

/** 直近の呼び出しの引数。 */
extern uint8         FakeBswM_LastChannel;
extern ComM_ModeType FakeBswM_LastMode;

/** 各テストケースの開始時に呼び、記録をすべて初期状態に戻す。 */
void FakeBswM_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* BSW_BSWM_FAKE_H */
