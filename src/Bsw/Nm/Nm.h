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
#include "Std_Types.h"

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
 * \brief   Nm モジュールを未初期化状態に戻す。
 *
 * \details 初期化済みフラグを未初期化に戻す。未初期化状態で呼ばれた場合は
 *          NM_E_UNINIT を報告し何もしない。
 *
 * \ServiceID      {0x10}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Nm_DeInit(void);

/**
 * \brief   NM フレームを周期送信する。
 *
 * \details ComM_GetCurrentComMode() が COMM_FULL_COMMUNICATION を返す間のみ
 *          NM フレームを送信する。NO_COM の間は送信しない
 *          （実車で NM フレームが止まるとバススリープへ向かうのと同じ意味）。
 *
 * \ServiceID      {0x13}
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

/**
 * \brief   Nm モジュールのバージョン情報を取得する。
 *
 * \details Nm_Init と並び、未初期化時でも NM_E_UNINIT を報告しない例外 API
 *          （他 BSW モジュールと共通の慣例）のため、初期化状態は確認せず
 *          NULL ポインタチェックのみ行う。
 *
 * \param[out]  versioninfo  バージョン情報の格納先。NULL 禁止。
 *
 * \ServiceID      {0xF1}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Nm_GetVersionInfo(Std_VersionInfoType* versioninfo);

#ifdef __cplusplus
}
#endif

#endif /* NM_H */
