/**
 * \file    CanIf.c
 * \brief   CAN インタフェース (AUTOSAR SWS_CANInterface 準拠)
 * \details CAN ドライバ (Can.c) と上位通信層 (PduR, DCM) の間に位置する
 *          AUTOSAR CanIf API を実装する。
 *          AUTOSAR 4.3.1 SWS_CANInterface 仕様に準拠し、
 *          Arduino UNO 向けに一部を簡略化している。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "CanIf.h"
#include "Can.h"
#include "CanSM.h"
#include "Det.h"

#define TAG "CanIf"

static const CanIf_ConfigType* CanIf_ConfigPtr = NULL;

/**
 * \brief   CAN インタフェースモジュールを初期化する。
 *
 * \details 設定ポインタを保存し、TX/RX PDU 数をログ出力する
 *          (AUTOSAR SWS_CANIF_00001)。
 *          Can_Init() の呼び出し後、他のすべての CanIf_* API より
 *          先に 1 回だけ呼び出すこと。
 *
 * \param[in]  ConfigPtr  CanIf 設定構造体へのポインタ。NULL 禁止。
 *
 * \pre        Can_Init() が正常に完了していること。
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanIf_Init(const CanIf_ConfigType* ConfigPtr)
{
    CanIf_ConfigPtr = ConfigPtr;
    DET_LOGI(TAG, "Init ok TX=%u RX=%u",
             (unsigned)ConfigPtr->TxPduCount, (unsigned)ConfigPtr->RxPduCount);
}

/**
 * \brief   CAN ドライバ経由で PDU の送信を要求する。
 *
 * \details TxPduId で TX PDU 設定を検索し、PDU 長を設定 DLC と照合したうえで
 *          Can_PduType を構築して Can_Write() を呼び出す
 *          (AUTOSAR SWS_CANIF_00005)。
 *
 * \param[in]  TxPduId     送信する TX PDU の ID。
 *                         設定済み TxPduCount 未満であること。
 * \param[in]  PduInfoPtr  送信するデータと長さへのポインタ。
 *                         NULL 禁止。SduDataPtr も NULL 禁止。
 *
 * \retval  E_OK      PDU が Can_Write() に正常に渡された。
 * \retval  E_NOT_OK  CanIf 未初期化、TxPduId 不正、NULL ポインタ、
 *                    SduLength が設定 DLC を超過、または Can_Write() 失敗。
 *
 * \pre        CanIf_Init() が正常に完了していること。
 * \pre        CAN コントローラが CAN_CS_STARTED 状態であること。
 *
 * \ServiceID      {0x49}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)
{
    if (CanIf_ConfigPtr == NULL)
        return E_NOT_OK;

    if (TxPduId >= CanIf_ConfigPtr->TxPduCount)
    {
        DET_LOGE(TAG, "TX E: invalid TxPduId");
        return E_NOT_OK;
    }

    if (PduInfoPtr == NULL || PduInfoPtr->SduDataPtr == NULL)
    {
        DET_LOGE(TAG, "TX E: PduInfoPtr NULL");
        return E_NOT_OK;
    }

    const CanIf_TxPduConfigType* txCfg = &CanIf_ConfigPtr->TxPduConfig[TxPduId];

    if (PduInfoPtr->SduLength > txCfg->Dlc)
    {
        DET_LOGE(TAG, "TX E: SduLength>DLC");
        return E_NOT_OK;
    }

    Can_PduType canPdu = {
        .swPduHandle = TxPduId,
        .id          = txCfg->CanId,
        .length      = (uint8)PduInfoPtr->SduLength,
        .sdu         = PduInfoPtr->SduDataPtr
    };

    DET_LOGD(TAG, "TX id=%u can=0x%lX", (unsigned)TxPduId, (unsigned long)txCfg->CanId);

    Can_ReturnType ret = Can_Write(txCfg->Hth, &canPdu);

    if (ret == CAN_BUSY)
        DET_LOGW(TAG, "TX BUSY");

    return (ret == CAN_OK) ? E_OK : E_NOT_OK;
}

/**
 * \brief   CAN ドライバから受信フレームを上位層へ通知する。
 *
 * \details CAN ドライバがフレームを受信した際に呼び出される。
 *          RX PDU テーブルから HOH と CAN ID が一致するエントリを検索し、
 *          設定された上位層の RxIndication コールバックへ転送する
 *          (AUTOSAR SWS_CANIF_00415, SWS_CANInterface_00451)。
 *          一致するエントリが存在しない場合はフレームを破棄してログを出力する。
 *
 * \param[in]  Mailbox     受信 CAN ID・HOH・コントローラ ID を格納した
 *                         ハードウェアメールボックス記述子へのポインタ。
 *                         NULL 禁止。
 * \param[in]  PduInfoPtr  受信 PDU のデータと長さへのポインタ。NULL 禁止。
 *
 * \pre        CanIf_Init() が正常に完了していること。
 *
 * \ServiceID      {0x10}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanIf_RxIndication(const Can_HwType* Mailbox, const PduInfoType* PduInfoPtr)
{
    if (CanIf_ConfigPtr == NULL || Mailbox == NULL || PduInfoPtr == NULL)
        return;

    for (uint8 i = 0; i < CanIf_ConfigPtr->RxPduCount; i++)
    {
        const CanIf_RxPduConfigType* rxCfg = &CanIf_ConfigPtr->RxPduConfig[i];

        if (rxCfg->Hrh == Mailbox->Hoh && rxCfg->CanId == Mailbox->CanId)
        {
            DET_LOGD(TAG, "RX can=0x%lX pdu=%u",
                     (unsigned long)Mailbox->CanId,
                     (unsigned)rxCfg->UpperLayerRxPduId);

            if (rxCfg->RxIndicationFct != NULL)
                rxCfg->RxIndicationFct(rxCfg->UpperLayerRxPduId, PduInfoPtr);

            return;
        }
    }

    DET_LOGW(TAG, "RX no match can=0x%lX", (unsigned long)Mailbox->CanId);
}

/**
 * \brief   CAN フレームの送信完了を上位層へ通知する。
 *
 * \details CAN ドライバが送信完了を確認した後に呼び出される。
 *          CanTxPduId で TX PDU 設定を検索し、設定された上位層の
 *          TxConfirmation コールバックを呼び出す
 *          (AUTOSAR SWS_CANIF_00011)。
 *          CanTxPduId が範囲外の場合は処理を無視する。
 *
 * \param[in]  CanTxPduId  送信が完了した TX PDU の ID。
 *                         設定済み TxPduCount 未満であること。
 *
 * \pre        CanIf_Init() が正常に完了していること。
 *
 * \ServiceID      {0x11}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanIf_TxConfirmation(PduIdType CanTxPduId)
{
    if (CanIf_ConfigPtr == NULL)
        return;

    if (CanTxPduId >= CanIf_ConfigPtr->TxPduCount)
        return;

    const CanIf_TxPduConfigType* txCfg = &CanIf_ConfigPtr->TxPduConfig[CanTxPduId];

    DET_LOGD(TAG, "TxConf id=%u", (unsigned)CanTxPduId);

    if (txCfg->TxConfirmFct != NULL)
        txCfg->TxConfirmFct(txCfg->UpperLayerTxPduId, E_OK);
}

/**
 * \brief   CAN コントローラの Bus-Off 状態を上位層へ通知する。
 *
 * \details Can_Isr() が Bus-Off を検出した際に呼び出される。
 *          CanSM_ControllerBusOff() へ委譲し、回復シーケンスを起動する
 *          (AUTOSAR SWS_CANIF_00233)。
 *
 * \param[in]  ControllerId  Bus-Off を検出したコントローラ ID。
 *
 * \ServiceID      {0x16}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanIf_ControllerBusOff(uint8 ControllerId)
{
    DET_LOGW(TAG, "ControllerBusOff ch=%u", (unsigned)ControllerId);
    CanSM_ControllerBusOff(ControllerId);
}
