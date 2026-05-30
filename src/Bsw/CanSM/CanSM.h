/**
 * \file    CanSM.h
 * \brief   CAN ステートマネージャ 公開インタフェース (AUTOSAR SWS_CanSM 準拠)
 * \details ComM と CanIf の間に位置し、CAN ネットワークの通信モード遷移と
 *          Bus-Off 回復シーケンスを管理する。
 *
 *          AUTOSAR 通信スタックにおける CanSM の位置:
 *            ComM
 *             ↓ CanSM_RequestComMode()
 *            CanSM  ← 本モジュール
 *             ↓ Can_SetControllerMode()
 *            CanIf → Can
 *
 *          Bus-Off 回復シーケンス:
 *            CanIf_ControllerBusOff() → CanSM_ControllerBusOff()
 *              → コントローラ停止 → T_REC 待機（CanSM_MainFunction）
 *              → コントローラ再起動 → FULL_COM 復帰
 *              → 連続失敗時は最大試行回数まで繰り返す
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef CANSM_H
#define CANSM_H

#include "Std_Types.h"
#include "ComM.h"
#include "CanSM_Cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/** ネットワークハンドル型 */
typedef uint8 CanSM_NetworkHandleType;

/**
 * \brief   CanSM モジュールを初期化する。
 *
 * \details 全ネットワークを COMM_NO_COMMUNICATION 状態に設定する。
 *          EcuM_Init() から ComM_Init() より先に呼び出すこと。
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanSM_Init(void);

/**
 * \brief   ネットワークの通信モード遷移を要求する。
 *
 * \details ComM から呼び出され、要求モードに応じて
 *          Can_SetControllerMode() でハードウェアを制御する。
 *          Bus-Off 回復中の場合は要求を保留する。
 *
 * \param[in]  network  ネットワークハンドル（0 〜 CANSM_CHANNEL_COUNT-1）。
 * \param[in]  mode     要求する通信モード (ComM_ModeType)。
 *
 * \retval  E_OK      モード遷移を受理した。
 * \retval  E_NOT_OK  network が範囲外、または Bus-Off 回復中。
 *
 * \ServiceID      {0x02}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType CanSM_RequestComMode(CanSM_NetworkHandleType network, ComM_ModeType mode);

/**
 * \brief   ネットワークの現在の通信モードを取得する。
 *
 * \param[in]  network  ネットワークハンドル。
 * \param[out] mode     現在のモードを受け取る変数へのポインタ。NULL 禁止。
 *
 * \retval  E_OK      正常に取得した。
 * \retval  E_NOT_OK  network が範囲外、または mode が NULL。
 *
 * \ServiceID      {0x03}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType CanSM_GetCurrentComMode(CanSM_NetworkHandleType network, ComM_ModeType* mode);

/**
 * \brief   Bus-Off 通知コールバック（CanIf から呼び出される）。
 *
 * \details CAN コントローラが Bus-Off 状態を検出した際に CanIf 経由で
 *          呼び出される。コントローラを停止し、回復タイマを起動する。
 *
 * \param[in]  ControllerId  Bus-Off を検出したコントローラ ID。
 *
 * \ServiceID      {0x04}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanSM_ControllerBusOff(uint8 ControllerId);

/**
 * \brief   CanSM 周期処理（Bus-Off 回復タイマ管理）。
 *
 * \details OS タスク (10 ms 周期) から呼び出される。
 *          Bus-Off 状態のとき回復タイマを監視し、
 *          T_REC 経過後にコントローラの再起動を試みる。
 *
 * \ServiceID      {0x05}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanSM_MainFunction(void);

#ifdef __cplusplus
}
#endif

#endif /* CANSM_H */
