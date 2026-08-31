/**
 * \file    Can_GeneralTypes.h
 * \brief   CAN 汎用型定義 (AUTOSAR Can_GeneralTypes)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
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

/**
 * \brief   CAN コントローラのエラー状態（CanIf_GetControllerErrorState()
 *          [SWS_CANIF_91001] が使用する型。CAN プロトコルの TEC/REC に基づく
 *          標準的な3状態）。
 * \details 値は MCP2515 の EFLG レジスタから Can_Hw_GetErrorState() が導出する
 *          （bit5=Bus-Off、bit4=TX Error-Passive、bit3=RX Error-Passive
 *          （TEC/REC いずれか一方でも閾値超過すれば Passive）、いずれも
 *          立っていなければ Active。Can_Hw.cpp 参照）。実 AUTOSAR
 *          SWS_CANDriver 4.3.1 は本型を
 *          使う `Can_GetControllerErrorState()` 相当の Service を規定していない
 *          （CanIf 側のみが定義された API）ため、本プロジェクトの
 *          `Can_GetControllerErrorState()` は AUTOSAR 非標準の拡張である。
 */
typedef enum
{
    CAN_ERRORSTATE_ACTIVE = 0,  /**< Error Active（正常）        */
    CAN_ERRORSTATE_PASSIVE,     /**< Error Passive（送信制限あり） */
    CAN_ERRORSTATE_BUSOFF       /**< Bus-Off（送信不能）          */
} Can_ErrorStateType;

/* SWS_Can_00417 */
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

/* SWS_Can_00496 */
typedef struct
{
    Can_IdType       CanId;
    Can_HwHandleType Hoh;
    uint8            ControllerId;
} Can_HwType;

#endif
