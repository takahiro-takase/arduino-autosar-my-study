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
 *            ComM_BusSMIndication()          → BswM_ComM_CurrentMode()
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
 * \brief   EcuM からのフェーズ変化通知コールバック。
 *
 * \details EcuM が状態遷移するたびに呼ぶ。前回と同じ状態なら何もしない。
 *          BswM は受け取った状態に一致するルールを評価し、
 *          Os_SetTaskActive() でタスクの有効・無効を切り替える。
 *
 * \param[in]  state  新しい EcuM フェーズ (EcuM_StateType)。
 *
 * \ServiceID      {0x0E}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void BswM_EcuM_CurrentState(EcuM_StateType state);

/**
 * \brief   ComM からの通信モード変化通知コールバック。
 *
 * \details ComM_BusSMIndication() が呼ぶ。
 *          BswM は受け取ったモードに一致するルールを評価する。
 *          本プロジェクトの初期設定では ComM トリガのルールは未定義のため
 *          ルール評価は空振りするが、ルール追加で容易に機能拡張できる。
 *
 * \param[in]  channel  CAN チャネル番号 (0 固定)。
 * \param[in]  mode     新しい通信モード (ComM_ModeType)。
 *
 * \ServiceID      {0x0F}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void BswM_ComM_CurrentMode(uint8 channel, ComM_ModeType mode);

#ifdef __cplusplus
}
#endif

#endif /* BSWM_H */
