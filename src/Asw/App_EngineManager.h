#ifndef APP_ENGINE_MANAGER_H
#define APP_ENGINE_MANAGER_H

#include "Std_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    ENGINE_STATE_OFF      = 0,
    ENGINE_STATE_STARTING = 1,
    ENGINE_STATE_RUNNING  = 2,
    ENGINE_STATE_FAULT    = 3
} EngineState_t;

void          App_EngineManager_Init(void);
void          App_EngineManager_Run(void);
EngineState_t App_EngineManager_GetState(void);

#ifdef __cplusplus
}
#endif

#endif
