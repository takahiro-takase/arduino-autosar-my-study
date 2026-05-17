/**
 * \file    Com.h
 * \brief   通信マネージャ 公開インタフェース (AUTOSAR SWS_COM 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef COM_H
#define COM_H

#include "Com_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* SWS_Com_00864 */
void Com_Init(const Com_ConfigType* config);
/* SWS_Com_00442 */
void Com_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr);
/* SWS_Com_00194 */
uint8 Com_ReceiveSignal(Com_SignalIdType SignalId, void* SignalDataPtr);
/* SWS_Com_00171 */
uint8 Com_SendSignal(Com_SignalIdType SignalId, const void* SignalDataPtr);
/* SWS_Com_00725 */
Std_ReturnType Com_TriggerIPDUSend(PduIdType PduId);
/* SWS_Com_00695 */
void Com_TxConfirmation(PduIdType TxPduId, Std_ReturnType result);

#ifdef __cplusplus
}
#endif

#endif
