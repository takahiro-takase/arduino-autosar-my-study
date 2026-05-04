#ifndef CAN_GENERAL_TYPES_H
#define CAN_GENERAL_TYPES_H

#include <stdint.h>

typedef enum
{
    CAN_OK = 0,
    CAN_NOT_OK,
    CAN_BUSY
} Can_ReturnType;

typedef enum
{
    CAN_CS_UNINIT = 0,
    CAN_CS_STOPPED,
    CAN_CS_STARTED,
    CAN_CS_SLEEP
} Can_ControllerStateType;

#endif
