/**
 * \file    CanIf.h
 * \brief   CAN インタフェース 公開インタフェース (AUTOSAR SWS_CANInterface 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef CANIF_H
#define CANIF_H

#include "CanIf_Types.h"
#include "CanIf_Cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

void           CanIf_Init(const CanIf_ConfigType* ConfigPtr);
void           CanIf_DeInit(void);
Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr);
void           CanIf_RxIndication(const Can_HwType* Mailbox, const PduInfoType* PduInfoPtr);
Std_ReturnType CanIf_ReadRxPduData(PduIdType CanIfRxSduId, PduInfoType* CanIfRxInfoPtr);
void           CanIf_TxConfirmation(PduIdType CanTxPduId);
void           CanIf_ControllerBusOff(uint8 ControllerId);
void           CanIf_ControllerWakeup(uint8 ControllerId);
void           CanIf_GetVersionInfo(Std_VersionInfoType* versioninfo);
Std_ReturnType CanIf_SetPduMode(uint8 ControllerId, CanIf_PduModeType PduModeRequest);
Std_ReturnType CanIf_GetPduMode(uint8 ControllerId, CanIf_PduModeType* PduModePtr);

#ifdef __cplusplus
}
#endif

#endif
