/**
 * \file    WdgIf_fake.h
 * \brief   WdgIf.h のテスト用フェイク実装の宣言（WdgM.c から見た下位ディスパッチ層）
 * \details WdgM.c は WdgIf_SetMode/WdgIf_SetTriggerCondition のみを呼ぶ。WdgIf を
 *          本物でリンクすると Wdg→Wdg_Hw（実 HW の IWDT レジスタ操作）まで
 *          芋づる式に必要になってしまうため、HAL 層のフェイクと同じ考え方で、
 *          WdgM.c が実際に呼ぶこの 2 関数だけを呼び出し記録付きのフェイクに
 *          差し替える（WdgIf_GetVersionInfo は WdgM.c から呼ばれないため未実装）。
 */
#ifndef WDGIF_FAKE_H
#define WDGIF_FAKE_H

#include "WdgIf.h"

#ifdef __cplusplus
extern "C" {
#endif

extern uint32 FakeWdgIf_SetModeCount;
extern uint32 FakeWdgIf_SetTriggerConditionCount;

/** 直近の呼び出し引数。 */
extern WdgIf_DeviceType FakeWdgIf_LastSetModeDevice;
extern WdgIf_ModeType   FakeWdgIf_LastSetModeMode;
extern WdgIf_DeviceType FakeWdgIf_LastTriggerDevice;
extern uint16           FakeWdgIf_LastTriggerTimeout;

/** WdgIf_SetMode() の戻り値。既定は E_OK。 */
extern Std_ReturnType FakeWdgIf_SetModeReturn;

/** 各テストケースの開始時に呼び、記録をすべてクリアする。 */
void FakeWdgIf_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* WDGIF_FAKE_H */
