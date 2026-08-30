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

/** ComM モジュール自身の初期化状態型 (AUTOSAR ComM_InitStatusType) */
typedef uint8 ComM_InitStatusType;

#define COMM_UNINIT  0U  /**< ComM は未初期化（Init 前、または DeInit 後） */
#define COMM_INIT    1U  /**< ComM は初期化済みで使用可能 */

/**
 * \brief   ComM_Init() の設定引数型（不透明型）。
 *
 * \details SWS_ComM_00146 は post-build 設定データへのポインタを要求するが、
 *          本プロジェクトは単一 ECU 構成で post-build バリアント切替を持たない
 *          ため、中身を定義しない不透明型とし、ポインタとしてのみ扱う
 *          （`KeyM_ConfigType` と同じ簡略化パターン。KeyM.h 冒頭コメント参照）。
 */
typedef struct ComM_ConfigType_Tag ComM_ConfigType;

/**
 * \brief   ComM モジュールを初期化する。
 *
 * \details 全チャネルを COMM_NO_COMMUNICATION 状態に設定する。
 *          CAN バスはまだアクティブにならない。
 *          EcuM_Init() から Can_Init() の後に呼び出すこと。
 *
 * \param[in]  ConfigPtr  常に NULL を渡すこと（本プロジェクトは post-build 設定を
 *                        持たないため）。
 *
 * \AUTOSARReq     {SWS_ComM_00146}
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void ComM_Init(const ComM_ConfigType* ConfigPtr);

/**
 * \brief   ComM モジュールを未初期化状態に戻す。
 *
 * \details 初期化済みフラグを未初期化に戻す。未初期化状態で呼ばれた場合は
 *          COMM_E_UNINIT を報告し何もしない。
 *
 * \ServiceID      {0x02}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void ComM_DeInit(void);

/**
 * \brief   ComM モジュールの初期化状態を取得する。
 *
 * \details [SWS_ComM_00612/00858] が明記するとおり、ComM_Init/GetVersionInfo と
 *          並び、本関数だけは未初期化状態で呼ばれても COMM_E_UNINIT を報告
 *          しない例外 API である。そのため初期化状態は確認せず、NULL
 *          ポインタチェックのみ行う。
 *
 * \param[out]  Status  COMM_UNINIT/COMM_INIT の格納先。NULL 禁止。
 *
 * \retval  E_OK      正常に取得した。
 * \retval  E_NOT_OK  Status が NULL。
 *
 * \AUTOSARReq     {SWS_ComM_00242}
 * \ServiceID      {0x03}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType ComM_GetStatus(ComM_InitStatusType* Status);

/**
 * \brief   ユーザが通信モードを要求する。
 *
 * \details 要求モードに応じてチャネルの状態遷移を行う。複数ユーザからの要求は
 *          最高優先モードに調停する (AUTOSAR SWS_ComM_00686、"highest wins" 戦略)。
 *
 *          FULL_COM -> NO_COM の場合のみ特別扱いする（[SWS_ComM_00133]）:
 *          Can_SetControllerMode() は呼ばず、Nm_NetworkRelease() のみを送って
 *          即座に E_OK を返す。実際の CanSM_RequestComMode() 呼び出しと
 *          ComM_ChannelMode の更新は、Nm が協調スリープを完了して
 *          ComM_Nm_BusSleepMode() を呼ぶまで遅延する（ComM.c ファイル冒頭
 *          コメント参照）。SILENT_COM 中（Nm が既に Prepare Bus-Sleep Mode
 *          へ到達済み）に NO_COM が再要求された場合も同様に何もしない
 *          （CanSM へ素通りさせると Nm 未到達のまま物理スリープしてしまう
 *          回帰があったため、2026-08 に修正。ComM.c 参照）。それ以外の遷移
 *          （NO_COM -> FULL_COM 等）は従来どおり即座に CanSM_RequestComMode()
 *          へ転送する。
 *
 * \param[in]  User     要求するユーザ ID (COMM_USER_0 等)。
 * \param[in]  ComMode  要求する通信モード
 *                      (COMM_NO_COMMUNICATION / COMM_SILENT_COMMUNICATION /
 *                       COMM_FULL_COMMUNICATION)。
 *
 * \retval  E_OK      モード遷移（または Nm への解放要求送信）を受理した。
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
 * \ServiceID      {0x08}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType ComM_GetCurrentComMode(ComM_UserHandleType User, ComM_ModeType* ComMode);

/**
 * \brief   ComM 周期処理（バス通信状態の監視）。
 *
 * \details 意図的な NOP。[SWS_ComM_00888] のとおり `ComMNmVariant=FULL`
 *          （本プロジェクトのように Nm_NetworkRequest()/Nm_NetworkRelease() を
 *          能動的に呼び、CanNm の協調スリープでチャタリングを防止する構成）
 *          では ComMTMinFullComModeDuration ヒステリシスタイマは不要と
 *          明記されている。詳細な根拠は ComM.c の実装コメント参照。
 *
 * \AUTOSARReq     {SWS_ComM_00888}
 * \ServiceID      {0x60}
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
 *          DET 相当で検知する（SWS_EcuM_04125/04127）ため、実際に EcuM の
 *          RUN 要求状態（内部変数 ComM_EcuMRunMode。生のチャネルモードとは
 *          別に持つ理由は ComM.c 参照）が変化する時のみこれらを呼び出す
 *          （Bus-Off L1/L2 バックオフのリトライ成功のたびに本関数が FULL_COM
 *          で呼ばれても、また Bus-Off 検出〜回復の間に一時的な
 *          COMM_SILENT_COMMUNICATION を挟んでも、EcuM の RUN 状態が実際には
 *          変化していなければ EcuM へは再通知しない）。
 *          あわせて `ComM_UserRequest[COMM_USER_0]`
 *          をこの Mode に同期させる（どのユーザの要求でもない自動的な状態変化を
 *          User0 の要求として扱うことで、次回の集約計算に古い要求が
 *          残らないようにするため。詳細は ComM.c の実装コメントを参照）。
 *
 *          FULL_COM 通知時、`ComM_NmReleasePending[]` が立っていれば
 *          （Nm 協調スリープ待ちの最中に Bus-Off が発生し、回復して CanSM が
 *          FULL_COM へ戻ってきたケース）Nm_NetworkRequest() は呼ばず、
 *          代わりに CanSM_RequestComMode(NO_COM) を再度呼んで解放を
 *          仕切り直す（詳細は ComM.c の実装コメント参照）。
 *
 *          呼び出しタイミング:
 *            - CanSM_RequestComMode 成功後 (NO_COM -> FULL_COM 遷移時、および
 *              Nm が協調スリープを完了して ComM_Nm_BusSleepMode() 経由で
 *              呼ばれた NO_COM 確定時)
 *            - Bus-Off 検出時・回復試行時 (SILENT_COMMUNICATION / FULL_COMMUNICATION)
 *            - ウェイクアップ検証成功時 (FULL_COM)
 *
 * \param[in]  Network  ネットワークハンドル (0 〜 COMM_CHANNEL_COUNT-1)。
 * \param[in]  Mode     新しい通信モード (ComM_ModeType)。
 *
 * \AUTOSARReq     {SWS_ComM_00675}
 * \ServiceID      {0x33}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void ComM_BusSM_ModeIndication(uint8 Network, ComM_ModeType Mode);

/**
 * \brief   Nm が Prepare Bus-Sleep Mode へ入ったことの通知（Nm から呼び出される）。
 *
 * \details [SWS_ComM_00826]。COMM_FULL_COMMUNICATION 中に呼ばれた場合のみ
 *          CanSM_RequestComMode(Network, COMM_SILENT_COMMUNICATION) を呼び、
 *          チャネルを受信専用へ切り替える（既に FULL_COM でない、例えば
 *          Bus-Off 中は no-op）。詳細は ComM.c ファイル冒頭コメント参照。
 *
 * \param[in]  Network  ネットワークハンドル（0 〜 COMM_CHANNEL_COUNT-1）。
 *
 * \AUTOSARReq     {SWS_ComM_00391, SWS_ComM_00826}
 * \ServiceID      {0x19}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void ComM_Nm_PrepareBusSleepMode(uint8 Network);

/**
 * \brief   Nm が Network Mode へ（再）入ったことの通知（Nm から呼び出される）。
 *
 * \details [SWS_ComM_00296]。典型的には、Prepare Bus-Sleep Mode 中に他ノードの
 *          NM フレームを受信して Nm が自律的にスリープを取りやめたケース
 *          （[SWS_CanNm_00124]）。COMM_FULL_COMMUNICATION でない場合、
 *          CanSM_RequestComMode(Network, COMM_FULL_COMMUNICATION) を呼ぶ
 *          **前に** ComM_NmReleasePending[Network] を **無条件で**（この
 *          呼び出しの成否に関わらず）クリアする。この無条件クリア自体が
 *          安全性の根拠のため、「成功時のみクリア」には変更しないこと
 *          （理由の詳細は ComM.c ファイル冒頭コメント参照）。
 *
 * \param[in]  Network  ネットワークハンドル（0 〜 COMM_CHANNEL_COUNT-1）。
 *
 * \AUTOSARReq     {SWS_ComM_00390, SWS_ComM_00296}
 * \ServiceID      {0x18}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void ComM_Nm_NetworkMode(uint8 Network);

/**
 * \brief   Nm が Bus-Sleep Mode へ到達したことの通知（Nm から呼び出される）。
 *
 * \details [SWS_ComM_00392]。ComM_RequestComMode() が FULL_COM -> NO_COM の
 *          要求時に送った Nm_NetworkRelease() を受けて Nm が協調スリープ
 *          （Repeat Message → Ready Sleep → Prepare Bus-Sleep → Bus-Sleep Mode）
 *          を完了したときに呼ばれる。ここで初めて CanSM_RequestComMode(NO_COM)
 *          を呼び、物理スリープと ComM_ChannelMode の更新を行う
 *          （[SWS_ComM_00637]。詳細は ComM.c ファイル冒頭コメント参照）。
 *
 * \param[in]  Network  ネットワークハンドル（0 〜 COMM_CHANNEL_COUNT-1）。
 *
 * \AUTOSARReq     {SWS_ComM_00392, SWS_ComM_00637}
 * \ServiceID      {0x1a}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void ComM_Nm_BusSleepMode(uint8 Network);

/**
 * \brief   ComM モジュールのバージョン情報を取得する。
 *
 * \details ComM_Init と並び、未初期化時でも COMM_E_UNINIT を報告しない
 *          例外 API（他 BSW モジュールと共通の慣例）のため、初期化状態は
 *          確認せず NULL ポインタチェックのみ行う。
 *
 * \param[out]  Versioninfo  バージョン情報の格納先。NULL 禁止。
 *
 * \ServiceID      {0x10}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void ComM_GetVersionInfo(Std_VersionInfoType* Versioninfo);

#ifdef __cplusplus
}
#endif

#endif /* COMM_H */
