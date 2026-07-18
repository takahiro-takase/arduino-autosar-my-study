/**
 * \file    EcuM.h
 * \brief   ECU ステートマネージャ 公開インタフェース (AUTOSAR SWS_EcuStateManager 準拠)
 * \details ECU 全体の起動シーケンスと周期処理をカプセル化する。
 *          実際の AUTOSAR EcuM は STARTUP / RUN / POST_RUN / SLEEP / SHUTDOWN
 *          の各フェーズを管理するが、本実装では Arduino 向けに
 *          EcuM_Init() と EcuM_MainFunction() の 2 関数に簡略化している。
 *          呼び出し側（main.cpp）は EcuM.h だけをインクルードすれば
 *          BSW の詳細を知らずにシステムを起動・運転できる。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef ECUM_H
#define ECUM_H

#include "Std_Types.h"
#include "EcuM_Cfg.h"

/* -----------------------------------------------------------------------
 * 型定義
 * ----------------------------------------------------------------------- */

/**
 * \brief   EcuM 動作フェーズ (AUTOSAR SWS_EcuM の EcuM_StateType に相当)
 *
 * \details フェーズ値は AUTOSAR 仕様の定義に準拠した範囲を使用している。
 *          STARTUP は EcuM_Init() 実行中のみ。
 *          SHUTDOWN は Arduino では電源断できないためアイドル待機となるが、
 *          CAN バスのウェイクアップ（CanSM_ControllerWakeup 経由の
 *          EcuM_RequestRUN）により RUN へ復帰でき、実機リセットは不要
 *          （詳細は EcuM_RequestRUN() を参照。CanSM の Bus-Off 回復は
 *          L1/L2 バックオフで無期限に継続するため、Bus-Off が原因で
 *          この SHUTDOWN 状態に至ることはない）。
 *          Os_SchedulerStep() 自体は SHUTDOWN 中も呼ばれ続けるが、
 *          BswM Rule 2 が WdgM_TriggerHwWatchdog / Can_MainFunction_Read /
 *          Can_MainFunction_Wakeup 以外の全タスクを無効化するため実質的に
 *          アイドル状態になる（HW ウォッチドッグ維持と CAN ウェイクアップ
 *          検出・検証中フレーム処理のためだけに、この 3 タスクだけは
 *          動かし続ける。詳細は BswM_Cfg.h を参照。CAN 受信自体は真の
 *          ハードウェア割り込み (Can_Isr()) のため BswM の無効化に関わらず
 *          常に起動する）。
 */
typedef enum
{
    ECUM_STATE_STARTUP  = 0x00U, /**< 初期化フェーズ (EcuM_Init 実行中)        */
    ECUM_STATE_RUN      = 0x10U, /**< 通常動作フェーズ (RUN ユーザが存在する)  */
    ECUM_STATE_POST_RUN = 0x20U, /**< 終了移行フェーズ (全 RUN ユーザ解放後)   */
    ECUM_STATE_SHUTDOWN = 0x30U  /**< シャットダウン完了 (WdgM_TriggerHwWatchdog /
                                  *   Can_MainFunction_Read / Can_MainFunction_Wakeup
                                  *   以外は停止。CAN ウェイクアップで RUN へ復帰可能) */
} EcuM_StateType;

/** EcuM RUN 要求ユーザ型 (ECUM_USER_* 定数を渡す) */
typedef uint8 EcuM_UserType;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   BSW スタック全体を起動フェーズ順に初期化する。
 *
 * \details AUTOSAR EcuM の StartupTwo フェーズに相当し、
 *          CAN ドライバ → CAN インタフェース → PDU ルータ →
 *          COM → RTE (SW-C 初期化) の順で各モジュールの _Init を呼び出す。
 *          Serial.begin() のような Arduino 固有の初期化は
 *          呼び出し側 setup() で事前に完了しておくこと。
 *
 * \pre        Arduino ランタイムが初期化済みであること（setup() の先頭で呼ぶ想定）。
 * \note       AUTOSAR EcuM では StartupOne (OS 起動前) と
 *             StartupTwo (OS 起動後) に分かれるが、本実装では一本化している。
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void EcuM_Init(void);

/**
 * \brief   BSW スタックの周期処理を実行する。
 *
 * \details CAN 受信割り込みペンディングのドレイン（Can_MainFunction_Read）と
 *          RTE Runnable スケジューリング（Rte_ScheduleRunnables）を呼び出す。
 *          AUTOSAR OS 環境では OsTask として周期起動されるが、
 *          本実装では Arduino の loop() から毎ループ呼び出す。
 *
 * \pre        EcuM_Init() が正常完了していること。
 * \note       AUTOSAR 標準の EcuM_MainFunction は主に状態遷移管理を行うが、
 *             本実装では BSW ポーリングと RTE スケジューリングを担う。
 *
 * \ServiceID      {0x18}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void EcuM_MainFunction(void);

/**
 * \brief   現在の EcuM フェーズを返す。
 *
 * \return  EcuM_StateType (STARTUP / RUN / POST_RUN / SHUTDOWN)
 *
 * \ServiceID      {0x06}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
EcuM_StateType EcuM_GetState(void);

/**
 * \brief   RUN フェーズの継続を要求する。
 *
 * \details POST_RUN 状態で呼ばれた場合は RUN へ戻る。
 *          SHUTDOWN 状態で呼ばれた場合も RUN へ戻る（CAN バスのウェイクアップ
 *          経由。CanSM_ControllerWakeup() → ComM_BusSMIndication(FULL_COM) →
 *          本関数、という経路を想定）。
 *          STARTUP 状態で呼ばれた場合はビットのみ記録し、
 *          EcuM_Init() 完了時に RUN へ遷移する。
 *          SWS_EcuM_04125: 要求はネストできない。同一ユーザが既に RUN を
 *          要求中に再度呼ぶと DET 相当 (ECUM_E_MULTIPLE_RUN_REQUESTS) を
 *          ログ出力し E_NOT_OK を返す（呼び出し元 ComM_BusSMIndication() は
 *          チャネルモードが実際に変化した時のみ本関数を呼ぶことで、この
 *          重複自体をなるべく避けている）。
 *
 * \param[in]  user  要求ユーザ (ECUM_USER_* 定数)。
 *
 * \retval  E_OK      受理した。
 * \retval  E_NOT_OK  user が範囲外、またはこのユーザが既に RUN を要求中
 *                    (ECUM_E_MULTIPLE_RUN_REQUESTS)。
 *
 * \ServiceID      {0x03}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType EcuM_RequestRUN(EcuM_UserType user);

/**
 * \brief   RUN フェーズの継続要求を解放する。
 *
 * \details 全ユーザが解放した場合 POST_RUN へ遷移し、
 *          ECUM_POST_RUN_TIMEOUT_MS 経過後に SHUTDOWN へ移行する。
 *          SWS_EcuM_04127: 対応する要求がないユーザの解放は DET 相当
 *          (ECUM_E_MISMATCHED_RUN_RELEASE) をログ出力し E_NOT_OK を返す。
 *
 * \param[in]  user  解放するユーザ (ECUM_USER_* 定数)。
 *
 * \retval  E_OK      受理した。
 * \retval  E_NOT_OK  user が範囲外、またはこのユーザに対応する RUN 要求が
 *                    存在しない (ECUM_E_MISMATCHED_RUN_RELEASE)。
 *
 * \ServiceID      {0x04}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType EcuM_ReleaseRUN(EcuM_UserType user);

#ifdef __cplusplus
}
#endif

#endif /* ECUM_H */
