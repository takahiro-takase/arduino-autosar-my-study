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

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Std_Types.h"
#include "EcuM_Cfg.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* -----------------------------------------------------------------------
 * 型定義
 * ----------------------------------------------------------------------- */
/**
 * \brief   EcuM 動作フェーズ (AUTOSAR SWS_EcuM の EcuM_StateType に相当)
 *
 * \details フェーズ値は AUTOSAR 仕様の定義に準拠した範囲を使用している。
 *          STARTUP は EcuM_Init() 実行中のみ。
 *          SHUTDOWN は Arduino では電源断できないためアイドル待機となるが、
 *          CAN バスのウェイクアップ（CanSM_ControllerModeIndication 経由の
 *          EcuM_RequestRUN）により RUN へ復帰でき、実機リセットは不要
 *          （詳細は EcuM_RequestRUN() を参照。CanSM の Bus-Off 回復は
 *          L1/L2 バックオフで無期限に継続するため、Bus-Off が原因で
 *          この SHUTDOWN 状態に至ることはない）。
 *          Os_SchedulerStep() 自体は SHUTDOWN 中も呼ばれ続けるが、
 *          BswM Rule 2 が WdgM_MainFunction / Can_MainFunction_Read /
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
    ECUM_STATE_SHUTDOWN = 0x30U  /**< シャットダウン完了 (WdgM_MainFunction /
                                  *   Can_MainFunction_Read / Can_MainFunction_Wakeup
                                  *   以外は停止。CAN ウェイクアップで RUN へ復帰可能) */
} EcuM_StateType;

/** EcuM RUN 要求ユーザ型 (ECUM_USER_* 定数を渡す) */
typedef uint8 EcuM_UserType;

/** EcuM ウェイクアップ要因型（[SWS_EcuM_04040]、uint32 ビットフィールド）。
 *  本プロジェクトが実際に使う値は ECUM_WKSOURCE_CAN のみ（EcuM_Cfg.h 参照）。 */
typedef uint32 EcuM_WakeupSourceType;

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================
 * Global Variables
 * ====================================================================== */

/* ======================================================================
 * Functions
 * ====================================================================== */

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
 * \note       AUTOSAR 標準の SWS_EcuM には存在しない本プロジェクト独自の
 *             拡張関数（EcuM_StateType 自体が実仕様 4.2.2 以降で廃止された
 *             概念）のため、対応する \AUTOSARReq は無い。ApiId は自己割当だが、
 *             従来の 0x06 は実仕様の EcuM_SelectShutdownTarget([SWS_EcuM_02822])
 *             の正規 ServiceID と衝突していたため、未使用の 0x05 へ変更した。
 * \ServiceID      {0x05}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
EcuM_StateType EcuM_GetState(void);

