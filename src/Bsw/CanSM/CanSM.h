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
 *             ↓ CanIf_SetControllerMode()
 *            CanIf
 *             ↓ Can_SetControllerMode()
 *            Can
 *
 *          Bus-Off 回復シーケンス（SWS_CanSM_00514/00515 の L1/L2 バックオフ準拠）:
 *            CanIf_ControllerBusOff() → CanSM_ControllerBusOff()
 *              → コントローラ停止 → L1（短い周期）待機（CanSM_MainFunction）
 *              → コントローラ再起動 → FULL_COM 復帰
 *              → 連続失敗時は試行回数をカウントし、CANSM_BUSOFF_L1_TO_L2_COUNT
 *                回を超えたら DTC を確定した上で L2（長い周期）へ切り替えて
 *                無期限にリトライを継続する（回復を諦めて停止する状態は
 *                AUTOSAR 仕様に存在しないため実装しない）
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef CANSM_H
#define CANSM_H

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Std_Types.h"
#include "ComStack_Types.h"
#include "Can_GeneralTypes.h"
#include "ComM.h"
#include "CanSM_Cfg.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/**
 * \brief   CanSM_Init() の設定引数型（不透明型）。
 *
 * \details SWS_CanSM_00023 は post-build パラメータへのポインタを要求するが、
 *          本プロジェクトは単一 ECU 構成で post-build バリアント切替を持たない
 *          ため、中身を定義しない不透明型とし、ポインタとしてのみ扱う
 *          （`KeyM_ConfigType` と同じ簡略化パターン。KeyM.h 冒頭コメント参照）。
 */
typedef struct CanSM_ConfigType_Tag CanSM_ConfigType;

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
 * \brief   CanSM モジュールを初期化する。
 *
 * \details 全ネットワークを COMM_NO_COMMUNICATION 状態に設定する。
 *          EcuM_Init() から ComM_Init() より先に呼び出すこと。
 *
 * \param[in]  ConfigPtr  常に NULL を渡すこと（本プロジェクトは post-build 設定を
 *                        持たないため。実 AUTOSAR 仕様は SWS_CanSM_00023 で
 *                        post-build パラメータへのポインタを要求する）。
 *
 * \AUTOSARReq     {SWS_CanSM_00023}
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanSM_Init(const CanSM_ConfigType* ConfigPtr);

/**
 * \brief   CanSM モジュールを未初期化状態に戻す。
 *
 * \details 初期化済みフラグを未初期化に戻す。未初期化状態で呼ばれた場合は
 *          CANSM_E_UNINIT を報告し何もしない。
 *
 * \ServiceID      {0x14}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanSM_DeInit(void);

/**
 * \brief   ネットワークの通信モード遷移を要求する。
 *
 * \details ComM から呼び出され、要求モードに応じて
 *          CanIf_SetControllerMode() でハードウェアを制御する。
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
Std_ReturnType CanSM_RequestComMode(NetworkHandleType network, ComM_ModeType mode);

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
Std_ReturnType CanSM_GetCurrentComMode(NetworkHandleType network, ComM_ModeType* mode);

/**
 * \brief   CanSM 周期処理（Bus-Off 回復タイマ・ウェイクアップ検証タイマ管理）。
 *
 * \details OS タスク (10 ms 周期、SHUTDOWN 中も動作) から呼び出される。
 *          Bus-Off 状態のとき回復タイマを監視し、L1/L2 いずれかの周期経過後に
 *          コントローラの再起動を試みる（詳細は CanSM.c 参照）。
 *          ウェイクアップ検証中は検証タイマを監視し、タイムアウトすれば
 *          ノイズによる誤ウェイクアップとみなして再スリープさせる。
 *
 * \ServiceID      {0x05}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanSM_MainFunction(void);

/**
 * \brief   CanSM モジュールのバージョン情報を取得する。
 *
 * \details CanSM_Init と並び、未初期化時でも CANSM_E_UNINIT を報告しない
 *          例外 API（他 BSW モジュールと共通の慣例）のため、初期化状態は
 *          確認せず NULL ポインタチェックのみ行う。
 *
 * \param[out]  VersionInfo  バージョン情報の格納先。NULL 禁止。
 *
 * \ServiceID      {0x01}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanSM_GetVersionInfo(Std_VersionInfoType* VersionInfo);

/* ======================================================================
 * Callback Functions and Notifications
 * ====================================================================== */

