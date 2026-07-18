/**
 * \file    Nm.c
 * \brief   ネットワークマネジメント実装 (AUTOSAR SWS_CANNM 準拠)
 * \details ComM が FULL_COM の間、NM フレーム（CAN 0x400, DLC=2）を
 *          NM_CYCLE_MS 周期で送信する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "Nm.h"
#include "Nm_Cfg.h"
#include "CanIf.h"
#include "ComM.h"
#include "ComM_Cfg.h"
#include "Det.h"

#define TAG "Nm"

/* 診断 CommunicationControl (UDS SID 0x28) からの送信有効/無効状態。
 * 既定は有効 (1)。Nm_SetTxEnabled() 参照。 */
static uint8 Nm_TxEnabled = 1U;

void Nm_Init(void)
{
    Nm_TxEnabled = 1U;
    DET_LOGI(TAG, "Init ok node=0x%02X", (unsigned)NM_SOURCE_NODE_ID);
}

/**
 * \brief   ComM が FULL_COM の間、NM フレームを組み立てて CanIf_Transmit() へ渡す。
 *
 * \details byte[0]=Control Bit Vector（本プロジェクトでは未使用のため常に 0）、
 *          byte[1]=Source Node Identifier。PduR/Com を経由せず、
 *          CanIf_Transmit() を直接呼び出す（実車の CanNm と同じ構造）。
 */
void Nm_MainFunction(void)
{
    ComM_ModeType mode;
    if (ComM_GetCurrentComMode(COMM_USER_0, &mode) != E_OK)
        return;
    if (mode != COMM_FULL_COMMUNICATION)
        return;  /* NO_COM 中は送信しない（実車でバススリープへ向かうのと同じ意味） */

    if (Nm_TxEnabled == 0U)
        return;  /* 診断 CommunicationControl (UDS 0x28) による送信抑制中 */

    uint8 pdu[NM_DLC];
    pdu[0] = 0x00U;              /* Control Bit Vector: 本プロジェクトでは未使用 (予約) */
    pdu[1] = NM_SOURCE_NODE_ID;  /* Source Node Identifier */

    PduInfoType pduInfo = {
        .SduDataPtr = pdu,
        .SduLength  = NM_DLC
    };

    (void)CanIf_Transmit(NM_CANIF_TX_PDU_ID, &pduInfo);
}

void Nm_SetTxEnabled(uint8 Enabled)
{
    if (Nm_TxEnabled != Enabled)
        DET_LOGI(TAG, "CommunicationControl tx=%u->%u", (unsigned)Nm_TxEnabled, (unsigned)Enabled);
    Nm_TxEnabled = Enabled;
}
