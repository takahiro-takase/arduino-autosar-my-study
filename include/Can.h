#ifndef CAN_H
#define CAN_H

#include <stdint.h>

typedef enum {
    CAN_CS_UNINIT = 0,
    CAN_CS_STOPPED,
    CAN_CS_STARTED,
    CAN_CS_SLEEP
} Can_ControllerStateType;

// AUTOSAR 風 Config（簡易版）
typedef struct
{
    uint32_t filterId;     // 受信したい ID
    uint32_t mask;         // マスク
} Can_FilterConfigType;

typedef struct {
    Can_FilterConfigType filter;
    uint8_t csPin;
    uint32_t baudrate;
} Can_ConfigType;

void Can_Init(const Can_ConfigType* Config);
void Can_SetControllerMode(Can_ControllerStateType mode);
void Can_Write(uint32_t id, uint8_t dlc, const uint8_t* data);
void Can_MainFunction_Read(void);
void Can_Isr(void);

#endif