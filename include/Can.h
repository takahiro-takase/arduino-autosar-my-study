#ifndef CAN_H
#define CAN_H

#include <stdint.h>

// AUTOSAR 風 Config（簡易版）
typedef struct
{
    uint32_t filterId;     // 受信したい ID
    uint32_t mask;         // マスク
} Can_ConfigType;

void Can_Init(const Can_ConfigType* Config);
void Can_SetControllerMode(uint8_t mode);
void Can_Write(uint32_t id, uint8_t dlc, uint8_t* data);
void Can_MainFunction_Read(void);
void Can_Isr(void);

#endif
