/**
 * \file    Nm.h
 * \brief   ネットワークマネジメント 公開インタフェース (AUTOSAR SWS_CANNM 準拠)
 * \details 本 ECU（メータ ECU）の NM フレーム（CAN 0x400）送信を担う。
 *          実車の CanNm は Com スタックを経由せず CanIf_Transmit() を
 *          直接呼び出すため、本モジュールも PduR/Com を介さない。
 *
 *          使い方:
 *            1. EcuM_Init 内で Nm_Init() を呼ぶ（ComM_Init 完了後）。
 *            2. Os スケジューラが NM_CYCLE_MS ごとに Nm_MainFunction() を呼ぶ。
 *               ComM が FULL_COM の間だけ NM フレームを送信する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef NM_H
#define NM_H

#include "Platform_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   Nm モジュールを初期化する。
 *
 * \pre        CanIf_Init() / ComM_Init() が正常に完了していること。
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Nm_Init(void);

/**
 * \brief   NM フレームを周期送信する。
 *
 * \details ComM_GetCurrentComMode() が COMM_FULL_COMMUNICATION を返す間のみ
 *          NM フレームを送信する。NO_COM の間は送信しない
 *          （実車で NM フレームが止まるとバススリープへ向かうのと同じ意味）。
 *
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Nm_MainFunction(void);

/**
 * \brief   診断 CommunicationControl (UDS SID 0x28) からの送信有効/無効要求を反映する。
 *
 * \details Enabled=0 の間、Nm_MainFunction() は ComM が FULL_COMMUNICATION でも
 *          NM フレームを送信しない。
 *
 * \param[in]  Enabled  0=送信を抑制する、1=通常どおり送信する。
 *
 * \ServiceID      {0x02}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Nm_SetTxEnabled(uint8 Enabled);

#ifdef __cplusplus
}
#endif

#endif /* NM_H */
