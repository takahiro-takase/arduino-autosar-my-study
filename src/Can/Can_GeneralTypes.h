#ifndef CAN_GENERAL_TYPES_H
#define CAN_GENERAL_TYPES_H

#include "Platform_Types.h"
#include "ComStack_Types.h"

typedef uint32 Can_IdType;

// ハードウェア送信オブジェクト（HOH: Hardware Object Handle）の識別子
// MCP2515 は TX バッファを 3 つ持つが、ここでは簡略化して uint8 で管理
typedef uint8 Can_HwHandleType;

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

typedef struct
{
    Can_IdType id;
    uint8 length;
    uint8* sdu;
} Can_PduType;

#endif
