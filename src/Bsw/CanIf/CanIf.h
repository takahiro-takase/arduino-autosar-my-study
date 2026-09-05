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
void           CanIf_GetVersionInfo(Std_VersionInfoType* versioninfo);
Std_ReturnType CanIf_SetPduMode(uint8 ControllerId, CanIf_PduModeType PduModeRequest);
Std_ReturnType CanIf_GetPduMode(uint8 ControllerId, CanIf_PduModeType* PduModePtr);
Std_ReturnType CanIf_SetControllerMode(uint8 ControllerId, Can_ControllerStateType ControllerMode);
Std_ReturnType CanIf_GetControllerMode(uint8 ControllerId, Can_ControllerStateType* ControllerModePtr);
Std_ReturnType CanIf_GetControllerErrorState(uint8 ControllerId, Can_ErrorStateType* ErrorStatePtr);

/**
 * \brief   指定 TX PDU の送信完了通知状態を取得し、読み出した状態をクリアする
 *          （[SWS_CANIF_00202]）。
 *
 * \details `CanIf_TxConfirmation()` が呼ばれると当該 TX PDU の状態は
 *          `CANIF_TX_RX_NOTIFICATION` になり、本関数を呼ぶと
 *          `CANIF_NO_NOTIFICATION` へリセットされる（[SWS_CANIF_00393]）。
 *          実仕様はこのリセット動作自体を `CANIF_PUBLIC_READTXPDU_NOTIFY_STATUS_API`/
 *          `CANIF_TXPDU_READ_NOTIFYSTATUS` の2つのビルド時コンフィグで
 *          有効/無効を切り替えられるが、本プロジェクトはそのような切替を
 *          持たないため常に読み出し時にリセットする（学習用簡略化）。
 *
 * \param[in]  CanIfTxSduId  対象 TX PDU の ID（`CanIf_ConfigPtr->TxPduConfig[]`
 *                           の添字、`CanIf_Transmit()`/`CanIf_TxConfirmation()`
 *                           と同じ名前空間）。
 *
 * \return  対象 TX PDU の通知状態（読み出し前の値）。未初期化または
 *          `CanIfTxSduId` が範囲外の場合は `CANIF_NO_NOTIFICATION` を返す
 *          （フェールセーフ、[SWS_CANIF_00331] の DET 報告と併用）。
 *
 * \AUTOSARReq     {SWS_CANIF_00202, SWS_CANIF_00393, SWS_CANIF_00331}
 * \ServiceID      {0x07}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
CanIf_NotifStatusType CanIf_ReadTxNotifStatus(PduIdType CanIfTxSduId);

/**
 * \brief   指定 RX PDU の受信通知状態を取得し、読み出した状態をクリアする
 *          （[SWS_CANIF_00230]）。
 *
 * \details `CanIf_RxIndication()` が対象 RX PDU への振り分けに成功すると
 *          当該 RX PDU の状態は `CANIF_TX_RX_NOTIFICATION` になり、本関数を
 *          呼ぶと `CANIF_NO_NOTIFICATION` へリセットされる（[SWS_CANIF_00394]）。
 *          実仕様はこのリセット動作自体を `CANIF_PUBLIC_READRXPDU_NOTIFY_STATUS_API`/
 *          `CANIF_RXPDU_READ_NOTIFYSTATUS` の2つのビルド時コンフィグで
 *          有効/無効を切り替えられるが、本プロジェクトはそのような切替を
 *          持たないため常に読み出し時にリセットする（学習用簡略化）。
 *
 * \param[in]  CanIfRxSduId  対象 RX PDU の ID（`CanIf_ConfigPtr->RxPduConfig[]`
 *                           の添字、`CanIf_ReadRxPduData()` と同じ名前空間。
 *                           `UpperLayerRxPduId` とは別の ID 空間である点に
 *                           注意——同関数の Doxygen 参照）。
 *
 * \return  対象 RX PDU の通知状態（読み出し前の値）。未初期化または
 *          `CanIfRxSduId` が範囲外の場合は `CANIF_NO_NOTIFICATION` を返す
 *          （フェールセーフ、[SWS_CANIF_00336] の DET 報告と併用）。
 *
 * \AUTOSARReq     {SWS_CANIF_00230, SWS_CANIF_00394, SWS_CANIF_00336}
 * \ServiceID      {0x08}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
CanIf_NotifStatusType CanIf_ReadRxNotifStatus(PduIdType CanIfRxSduId);

#ifdef __cplusplus
}
#endif

#endif
