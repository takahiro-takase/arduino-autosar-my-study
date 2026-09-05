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

/**
 * \brief   指定コントローラで、直近の起動以降に一度でも TX 確認があったかを返す
 *          （[SWS_CANIF_00734]）。
 *
 * \details `CanIf_TxConfirmation()` が呼ばれるたびに、対象コントローラ（本
 *          プロジェクトは `CANIF_CONTROLLER_MAX=1` のため常にコントローラ 0）
 *          が `CAN_CS_STARTED` であれば状態を `CANIF_TX_RX_NOTIFICATION` へ
 *          セットする（[SWS_CANIF_00740]、STARTED 以外では更新しない。Can.c
 *          の `Can_TxConfQueue` は非同期にドレインされるため、送信要求時点で
 *          STARTED でも通知が届く頃には停止済みということがありうるため）。
 *          `CanIf_SetControllerMode(ControllerId, CAN_CS_STARTED)` が成功する
 *          たびに `CANIF_NO_NOTIFICATION` へリセットする（Table 8.25 の
 *          "since the last CAN controller start" に対応）。`CAN_CS_STOPPED`
 *          への遷移成功時にもリセットする（[SWS_CANIF_00739]。同要求が求める
 *          「未確認 TX への負の確認通知の一括送出」までは実装しない）。
 *
 *          `CanIf_ReadTxNotifStatus()`/`CanIf_ReadRxNotifStatus()` と異なり、
 *          本関数は「Read」ではなく「Get」であり、実仕様も読み出し時の
 *          クリアを規定していない（[SWS_CANIF_00734]にリセット言及なし）
 *          ため、本実装も読み出し時にはクリアしない。
 *
 * \note    実仕様は CanSM の Bus-Off 回復判定（G_BUS_OFF_PASSIVE、
 *          [SWS_CanSM_00497]、`CANSM_BOR_TX_CONFIRMATION_POLLING` 有効時）が
 *          本関数を使う想定だが、本プロジェクトの `CanSM_MainFunction()` は
 *          コントローラ再起動（`CanIf_SetControllerMode(STARTED)`）の成功
 *          そのものを回復完了とみなす簡略設計のため、現時点では未配線。
 *
 * \param[in]  ControllerId  対象コントローラの ID。
 *
 * \return  対象コントローラの TX 確認状態。未初期化または `ControllerId` が
 *          範囲外の場合は `CANIF_NO_NOTIFICATION` を返す
 *          （フェールセーフ、[SWS_CANIF_00736] の DET 報告と併用）。
 *
 * \AUTOSARReq     {SWS_CANIF_00734, SWS_CANIF_00736, SWS_CANIF_00739, SWS_CANIF_00740}
 * \ServiceID      {0x19}
 * \Reentrancy     {Reentrant (Not for the same controller)}
 * \Synchronicity  {Synchronous}
 */
CanIf_NotifStatusType CanIf_GetTxConfirmationState(uint8 ControllerId);

#ifdef __cplusplus
}
#endif

#endif
