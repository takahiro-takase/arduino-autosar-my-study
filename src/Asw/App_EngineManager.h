#ifndef APP_ENGINE_MANAGER_H
#define APP_ENGINE_MANAGER_H

#include "Rte_Type.h"

#ifdef __cplusplus
extern "C" {
#endif

void          App_EngineManager_Init(void);
void          App_EngineManager_Run(void);
EngineState_t App_EngineManager_GetState(void);

#ifdef __cplusplus
}
#endif

#endif
