/**
 * \file    Fake_Rte.h
 * \brief   Fake_Rte.c が返す、センサ値（エンジン回転数・冷却水温）をテストから指定するための宣言
 * \details Dcm の RoutineControl（EngineHealthCheck）の合否判定のように、Rte 経由で読むセンサ値によって
 *          結果が変わる処理を、テストが確かめられるようにする。値の既定は 0（正常）。
 *          テストは、他のテストへ影響を残さないよう、SetUp()/TearDown() で FakeRte_Reset() を呼ぶこと。
 */
#ifndef FAKE_RTE_H
#define FAKE_RTE_H

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Rte_Type.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================
 * Global Variables
 * ====================================================================== */

/** Rte_Read_SpeedSensor_EngineSpeed() が返す値（既定 0）。 */
extern EngineSpeed_t FakeRte_EngineSpeed;

/** Rte_Read_TempSensor_CoolantTemp() が返す値（既定 0）。 */
extern CoolantTemp_t FakeRte_CoolantTemp;

/* ======================================================================
 * Functions
 * ====================================================================== */

/** 指定値を既定（0）へ戻す。 */
void FakeRte_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* FAKE_RTE_H */
