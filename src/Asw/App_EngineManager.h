/**
 * \file    App_EngineManager.h
 * \brief   エンジンマネージャ SW-C 公開インタフェース (AUTOSAR ASW)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
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
