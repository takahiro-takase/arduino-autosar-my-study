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
 * \ServiceID      {0x01}
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
 *          経由。CanSM_ControllerWakeup() → ComM_BusSM_ModeIndication(FULL_COM) →
 *          本関数、という経路を想定）。
 *          STARTUP 状態で呼ばれた場合はビットのみ記録し、
 *          EcuM_Init() 完了時に RUN へ遷移する。
 *          SWS_EcuM_04125: 要求はネストできない。同一ユーザが既に RUN を
 *          要求中に再度呼ぶと DET 相当 (ECUM_E_MULTIPLE_RUN_REQUESTS) を
 *          ログ出力し E_NOT_OK を返す（呼び出し元 ComM_BusSM_ModeIndication() は
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

/**
 * \brief   POST_RUN フェーズの継続を要求する。
 *
 * \details [SWS_EcuM_04128]: RUN 要求とは独立して追跡される
 *          （本実装では `EcuM_RunUsers` とは別の `EcuM_PostRunUsers`
 *          ビットマスクで管理する）。POST_RUN 中に1件でも要求が残っていれば
 *          `ECUM_POST_RUN_TIMEOUT_MS` のタイムアウトを保留し、SHUTDOWN への
 *          自動遷移を防ぐ。RUN 中に呼んだ場合はビットを記録するのみで、
 *          その場での状態遷移は起きない（後で自然に POST_RUN へ遷移した際に
 *          反映される）。
 *          [SWS_EcuM_04125]相当のネスト禁止規則（EcuM_RequestRUN と同一の
 *          エラーコード ECUM_E_MULTIPLE_RUN_REQUESTS を流用）も同様に適用する。
 *
 * \param[in]  user  要求ユーザ (ECUM_USER_* 定数)。
 *
 * \retval  E_OK      受理した。
 * \retval  E_NOT_OK  user が範囲外、またはこのユーザが既に POST_RUN を要求中。
 *
 * \note    実仕様は Reentrant と規定するが、本実装は `EcuM_RequestRUN`/
 *          `EcuM_ReleaseRUN` と同じ無保護のビットマスク読み書きのため、
 *          実際の並行呼び出し安全性に合わせ Non Reentrant として文書化する
 *          （既存の RUN 系 API と同じ簡略化方針。EcuM.h 冒頭の既存注記参照）。
 *
 * \AUTOSARReq     {SWS_EcuM_04128}
 * \ServiceID      {0x0a}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType EcuM_RequestPOST_RUN(EcuM_UserType user);

/**
 * \brief   POST_RUN フェーズの継続要求を解放する。
 *
 * \details [SWS_EcuM_04129]。最後の1件を解放した瞬間から改めて
 *          `ECUM_POST_RUN_TIMEOUT_MS` のタイムアウトを起算する
 *          （解放が遅れた分だけ SHUTDOWN までの猶予を必ず確保するため）。
 *          [SWS_EcuM_04127]相当のエラーコード ECUM_E_MISMATCHED_RUN_RELEASE
 *          を `EcuM_ReleaseRUN` と共用する。
 *
 * \param[in]  user  解放するユーザ (ECUM_USER_* 定数)。
 *
 * \retval  E_OK      受理した。
 * \retval  E_NOT_OK  user が範囲外、またはこのユーザに対応する POST_RUN
 *                    要求が存在しない。
 *
 * \note    実仕様は Reentrant と規定するが、`EcuM_RequestPOST_RUN` と同じ
 *          理由で Non Reentrant として文書化する。
 *
 * \AUTOSARReq     {SWS_EcuM_04129}
 * \ServiceID      {0x0b}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType EcuM_ReleasePOST_RUN(EcuM_UserType user);

/**
 * \brief   EcuM モジュールのバージョン情報を取得する。
 *
 * \details EcuM_Init と並び、未初期化時でもエラー報告しない例外 API
 *          （他 BSW モジュールと共通の慣例）のため、初期化状態は確認せず
 *          NULL ポインタチェックのみ行う。
 *
 * \param[out]  versioninfo  バージョン情報の格納先。NULL 禁止。
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void EcuM_GetVersionInfo(Std_VersionInfoType* versioninfo);

#ifdef __cplusplus
}
#endif

#endif /* ECUM_H */
