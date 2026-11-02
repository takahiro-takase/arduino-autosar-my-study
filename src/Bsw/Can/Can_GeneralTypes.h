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

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Platform_Types.h"
#include "Std_Types.h"
#include "ComStack_Types.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

typedef uint32 Can_IdType;

// ハードウェア送信オブジェクト（HOH: Hardware Object Handle）の識別子
// MCP2515 は TX バッファを 3 つ持つが、ここでは簡略化して uint8 で管理
typedef uint8 Can_HwHandleType;

/* Can_Write() の戻り値（Std_ReturnType）に加わる値。TX ハードウェアバッファに空きがない、
 * または再入できない先行呼び出しがあるとき（[SWS_Can_00213/00214]）。4.3.1 で Can_ReturnType
 * （CAN_OK/CAN_NOT_OK/CAN_BUSY）は廃止され、E_OK/E_NOT_OK に統一された。 */
#define CAN_BUSY  ((Std_ReturnType)0x02U)

/* [SWS_Can_91014 等] 値は仕様どおり（STARTED=0x01、STOPPED=0x02）。 */
typedef enum
{
    CAN_CS_UNINIT  = 0x00,
    CAN_CS_STARTED = 0x01,
    CAN_CS_STOPPED = 0x02,
    CAN_CS_SLEEP   = 0x03
} Can_ControllerStateType;

/**
 * \brief   CAN コントローラのエラー状態（CanIf_GetControllerErrorState()
 *          [SWS_CANIF_91001] が使用する型。CAN プロトコルの TEC/REC に基づく
 *          標準的な3状態）。
 * \details 値は MCP2515 の EFLG レジスタから Can_Hw_GetErrorState() が導出する
 *          （bit5=Bus-Off、bit4=TX Error-Passive、bit3=RX Error-Passive
 *          （TEC/REC いずれか一方でも閾値超過すれば Passive）、いずれも
 *          立っていなければ Active。Can_Hw.cpp 参照）。本型は SWS_CAN の 4.2.2、
 *          これを使う `Can_GetControllerErrorState()` は 4.3.0 で追加された（Can.c 参照）。
 */
typedef enum
{
    CAN_ERRORSTATE_ACTIVE = 0,  /**< Error Active（正常）        */
    CAN_ERRORSTATE_PASSIVE,     /**< Error Passive（送信制限あり） */
    CAN_ERRORSTATE_BUSOFF       /**< Bus-Off（送信不能）          */
} Can_ErrorStateType;

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