/**
 * \brief   RUN フェーズの継続を要求する。
 *
 * \details POST_RUN 状態で呼ばれた場合は RUN へ戻る。
 *          SHUTDOWN 状態で呼ばれた場合も RUN へ戻る（CAN バスのウェイクアップ
 *          経由。CanSM_ControllerModeIndication() → ComM_BusSM_ModeIndication(FULL_COM) →
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
 * \brief   ウェイクアップ要因を確認する（[SWS_Can_00271]、CAN ドライバの
 *          ウェイクアップ割り込み/ポーリング文脈からの直接通知先）。
 *          旧 `CanIf_ControllerWakeup`（実仕様に存在しない独自 API だった、
 *          2026-09-05 是正）の役割を引き継ぐ。
 *
 * \details 実仕様の `EcuM_CheckWakeup()` は EcuM 本体ではなく ECU 統合側が
 *          実装する「コールアウト」（EcuM 自身がポーリング時に、CAN
 *          ドライバの ISR/Can_MainFunction_Wakeup が割り込み時にそれぞれ
 *          呼ぶ）と位置づけられるが、本プロジェクトは単一 ECU 構成で、EcuM の
 *          ウェイクアップソース管理のうち、検証待ち（PENDING）・検証済み（VALIDATED）・
 *          期限切れ（EXPIRED）の保持と、検証のタイムアウト（`ECUM_VALIDATION_TIMEOUT_MS`）だけを
 *          持つ（`EcuM_SetWakeupEvent` の代わりに本関数が、検証待ちにして、タイムアウトを起算する。
 *          BswM への通知 `BswM_EcuM_CurrentWakeup()` は持たない）ため、EcuM.c 内の通常関数として
 *          簡略実装する。
 *
 *          本プロジェクトで実際にサポートするウェイクアップ要因は CAN バス
 *          起床（`ECUM_WKSOURCE_CAN`）のみである。実際の検証は、Listen-Only
 *          によるノイズ除去（`CanSM_ControllerModeIndication()` 側の既存の
 *          ステートマシン）と、有効フレーム受信の確認（`CanIf_CheckValidation()`
 *          → `EcuM_ValidateWakeupEvent()` → `ComM_EcuM_WakeUpIndication()`
 *          → `CanSM_RequestComMode(FULL_COM)` による FULL_COM 確定）が担うため、
 *          本関数は起床要因を検証待ちとして保留し、CanSM へ委譲する。
 *
 * \param[in]  wakeupSource  検出されたウェイクアップ要因のビットマスク。
 *                           `ECUM_WKSOURCE_CAN` ビットが立っていない場合は
 *                           未知の要因として `ECUM_E_INVALID_PAR` を DET へ
 *                           報告し、何もしない（本プロジェクトが持つ
 *                           ウェイクアップ要因は CAN バス起床のみのため）。
 *
 * \pre        `Can_MainFunction_Wakeup()`（または `Can_Isr()`）の文脈から
 *             呼ぶこと（[SWS_Can_00271]）。
 *
 * \AUTOSARReq     {SWS_Can_00271}
 * \ServiceID      {0x42}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void EcuM_CheckWakeup(EcuM_WakeupSourceType wakeupSource);

/**
 * \brief   起床要因が検証されたことを EcuM へ通知する（[SWS_EcuM_02829]）。
 *
 * \details CanIf_CheckValidation() が、起床後の最初の受信を確認したときに呼ぶ
 *          （仕様の <User_ValidateWakeupEvent>、[SWS_CANIF_00179]）。
 *          検証待ち（保留中）の起床要因から、検証された要因を外し、
 *          起床したチャネルを ComM へ通知する（`ComM_EcuM_WakeUpIndication()`、
 *          [SWS_EcuM_02645]）。ComM は、チャネルが NO_COM のときに FULL_COM を
 *          CanSM へ要求し、CanSM がコントローラを起動して FULL_COM へ確定する
 *          （以前は CanIf_RxIndication() が独自 API の CanSM_RxIndication() を
 *          呼び、CanSM が自分で確定していた。2026-10 に置き換え）。
 *
 *          検証待ち（PENDING）から、検証済み（VALIDATED）へ移し、期限切れ（EXPIRED）に
 *          なっていた要因は、そこから外す（遅れて検証された場合）。検証のタイムアウトの
 *          タイマは、検証待ちが無くなると止まる（[SWS_EcuM_02710]）。本プロジェクトの EcuM は、
 *          `BswM_EcuM_CurrentWakeup()` の通知（[SWS_EcuM_04078]/[SWS_EcuM_04079]）を
 *          持たない（`EcuM_CheckWakeup()` の説明参照）。
 *
 * \param[in]  sources  検証された起床要因のビットマスク。`ECUM_WKSOURCE_CAN` 以外の
 *                       ビットを含む、または 0 の場合は、未知の要因として
 *                       `ECUM_E_UNKNOWN_WAKEUP_SOURCE`（[SWS_EcuM_02868]）を DET へ
 *                       報告し、何もしない。
 *
 * \AUTOSARReq     {SWS_EcuM_02829, SWS_EcuM_02645, SWS_EcuM_02868}
 * \ServiceID      {0x14}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void EcuM_ValidateWakeupEvent(EcuM_WakeupSourceType sources);

/**
 * \brief   検証待ち（PENDING）の起床要因を返す（[SWS_EcuM_01156]）。
 *
 * \details 起床を検出（`EcuM_CheckWakeup()`）して、まだ検証されておらず、期限切れにもなっていない要因。
 *
 * \return  検証待ちの起床要因のビットマスク（無ければ 0）。
 *
 * \AUTOSARReq     {SWS_EcuM_01156, SWS_EcuM_03003}
 * \ServiceID      {0x0d}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
EcuM_WakeupSourceType EcuM_GetPendingWakeupEvents(void);

/**
 * \brief   検証済み（VALIDATED）の起床要因を返す（[SWS_EcuM_02830]）。
 *
 * \return  検証済みの起床要因のビットマスク（無ければ 0）。
 *
 * \AUTOSARReq     {SWS_EcuM_02830}
 * \ServiceID      {0x15}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
EcuM_WakeupSourceType EcuM_GetValidatedWakeupEvents(void);

/**
 * \brief   期限切れ（EXPIRED）の起床要因を返す（[SWS_EcuM_04076]）。
 *
 * \details 検証のタイムアウト（`ECUM_VALIDATION_TIMEOUT_MS`）までに検証されなかった要因
 *          （[SWS_EcuM_04081]）。
 *
 * \return  期限切れの起床要因のビットマスク（無ければ 0）。
 *
 * \AUTOSARReq     {SWS_EcuM_04076, SWS_EcuM_04081}
 * \ServiceID      {0x19}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
EcuM_WakeupSourceType EcuM_GetExpiredWakeupEvents(void);

/**
 * \brief   指定した起床要因を、検証待ち・検証済み・期限切れのすべてから消す（[SWS_EcuM_02683]）。
 *
 * \param[in]  sources  消す起床要因のビットマスク。
 *
 * \AUTOSARReq     {SWS_EcuM_02683}
 * \ServiceID      {0x16}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void EcuM_ClearWakeupEvent(EcuM_WakeupSourceType sources);

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

/* ======================================================================
 * Callback Functions and Notifications
 * ====================================================================== */

/* ======================================================================
 * Test Functions
 * ====================================================================== */

#ifdef ECUM_UNIT_TEST
/**
 * \brief   EcuM の内部状態（状態・RUN/POST_RUN 要求ビット・POST_RUN タイマ）を
 *          初期値へ戻す（単体テスト専用）。
 *
 * \details EcuM に DeInit 相当の API が無く、状態が static のままテストケースを
 *          またいで残るため、各テストの SetUp で呼ぶ（標準外の関数。
 *          Wdg_Test_ResetInitState() と同じ設計方針）。
 */
void EcuM_Test_ResetInitState(void);

/**
 * \brief   EcuM の状態を指定の値へ直接設定する（単体テスト専用）。
 *
 * \details 実 EcuM_Init() は BSW 全モジュールを初期化して ECUM_STATE_RUN へ
 *          入るため、EcuM 単体のテストでは RUN/POST_RUN/SHUTDOWN の状態から
 *          始める手段が無い。状態以外（RUN/POST_RUN 要求ビット・タイマ）は
 *          変えない（標準外の関数。EcuM_Test_ResetInitState() と同じ設計方針）。
 *
 * \param[in]  state  設定する EcuM の状態。
 */
void EcuM_Test_SetState(EcuM_StateType state);
#endif

#ifdef __cplusplus
}
#endif

#endif /* ECUM_H */