/**
 * \brief   Bus-Off 通知コールバック（CanIf から呼び出される）。
 *
 * \details CAN コントローラが Bus-Off 状態を検出した際に CanIf 経由で
 *          呼び出される。コントローラを停止し、回復タイマを起動する。
 *          検出直後（回復試行の前）に `ComM_BusSM_ModeIndication(Network,
 *          COMM_SILENT_COMMUNICATION)` を呼び、ComM のチャネル状態を
 *          回復完了まで FULL_COM のまま放置しない（SWS_CanSM_00521。
 *          詳細は CanSM.c 参照）。
 *
 * \param[in]  ControllerId  Bus-Off を検出したコントローラ ID。
 *
 * \ServiceID      {0x04}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanSM_ControllerBusOff(uint8 ControllerId);

/**
 * \brief   コントローラモード変化通知コールバック（[SWS_CanSM_00396]）。旧
 *          `CanSM_ControllerWakeup`（2026-09-05、実仕様名・シグネチャへ
 *          是正）。
 *
 * \details 実仕様は「CAN コントローラのモードが変化したことを通知する」
 *          汎用コールバックだが、本プロジェクトは通常のモード変化を
 *          `CanIf_SetControllerMode()` の戻り値で同期的に把握する設計
 *          （CanIf.c 冒頭のコメント参照）のため、本コールバック経由の通知が
 *          必要になる場面は「CAN コントローラが CAN_CS_SLEEP 中にバス活動を
 *          検知して自律的にウェイクアップした」場合（`EcuM_CheckWakeup()`
 *          からの委譲、[SWS_Can_00271]。2026-09-05 是正前は実仕様に存在
 *          しない独自 API `CanIf_ControllerWakeup()` 経由だった）のみに
 *          限定される。「通常のスリープ（ComM の NO_COM 要求による、
 *          CANSM_STATE_NO_COM）」からの復帰のみを扱う。CANSM_STATE_BUS_OFF
 *          は実 HW をスリープさせず Can_T_STOP/Can_T_START のみで回復を
 *          試行するため、この状態から呼ばれることは原理的にない。
 *
 *          この時点ではまだ FULL_COM へ確定しない（ウェイクアップ検証、
 *          AUTOSAR EcuM Wakeup Validation Protocol 相当）。CAN_T_WAKEUP のみ
 *          実行して Listen-Only へ遷移し、検証タイマを開始する。実際に
 *          FULL_COM へ確定するかどうかは、有効な受信の確認後に ComM が呼ぶ
 *          CanSM_RequestComMode(FULL_COM) /
 *          CanSM_MainFunction()（タイムアウト）が決める。
 *
 * \param[in]  ControllerId   モードが変化したコントローラ ID。
 * \param[in]  ControllerMode 通知されたコントローラモード。本プロジェクトの
 *                            唯一の呼び出し元（`EcuM_CheckWakeup()`）は
 *                            `CAN_CS_STOPPED`（ウェイクアップ検知直後、
 *                            本関数自身がこの直後に確定させる Listen-Only
 *                            相当のモード）を渡す。上記の通り呼び出し場面が
 *                            ウェイクアップ検知のみに限定されるため、内部
 *                            ロジックはこの値では分岐しない（引数は
 *                            シグネチャ準拠のためのみ使用）。
 *
 * \note    実仕様の \Reentrancy は「Reentrant (only for different CAN
 *          controllers)」だが、本実装は `CanSM_State`/`CanSM_ValidationTimerMs`
 *          等の共有 static 変数を排他制御なしに読み書きするため、
 *          兄弟関数（`CanSM_ControllerBusOff`/
 *          `CanSM_RequestComMode` 等、いずれも `Non Reentrant`）と同様に
 *          `Non Reentrant` のまま扱う（本プロジェクトは単一コントローラ構成
 *          で ControllerId!=0 を DET 拒否するため、実仕様が言う「異なる
 *          コントローラ間」のケース自体が到達しない点も踏まえた判断）。
 *
 * \AUTOSARReq     {SWS_CanSM_00396, SWS_CanSM_00397, SWS_CanSM_00398}
 * \ServiceID      {0x07}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanSM_ControllerModeIndication(uint8 ControllerId, Can_ControllerStateType ControllerMode);

/**
 * \brief   起床の検証を始める（[SWS_CanSM_00609]）。EcuM から呼ばれる。
 *
 * \details 初期化済みで、ネットワークハンドルが正しければ、E_OK を返し、状態機械の T_START_WAKEUP_SOURCE の
 *          トリガとして扱う（[SWS_CanSM_00616]/[SWS_CanSM_00607]）。トリガが効くのは NO_COM（ボランタリスリープ中）だけで、
 *          それ以外の状態では、何もしない（要求自体は受理する）。
 *          NO_COM のとき、CAN_T_WAKEUP（SLEEP→STOPPED、Listen-Only）だけを実行して CANSM_STATE_WAKEUP_VALIDATING へ入り、
 *          検証タイマを開始する。MCP2515 の WAKIF はノイズでも誤って立ちうるため、この時点では FULL_COM へ確定しない。
 *          FULL_COM へ確定するのは、有効な受信の確認後に ComM が CanSM_RequestComMode(FULL_COM) を呼んだとき（検証成功）、
 *          または CanSM_MainFunction() が検証のタイムアウトを検出して、再スリープするとき（検証失敗）。
 *
 *          以前は、同じ処理が CanSM_ControllerModeIndication() の中にあり、EcuM_CheckWakeup() が
 *          CanSM_ControllerModeIndication(0, CAN_CS_STOPPED) を「起床の合図」として呼んでいた（2026-11 に、本関数へ移した。
 *          仕様では、CanSM_ControllerModeIndication() は、コントローラがそのモードに達した完了通知）。
 *
 * \param[in]  network  対象のネットワークハンドル（0 〜 CANSM_CHANNEL_COUNT-1）。
 *
 * \retval  E_OK      要求を受理した。
 * \retval  E_NOT_OK  未初期化、またはネットワークハンドルが範囲外。
 *
 * \AUTOSARReq     {SWS_CanSM_00609, SWS_CanSM_00607, SWS_CanSM_00616, SWS_CanSM_00617, SWS_CanSM_00612}
 * \ServiceID      {0x11}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType CanSM_StartWakeupSource(NetworkHandleType network);

/**
 * \brief   起床の検証を止める（[SWS_CanSM_00610]）。EcuM から呼ばれる。
 *
 * \details 初期化済みで、ネットワークハンドルが正しければ、E_OK を返し、状態機械の T_STOP_WAKEUP_SOURCE の
 *          トリガとして扱う（[SWS_CanSM_00622]/[SWS_CanSM_00608]）。トリガが効くのは WAKEUP_VALIDATING（検証中）だけで、
 *          コントローラを再びスリープさせて、NO_COM へ戻る（検証失敗と同じ。CanSM_MainFunction() の検証タイムアウトと
 *          共通の処理）。それ以外の状態では、何もしない（要求自体は受理する）。
 *          本プロジェクトの EcuM は、現在この関数を呼ばない（検証の失敗は、CanSM_MainFunction() の検証タイムアウトが
 *          自分で検出して、再スリープする）。
 *
 * \param[in]  network  対象のネットワークハンドル（0 〜 CANSM_CHANNEL_COUNT-1）。
 *
 * \retval  E_OK      要求を受理した。
 * \retval  E_NOT_OK  未初期化、またはネットワークハンドルが範囲外。
 *
 * \AUTOSARReq     {SWS_CanSM_00610, SWS_CanSM_00608, SWS_CanSM_00618, SWS_CanSM_00619, SWS_CanSM_00620, SWS_CanSM_00622}
 * \ServiceID      {0x12}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType CanSM_StopWakeupSource(NetworkHandleType network);

/* ======================================================================
 * Test Functions
 * ====================================================================== */

#ifdef __cplusplus
}
#endif

#endif /* CANSM_H */
