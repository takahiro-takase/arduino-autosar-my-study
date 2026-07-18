/**
 * \file    CanTp.h
 * \brief   CAN トランスポートプロトコル 公開インタフェース
 *          (AUTOSAR SWS_CanTp 準拠, ISO 15765-2)
 * \details CanIf/PduR と DCM の間に位置し、Single Frame (SF) /
 *          First Frame (FF) / Consecutive Frame (CF) /
 *          Flow Control (FC) の送受信を仲介する。
 *
 *          RX フロー:
 *            PduR_CanIfRxIndication → CanTp_RxIndication
 *            CanTp: SF/FF+CF を組み立て → Dcm_ComIndication (UDS ペイロードのみ)
 *
 *          TX フロー:
 *            Dcm → CanTp_Transmit (UDS ペイロードのみ)
 *            CanTp: SF/FF+CF に分割 → PduR_Transmit → CanIf → CAN 0x7E8
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef CANTP_H
#define CANTP_H

#include "CanTp_Cfg.h"
#include "ComStack_Types.h"
#include "Std_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   CanTp モジュールを初期化する。
 *
 * \details RX/TX チャネルの状態を IDLE にリセットする (SWS_CanTp_00208)。
 *          EcuM_Init() から PduR_Init() の後に呼び出すこと。
 *
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanTp_Init(void);

/**
 * \brief   DCM からの UDS ペイロードを CAN トランスポートフレームに分割して送信する。
 *
 * \details PduInfoPtr の SduLength <= 7 なら Single Frame として送信。
 *          8 バイト以上なら First Frame + Consecutive Frame に分割し、
 *          Flow Control 受信後に CF を順次送信する (SWS_CanTp_00218)。
 *
 * \param[in]  TxSduId     TX N-SDU ID (CANTP_TX_SDU_ID)。本実装は 1 チャネル固定。
 * \param[in]  PduInfoPtr  送信する UDS ペイロード (PCI バイトを含まない生データ)。
 *                         SduLength が CANTP_TX_BUFFER_SIZE を超えてはならない。
 *
 * \retval  E_OK      送信要求を受け付けた (SF) または FF を送信した (MF)。
 * \retval  E_NOT_OK  NULL ポインタ、長さ超過、TX チャネルがビジー、
 *                    または SF/FF の物理送信が同期的に失敗した
 *                    (PduR/CanIf/Can_Write が拒否した)。
 *
 * \ServiceID      {0x49}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType CanTp_Transmit(PduIdType TxSduId, const PduInfoType* PduInfoPtr);

/**
 * \brief   PduR から配信された受信 CAN フレームを処理する。
 *
 * \details フレームタイプ (SF/FF/CF/FC) を判定し、
 *          SF または組立完了した多重フレームを Dcm_ComIndication へ渡す
 *          (SWS_CanTp_00214)。
 *          FC フレームは TX 側の FC 待ちを解除する。
 *
 * \param[in]  RxPduId     受信 PDU ID (本実装では単一チャネルのため未使用)。
 * \param[in]  PduInfoPtr  受信した 8 バイト CAN フレームへのポインタ。
 *
 * \ServiceID      {0x42}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanTp_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr);

/**
 * \brief   PduR からの送信完了通知を受け取る。
 *
 * \details CanIf がフレーム送信を完了すると PduR を経由して呼び出される
 *          (SWS_CanTp_00236)。本実装では現在 no-op（意図的。詳細は CanTp.c の
 *          実装コメント参照）: 本プロジェクトの非同期 TX 確認経路は真の送信
 *          失敗を表現できず、result は実質的に常に E_OK しか渡ってこない。
 *          実際に到達可能な送信失敗は CanTp_SendFrame() の同期的な戻り値と
 *          CF 送信の N_As 相当タイムアウトで別途検出している。
 *
 * \param[in]  TxPduId  送信完了した PDU の ID。
 * \param[in]  result   送信結果 (E_OK / E_NOT_OK。上記理由により本経路では
 *                      実質的に常に E_OK)。
 *
 * \ServiceID      {0x48}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanTp_TxConfirmation(PduIdType TxPduId, Std_ReturnType result);

/**
 * \brief   タイムアウト監視と CF 送信を周期的に処理する。
 *
 * \details EcuM_MainFunction から毎ループ呼び出す。以下を担当する:
 *          - TX WAIT_FC 状態: N_Bs タイムアウト検出 → TX 中断
 *          - TX SEND_CF 状態: STmin 経過後に次の CF を送信
 *          - RX WAIT_CF 状態: N_Cr タイムアウト検出 → RX 中断
 *
 * \ServiceID      {0x1B}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanTp_MainFunction(void);

#ifdef __cplusplus
}
#endif

#endif /* CANTP_H */
