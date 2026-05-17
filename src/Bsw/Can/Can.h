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

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint32 filterId;
    uint32 mask;
} Can_FilterConfigType;

typedef uint8 Can_CrystalFreqType;
#define CAN_CRYSTAL_8MHZ  ((Can_CrystalFreqType)8U)
#define CAN_CRYSTAL_16MHZ ((Can_CrystalFreqType)16U)
#define CAN_CRYSTAL_20MHZ ((Can_CrystalFreqType)20U)

typedef struct
{
    Can_FilterConfigType filter;
    uint8               csPin;
    uint8               intPin;
    uint32              baudrate;
    Can_CrystalFreqType crystalFreq;
} Can_ConfigType;

void           Can_Init(const Can_ConfigType* Config);
Can_ReturnType Can_SetControllerMode(uint8 Controller, Can_StateTransitionType Transition);
Can_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType* PduInfo);
void           Can_MainFunction_Read(void);
void           Can_Isr(void);

#ifdef __cplusplus
}
#endif

#endif
