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

#include "Std_Types.h"
#include "ComStack_Types.h"
#include "ComM.h"
#include "CanSM_Cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   CanSM_Init() の設定引数型（不透明型）。
 *
 * \details SWS_CanSM_00023 は post-build パラメータへのポインタを要求するが、
 *          本プロジェクトは単一 ECU 構成で post-build バリアント切替を持たない
 *          ため、中身を定義しない不透明型とし、ポインタとしてのみ扱う
 *          （`KeyM_ConfigType` と同じ簡略化パターン。KeyM.h 冒頭コメント参照）。
 */
typedef struct CanSM_ConfigType_Tag CanSM_ConfigType;

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
 * \brief   ウェイクアップ通知コールバック（CanIf から呼び出される）。
 *
 * \details CAN コントローラが CAN_CS_SLEEP 中にバス活動を検知して自律的に
 *          ウェイクアップした際に CanIf 経由で呼び出される。
 *          「通常のスリープ（ComM の NO_COM 要求による、CANSM_STATE_NO_COM）」
 *          からの復帰のみを扱う。CANSM_STATE_BUS_OFF は実 HW をスリープさせず
 *          Can_T_STOP/Can_T_START のみで回復を試行するため、この状態から
 *          呼ばれることは原理的にない。
 *
 *          この時点ではまだ FULL_COM へ確定しない（ウェイクアップ検証、
 *          AUTOSAR EcuM Wakeup Validation Protocol 相当）。CAN_T_WAKEUP のみ
 *          実行して Listen-Only へ遷移し、検証タイマを開始する。実際に
 *          FULL_COM へ確定するかどうかは CanSM_RxIndication() /
 *          CanSM_MainFunction() が決める。
 *
 * \param[in]  ControllerId  ウェイクアップを検出したコントローラ ID。
 *
 * \ServiceID      {0x06}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanSM_ControllerWakeup(uint8 ControllerId);

/**
 * \brief   受信通知コールバック（CanIf から全受信フレームについて呼び出される）。
 *
 * \details AUTOSAR SWS_CanSM の CanSMRxIndicationUsed 設定に相当。
 *          ウェイクアップ検証中 (CANSM_STATE_WAKEUP_VALIDATING) にのみ意味を持ち、
 *          有効な CAN フレームの受信をもって直前のウェイクアップがノイズでは
 *          ないと判断し、FULL_COM へ確定させる。それ以外の状態では何もしない。
 *
 * \param[in]  ControllerId  受信したコントローラ ID。
 *
 * \ServiceID      {0x07}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanSM_RxIndication(uint8 ControllerId);

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

#ifdef __cplusplus
}
#endif

#endif /* CANSM_H */
