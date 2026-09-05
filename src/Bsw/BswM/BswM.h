/**
 * \file    BswM.h
 * \brief   BSW モードマネージャ 公開インタフェース (AUTOSAR SWS_BswM 準拠)
 * \details BswM は BSW モジュールや EcuM から「モード変化の通知」を受け取り、
 *          あらかじめ定義したルールテーブルを評価して Os タスクの有効・無効を
 *          切り替えるルールエンジンである。
 *
 *          AUTOSAR における役割:
 *            EcuM — 「今どのフェーズか」を管理する (状態マシン)
 *            BswM — 「そのフェーズで何をするか」を管理する (ルールエンジン)
 *
 *          モード通知経路:
 *            EcuM_Init() / EcuM_ReleaseRUN() → BswM_EcuM_CurrentState()
 *            ComM_BusSM_ModeIndication()          → BswM_ComM_CurrentMode()
 *            Dcm_HandleCommunicationControl()/Dcm_CommControlReset()
 *              → BswM_Dcm_CommunicationMode_CurrentState()
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef BSWM_H
#define BSWM_H

#include "Std_Types.h"
#include "BswM_PBCfg.h"
#include "EcuM.h"
#include "ComM.h"
#include "Dcm_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   BswM モジュールを初期化する。
 *
 * \details コンフィグポインタを保存し内部モードキャッシュをリセットする。
 *          EcuM_Init() 内で Os_Init() の直前に呼び出すこと。
 *
 * \param[in]  ConfigPtr  ポストビルドコンフィグへのポインタ。NULL 禁止。
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void BswM_Init(const BswM_ConfigType* ConfigPtr);

/**
 * \brief   BswM モジュールを非初期化する。
 *
 * \details [SWS_BswM_00120] の通り、以降 BswM_EcuM_CurrentState()/
 *          BswM_ComM_CurrentMode() が呼ばれてもモード処理（ルール評価・
 *          Os_SetTaskActive() 呼び出し）を一切行わない。BswM_Init() 前に
 *          呼ぶと BSWM_E_NO_INIT を報告する。
 *
 * \AUTOSARReq     {SWS_BswM_00119}
 * \ServiceID      {0x04}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void BswM_Deinit(void);

/**
 * \brief   BswM モジュールのバージョン情報を取得する。
 *
 * \details SWS_BswM_00003。BswM_Init と並び、未初期化時でも BSWM_E_NO_INIT を
 *          報告しない例外 API（他 BSW モジュールと共通の慣例）のため、
 *          初期化状態は確認せず NULL ポインタチェックのみ行う。
 *
 * \param[out]  VersionInfo  バージョン情報の格納先。NULL 禁止。
 *
 * \ServiceID      {0x01}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void BswM_GetVersionInfo(Std_VersionInfoType* VersionInfo);

/**
 * \brief   EcuM からのフェーズ変化通知コールバック。
 *
 * \details EcuM が状態遷移するたびに呼ぶ。前回と同じ状態なら何もしない。
 *          BswM は受け取った状態に一致するルールを評価し、
 *          Os_SetTaskActive() でタスクの有効・無効を切り替える。
 *
 * \param[in]  state  新しい EcuM フェーズ (EcuM_StateType)。
 *
 * \ServiceID      {0x0F}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void BswM_EcuM_CurrentState(EcuM_StateType state);

/**
 * \brief   ComM からの通信モード変化通知コールバック。
 *
 * \details ComM_BusSM_ModeIndication() が呼ぶ。
 *          BswM は受け取ったモードを含む条件を持つ全ルール（単一条件・複合
 *          条件いずれも）を評価する（BswM_PBCfg.c の Rule3/Rule5 参照）。
 *
 * \param[in]  channel  CAN チャネル番号 (0 固定)。
 * \param[in]  mode     新しい通信モード (ComM_ModeType)。
 *
 * \ServiceID      {0x0E}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void BswM_ComM_CurrentMode(NetworkHandleType channel, ComM_ModeType mode);

/**
 * \brief   Dcm からの UDS 0x28 CommunicationControl 通知コールバック。
 *
 * \details Dcm_HandleCommunicationControl()/Dcm_CommControlReset() が呼ぶ。
 *          BswM は受け取った Dcm_CommunicationModeType 値に一致する
 *          `BSWM_ACTION_DCM_COMM_APPLY` ルール（`BswM_PBCfg.c`）を発火させ、
 *          `BswM_ApplyDcmCommMode()` 経由で実際に Com/Nm へ反映する
 *          （2026-09-05、シグネチャ準拠サーベイで、Dcm 側が Com/Nm を直接
 *          呼んでいたレイヤ違反を是正した際に新設）。
 *
 * \param[in]  Network        通信チャネル（本プロジェクトは単一ネットワーク
 *                            構成のため受け取るだけで検証・使用しない）。
 * \param[in]  RequestedMode  要求された通信モード。
 *
 * \note    BswM 未初期化時（`BswM_Cfg == NULL`）は DET 報告のみで無視するが、
 *          本関数は仕様どおり戻り値を持たないため呼び出し元（Dcm）へは
 *          伝わらない。実際にはこの呼び出し経路（`Dcm_HandleCommunicationControl()`/
 *          `Dcm_CommControlReset()`）は EcuM_Init() の起動シーケンスで
 *          BswM_Init() 完了後にしか到達しない（診断セッション確立後の
 *          UDS 要求経由のため）ため、実運用上は問題にならない。
 *
 * \AUTOSARReq     {SWS_BswM_00048, SWS_BswM_00079, SWS_BswM_00093}
 * \ServiceID      {0x06}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void BswM_Dcm_CommunicationMode_CurrentState(NetworkHandleType Network, Dcm_CommunicationModeType RequestedMode);

#ifdef __cplusplus
}
#endif

#endif /* BSWM_H */
