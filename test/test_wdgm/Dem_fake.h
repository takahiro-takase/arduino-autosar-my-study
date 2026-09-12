/**
 * \file    Dem_fake.h
 * \brief   Dem.h のテスト用フェイク実装の宣言（WdgM.c から見た通知先モジュール）
 * \details WdgM.c は [SWS_WdgM_00129] 対応として Dem_SetEventStatus() のみを
 *          呼ぶ。Dem を本物でリンクすると NvM（EEPROM 永続化）まで芋づる式に
 *          必要になってしまうため、WdgIf_fake.h と同じ考え方で、WdgM.c が
 *          実際に呼ぶこの1関数だけを呼び出し記録付きのフェイクに差し替える
 *          （2026-09 追加）。
 */
#ifndef DEM_FAKE_H
#define DEM_FAKE_H

#include "Dem.h"

#ifdef __cplusplus
extern "C" {
#endif

extern uint32 FakeDem_SetEventStatusCount;

/** 直近の呼び出し引数。 */
extern Dem_EventIdType     FakeDem_LastEventId;
extern Dem_EventStatusType FakeDem_LastEventStatus;

/** 各テストケースの開始時に呼び、記録をすべてクリアする。 */
void FakeDem_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* DEM_FAKE_H */
