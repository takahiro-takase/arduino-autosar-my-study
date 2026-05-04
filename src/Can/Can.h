#ifndef CAN_H
#define CAN_H

#include "Platform_Types.h"
#include "ComStack_Types.h"
#include "Can_GeneralTypes.h"

// AUTOSAR 風 Config（簡易版）
typedef struct
{
    uint32 filterId; // 受信したい ID
    uint32 mask;     // マスク
} Can_FilterConfigType;

typedef struct
{
    Can_FilterConfigType filter;
    uint8 csPin;
    uint32 baudrate;
} Can_ConfigType;

void Can_Init(const Can_ConfigType* Config);
void Can_SetControllerMode(Can_ControllerStateType mode);
Can_ReturnType Can_Write(uint32 id, uint8 dlc, const uint8* data);
void Can_MainFunction_Read(void);
void Can_Isr(void);

#endif