#ifndef APP_ENGINE_MANAGER_H
#define APP_ENGINE_MANAGER_H

#include "Std_Types.h"

// -------------------------------------------------------
// エンジン管理 SW-C の状態型
//
// AUTOSAR では ImplementationDataType として ARXML 定義される。
// SW-C はこの型だけで状態を管理し、RTE API 経由で外部と通信する。
// -------------------------------------------------------
typedef enum
{
    ENGINE_STATE_OFF      = 0,
    ENGINE_STATE_STARTING = 1,
    ENGINE_STATE_RUNNING  = 2,
    ENGINE_STATE_FAULT    = 3
} EngineState_t;

// -------------------------------------------------------
// SW-C 公開 API
//
// Init   : setup() から 1 回だけ呼ぶ
// Run    : RTE スケジューラが周期呼び出しする Runnable
// GetState: デバッグ用（RTE を介さない直接参照は本来 NG）
// -------------------------------------------------------
void          App_EngineManager_Init(void);
void          App_EngineManager_Run(void);
EngineState_t App_EngineManager_GetState(void);

#endif
