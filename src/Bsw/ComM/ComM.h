/**
 * \file    ComM.h
 * \brief   通信マネージャ 公開インタフェース (AUTOSAR SWS_ComM 準拠)
 * \details CAN 通信スタックの通信モードを管理する。
 *          複数のユーザ（EcuM, アプリ等）からのモード要求を調停し、
 *          チャネルの通信状態（NO_COM / SILENT_COM / FULL_COM）を制御する。
 *          チャネルへの実際の操作は Can_SetControllerMode() 経由で行う。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef COMM_H
#define COMM_H

#include "Std_Types.h"
#include "ComM_Cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/** ユーザハンドル型 */
typedef uint8 ComM_UserHandleType;

/** 通信モード型 */
typedef uint8 ComM_ModeType;

#define COMM_NO_COMMUNICATION      0U  /**< 通信停止（CAN バス非アクティブ） */
#define COMM_SILENT_COMMUNICATION  1U  /**< 受信専用（TX 停止） */
#define COMM_FULL_COMMUNICATION    2U  /**< 全二重通信（TX/RX 有効） */

/**
 * \brief   ComM モジュールを初期化する。
 *
 * \details 全チャネルを COMM_NO_COMMUNICATION 状態に設定する。
 *          CAN バスはまだアクティブにならない。
 *          EcuM_Init() から Can_Init() の後に呼び出すこと。
 *
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void ComM_Init(void);

/**
 * \brief   ユーザが通信モードを要求する。
 *
 * \details 要求モードに応じてチャネルの状態遷移を行い、
 *          Can_SetControllerMode() でハードウェアを制御する。
 *          複数ユーザからの要求は最高優先モードに調停する
 *          (AUTOSAR SWS_ComM_00069)。
 *
 * \param[in]  User     要求するユーザ ID (COMM_USER_0 等)。
 * \param[in]  ComMode  要求する通信モード
 *                      (COMM_NO_COMMUNICATION / COMM_SILENT_COMMUNICATION /
 *                       COMM_FULL_COMMUNICATION)。
 *
 * \retval  E_OK      モード遷移を受理した。
 * \retval  E_NOT_OK  User が範囲外、または不正な ComMode。
 *
 * \ServiceID      {0x05}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType ComM_RequestComMode(ComM_UserHandleType User, ComM_ModeType ComMode);

/**
 * \brief   ユーザの現在の通信モードを取得する。
 *
 * \param[in]  User      照会するユーザ ID。
 * \param[out] ComMode   現在のモードを受け取る変数へのポインタ。NULL 禁止。
 *
 * \retval  E_OK      正常に取得した。
 * \retval  E_NOT_OK  User が範囲外、または ComMode が NULL。
 *
 * \ServiceID      {0x06}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType ComM_GetCurrentComMode(ComM_UserHandleType User, ComM_ModeType* ComMode);

/**
 * \brief   ComM 周期処理（バス通信状態の監視）。
 *
 * \details 本実装ではパッシブウェイクアップ・バススリープは未対応のためスタブ。
 *          AUTOSAR 準拠実装では NM（Network Manager）連携を行う。
 *
 * \ServiceID      {0x03}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void ComM_MainFunction(void);

/**
 * \brief   CanSM からの通信モード変化通知コールバック（下位層 → 上位層）。
 *
 * \details CanSM が実際の CAN バス状態を変化させた際に呼び出す。
 *          ComM はチャネル状態を更新し、EcuM_RequestRUN / EcuM_ReleaseRUN を通じて
 *          EcuM に RUN 要求の変化を伝える。ただし `EcuM_RequestRUN()`/
 *          `EcuM_ReleaseRUN()` は同一ユーザからの重複要求・対応しない解放を
 *          DET 相当で検知する（SWS_EcuM_04125/04127）ため、実際にチャネル
 *          モードが変化した時のみこれらを呼び出す（Bus-Off L1/L2 バックオフの
 *          リトライ成功のたびに本関数が FULL_COM で呼ばれても、モードが
 *          変化していなければ EcuM へは再通知しない）。
 *          あわせて `ComM_UserRequest[COMM_USER_0]`
 *          をこの Mode に同期させる（どのユーザの要求でもない自動的な状態変化を
 *          User0 の要求として扱うことで、次回の集約計算に古い要求が
 *          残らないようにするため。詳細は ComM.c の実装コメントを参照）。
 *
 *          呼び出しタイミング:
 *            - CanSM_RequestComMode 成功後 (FULL_COM / NO_COM への遷移時)
 *            - Bus-Off 回復試行時 (FULL_COM。L1/L2 バックオフで無期限に継続する
 *              ため、Bus-Off を理由に NO_COM が呼ばれることはない)
 *            - ウェイクアップ検証成功時 (FULL_COM)
 *
 * \param[in]  Network  ネットワークハンドル (0 〜 COMM_CHANNEL_COUNT-1)。
 * \param[in]  Mode     新しい通信モード (ComM_ModeType)。
 *
 * \ServiceID      {0x26}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void ComM_BusSMIndication(uint8 Network, ComM_ModeType Mode);

#ifdef __cplusplus
}
#endif

#endif /* COMM_H */
