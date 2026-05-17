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

/* SWS_Can_00404 */
typedef enum
{
    CAN_T_START  = 0x01, /* CAN_CS_STOPPED -> CAN_CS_STARTED */
    CAN_T_STOP   = 0x02, /* CAN_CS_STARTED -> CAN_CS_STOPPED */
    CAN_T_SLEEP  = 0x03, /* CAN_CS_STOPPED -> CAN_CS_SLEEP   */
    CAN_T_WAKEUP = 0x04  /* CAN_CS_SLEEP   -> CAN_CS_STOPPED */
} Can_StateTransitionType;

typedef struct
{
    PduIdType  swPduHandle; // CanIf が書き込む PDU ID（TxConfirmation で返ってくる）
    Can_IdType id;
    uint8      length;
    uint8*     sdu;
} Can_PduType;

#endif
