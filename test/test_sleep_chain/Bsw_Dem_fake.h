/**
 * \file    Bsw_Dem_fake.h
 * \brief   Dem.h（CanSM の Bus-Off 通信路報告先）のテスト用スパイ実装の宣言
 * \details CanSM.c は FULL_COM 復帰のたびに `Dem_ReportErrorStatus
 *          (DEM_EVENT_CAN_BUSOFF, ...)` を呼ぶ。本テストの対象はスリープ/
 *          ウェイクアップの状態遷移ロジックであり、Dem 側のデバウンス・
 *          DTC 確定・EEPROM 永続化までは対象に含めないため、呼び出し回数・
 *          引数を記録するだけのフェイクに差し替える
 *          （Bsw_ComM_fake.h と同じ境界の考え方）。
 */
#ifndef BSW_DEM_FAKE_H
#define BSW_DEM_FAKE_H

#include "Std_Types.h"
#include "Dem.h"

#ifdef __cplusplus
extern "C" {
#endif

extern uint32              FakeDem_ReportErrorStatusCount;

/** 直近の Dem_ReportErrorStatus() 呼び出しの引数。 */
extern Dem_EventIdType     FakeDem_LastEventId;
extern Dem_EventStatusType FakeDem_LastEventStatus;

/** 各テストケースの開始時に呼び、記録をすべて初期状態に戻す。 */
void FakeDem_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* BSW_DEM_FAKE_H */
