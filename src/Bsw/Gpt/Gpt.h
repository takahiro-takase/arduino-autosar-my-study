/**
 * \file    Gpt.h
 * \brief   GPT Driver 公開インタフェース (AUTOSAR SWS_Gpt 準拠)
 * \details 実チャネルは 1 本の連続動作 (CONTINUOUS) タイマとして
 *          Hal/Gpt_Hw 層（Renesas RA FspTimer）に委譲する。
 *          呼び出し元は本ヘッダのみをインクルードし、FspTimer / Arduino API を
 *          直接参照しない（境界は src/Hal/Gpt_Hw.h）。
 *
 *          対応範囲外（本プロジェクトでは未実装）:
 *            Gpt_SetMode / Gpt_EnableWakeup / Gpt_DisableWakeup /
 *            Gpt_CheckWakeup — いずれも AUTOSAR 仕様上
 *            GptWakeupFunctionalityApi でプリコンパイル On/Off 可能な
 *            機能([SWS_Gpt_00201] 等)であり、本プロジェクトの EcuM は
 *            SLEEP モードや EcuM_SetWakeupEvent を持たない（RUN のみ）ため、
 *            仕様に沿った形で丸ごと未実装とする。
 *            Gpt_GetPredefTimerValue — GPT Predef Timer（RA 用の事前定義
 *            タイマ抽象）を使う予定がないため未実装。
 *
 *          チャネル状態機械（[SWS_Gpt_00295] 等）:
 *            initialized (一度も Start されていない)
 *              -> running   (Gpt_StartTimer)
 *              -> stopped   (Gpt_StopTimer、または DeInit 前提として running 禁止)
 *              -> expired   (ONESHOT のみ。目標時間到達で自動停止)
 *            continuous モードは目標時間到達のたびに running のまま
 *            経過時間カウンタだけを 0 に戻す（[SWS_Gpt_00361]）。
 *
 *          ISR コンテキストの制約（Hal/Gpt_Hw.cpp 参照）:
 *            実 HW タイマの割り込みは Gpt_OnTick() 経由でこのモジュール内部の
 *            状態機械を直接更新し、有効化されていれば GptNotification
 *            関数ポインタも ISR コンテキストから直接呼び出す
 *            （[SWS_Gpt_00275] の「通知は割り込みコンテキストで発生する」
 *            という仕様どおりの実装）。このため GptNotification に登録する
 *            関数は Serial 出力や Os_SchedulerStep() 等のブロッキング/重い
 *            処理を絶対に行ってはならず、volatile カウンタの更新など
 *            ISR セーフな処理のみに限定すること（src/Asw/App_GptDemo.c の
 *            App_GptDemo_OnTick() 参照）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef GPT_H
#define GPT_H

#include "Std_Types.h"
#include "Gpt_Cfg.h"
#include "Gpt_PBCfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Gpt_ChannelType / Gpt_ValueType / Gpt_ChannelMode / Gpt_NotificationPtrType
 * は Gpt_PBCfg.h との循環インクルードを避けるため Gpt_Cfg.h 側で定義する
 * （Gpt_Cfg.h 冒頭のコメント参照）。 */

/* -----------------------------------------------------------------------
 * 公開 API
 * ----------------------------------------------------------------------- */

/**
 * \brief   GPT ドライバを初期化する。
 *
 * \details 全チャネルを "initialized" 状態にし、通知を全て無効化する
 *          （[SWS_Gpt_00107]）。HW 側の準備は Gpt_Hw_Init() に委譲する。
 *
 * \param[in]  ConfigPtr  ポストビルドコンフィグへのポインタ。NULL 禁止。
 *
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Gpt_Init(const Gpt_ConfigType* ConfigPtr);

/**
 * \brief   GPT ドライバを未初期化状態に戻す。
 *
 * \details いずれかのチャネルが running 状態の場合は GPT_E_BUSY を報告し、
 *          何も行わずに戻る（[SWS_Gpt_00234]）。全チャネルが running でなければ
 *          Gpt_Hw_DeInit() を呼び、未初期化状態にする。
 *
 * \ServiceID      {0x02}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Gpt_DeInit(void);

/**
 * \brief   指定チャネルの経過時間を返す（[SWS_Gpt_00010]/[00361]）。
 *
 * \param[in]  Channel  チャネル番号。
 * \return     経過 tick 数。initialized 状態、または Channel 不正の場合は 0。
 *
 * \ServiceID      {0x03}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Gpt_ValueType Gpt_GetTimeElapsed(Gpt_ChannelType Channel);

/**
 * \brief   指定チャネルの目標時間までの残り時間を返す（[SWS_Gpt_00083]）。
 *
 * \param[in]  Channel  チャネル番号。
 * \return     残り tick 数。initialized 状態、expired (ONESHOT)、
 *             または Channel 不正の場合は 0。
 *
 * \ServiceID      {0x04}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Gpt_ValueType Gpt_GetTimeRemaining(Gpt_ChannelType Channel);

/**
 * \brief   指定チャネルのタイマを開始する（[SWS_Gpt_00274]）。
 *
 * \param[in]  Channel  チャネル番号。
 * \param[in]  Value    目標時間（tick 数）。0 は不正（GPT_E_PARAM_VALUE）。
 *
 * \note    running 状態のチャネルに対して呼ぶと GPT_E_BUSY を報告し、
 *          何も行わない（[SWS_Gpt_00084]）。
 *
 * \ServiceID      {0x05}
 * \Reentrancy     {Reentrant（チャネルが異なれば）}
 * \Synchronicity  {Synchronous}
 */
void Gpt_StartTimer(Gpt_ChannelType Channel, Gpt_ValueType Value);

/**
 * \brief   指定チャネルのタイマを停止する（[SWS_Gpt_00013]）。
 *
 * \param[in]  Channel  チャネル番号。
 *
 * \note    initialized / stopped / expired 状態で呼んでも無害
 *          （状態変化なし、[SWS_Gpt_00344]）。
 *
 * \ServiceID      {0x06}
 * \Reentrancy     {Reentrant（チャネルが異なれば）}
 * \Synchronicity  {Synchronous}
 */
void Gpt_StopTimer(Gpt_ChannelType Channel);

/**
 * \brief   指定チャネルの割り込み通知を有効化する（[SWS_Gpt_00014]）。
 *
 * \param[in]  Channel  チャネル番号。GptNotification が設定されていない
 *                       チャネルを指定すると GPT_E_PARAM_CHANNEL を報告する
 *                       （[SWS_Gpt_00377]）。
 *
 * \ServiceID      {0x07}
 * \Reentrancy     {Reentrant（チャネルが異なれば）}
 * \Synchronicity  {Synchronous}
 */
void Gpt_EnableNotification(Gpt_ChannelType Channel);

/**
 * \brief   指定チャネルの割り込み通知を無効化する（[SWS_Gpt_00015]）。
 *
 * \param[in]  Channel  チャネル番号。GptNotification が設定されていない
 *                       チャネルを指定すると GPT_E_PARAM_CHANNEL を報告する
 *                       （[SWS_Gpt_00379]）。
 *
 * \ServiceID      {0x08}
 * \Reentrancy     {Reentrant（チャネルが異なれば）}
 * \Synchronicity  {Synchronous}
 */
void Gpt_DisableNotification(Gpt_ChannelType Channel);

/**
 * \brief   GPT ドライバのバージョン情報を取得する。
 *
 * \param[out]  versioninfo  バージョン情報の格納先。NULL 禁止。
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Gpt_GetVersionInfo(Std_VersionInfoType* versioninfo);

#ifdef __cplusplus
}
#endif

#endif /* GPT_H */
