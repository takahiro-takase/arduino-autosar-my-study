/**
 * \file    Can.h
 * \brief   CAN ドライバ 公開インタフェース (AUTOSAR SWS_Can 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef CAN_H
#define CAN_H

#include "Platform_Types.h"
#include "ComStack_Types.h"
#include "Can_GeneralTypes.h"
#include "Can_Cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

void           Can_Init(const Can_ConfigType* Config);
Can_ReturnType Can_SetControllerMode(uint8 Controller, Can_StateTransitionType Transition);
Can_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType* PduInfo);
void           Can_MainFunction_Read(void);
void           Can_MainFunction_Write(void);
void           Can_MainFunction_BusOff(void);
void           Can_MainFunction_Wakeup(void);
void           Can_Isr(void);

#ifdef __cplusplus
}
#endif

#endif
