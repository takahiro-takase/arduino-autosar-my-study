/**
 * \file    CanNm.h
 * \brief   ネットワークマネジメント 公開インタフェース (AUTOSAR SWS_CANNM 準拠)
 * \details 本 ECU（メータ ECU）の CanNm 状態機械（Network Mode の Repeat
 *          Message/Normal Operation/Ready Sleep の3内部状態、Prepare
 *          Bus-Sleep Mode、Bus-Sleep Mode）を実装する。
 *          実車の CanNm は Com スタックを経由せず CanIf_Transmit()/
 *          CanIf_RxIndication() を直接やり取りするため、本モジュールも
 *          PduR/Com を介さない。
 *
 *          呼び出し関係（ComM が「通信の要否」を、汎用 Nm 層（`src/Bsw/Nm/`、
 *          [[AUTOSAR_SWS_NetworkManagementInterface.pdf]] 準拠）経由で
 *          CanNm に伝える。実 AUTOSAR の階層 ComM→Nm→CanNm→CanIf をそのまま
 *          反映しており、Nm 自身は単一チャネル構成のためほぼ素通しの薄い層）:
 *            NO_COM 方向: ComM_RequestComMode() がユーザ要求を FULL_COM から
 *            NO_COM へ集約した「その場で」Nm_NetworkRelease() を呼び、Nm が
 *            CanNm_NetworkRelease() へ委譲する（[SWS_ComM_00133] 準拠。ComM の
 *            チャネルモードはこの時点ではまだ FULL_COM のまま。ComM.c
 *            ファイル冒頭コメント参照）。
 *            FULL_COM 方向: ComM_BusSM_ModeIndication() がチャネルモードを
 *            FULL_COM へ確定させたときに Nm_NetworkRequest() を呼び、Nm が
 *            CanNm_NetworkRequest() へ委譲する。以降の実際の送受信・タイマ
 *            管理・状態遷移は本モジュールが自律的に行う（ComM は毎周期
 *            ポーリングしない）。
 *
 *          Bus-Sleep Mode への到達通知（協調スリープの要）:
 *            CanNm が実際に Bus-Sleep Mode へ遷移した瞬間、`Nm_BusSleepMode()`
 *            （[SWS_Nm_00162]）を呼び、Nm が ComM_Nm_BusSleepMode()
 *            （[SWS_ComM_00392]）へ転送する。ComM はこれを受けて初めて
 *            CanSM_RequestComMode(NO_COM) を呼び、CanSM が
 *            Can_SetControllerMode(CAN_T_SLEEP) で実際に CAN コントローラを
 *            スリープさせる（ComM.c/CanSM.c 参照）。これにより、他ノード
 *            （仮想他ECU）が NM フレームを送信し続けている間は本 ECU が
 *            実際にはスリープしないことを実機で確認できる。
 *
 *          使い方:
 *            1. EcuM_Init 内で CanNm_Init() を呼ぶ（ComM_Init 完了後）。
 *               続けて Nm_Init() を呼ぶ（Nm.c 参照。ComM_RequestComMode() より
 *               必ず前に置くこと）。
 *            2. Os スケジューラが CANNM_CYCLE_MS ごとに CanNm_MainFunction() を呼ぶ
 *               （タイマ満了判定・PDU 再送信。Nm_MainFunction() は NM
 *               Coordinator 機能専用のため本プロジェクトには存在しない、
 *               Nm_Cfg.h 冒頭コメント参照）。
 *            3. CanIf が CAN 0x400 受信のたびに CanNm_RxIndication() を、
 *               送信完了のたびに CanNm_TxConfirmation() を呼ぶ。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef CANNM_H
#define CANNM_H

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Platform_Types.h"
#include "Std_Types.h"
#include "ComStack_Types.h"
#include "CanNm_Cfg.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/**
 * \brief   CanNm の3つの操作モード ([SWS_CanNm_00092])。
 */
typedef enum
{
    CANNM_MODE_BUS_SLEEP = 0,        /**< Bus-Sleep Mode */
    CANNM_MODE_PREPARE_BUS_SLEEP,    /**< Prepare Bus-Sleep Mode */
    CANNM_MODE_NETWORK               /**< Network Mode（内部状態は CanNm_StateType 参照） */
} CanNm_ModeType;

/**
 * \brief   CanNm の内部状態（[SWS_CanNm_00094] の Network Mode 内部3状態を含む）。
 */
typedef enum
{
    CANNM_STATE_BUS_SLEEP = 0,        /**< Bus-Sleep Mode */
    CANNM_STATE_PREPARE_BUS_SLEEP,    /**< Prepare Bus-Sleep Mode */
    CANNM_STATE_REPEAT_MESSAGE,       /**< Network Mode: Repeat Message State */
    CANNM_STATE_NORMAL_OPERATION,     /**< Network Mode: Normal Operation State */
    CANNM_STATE_READY_SLEEP           /**< Network Mode: Ready Sleep State */
} CanNm_StateType;

/**
 * \brief   CanNm_Init() の設定引数型（不透明型）。
 *
 * \details SWS_CanNm_00208 は post-build 設定構造体へのポインタを要求するが、
 *          本プロジェクトは単一 ECU 構成で post-build バリアント切替を
 *          持たないため、中身を定義しない不透明型とし、ポインタとしてのみ
 *          扱う（`CanSM_ConfigType`/`KeyM_ConfigType` と同じ簡略化パターン）。
 */
typedef struct CanNm_ConfigType_Tag CanNm_ConfigType;

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
 * \brief   CanNm モジュールを初期化する。Bus-Sleep Mode から開始する。
 *
 * \pre        CanIf_Init() / ComM_Init() が正常に完了していること。
 *
 * \param[in]  ConfigPtr  常に NULL を渡すこと（本プロジェクトは post-build
 *                        設定を持たないため。実 AUTOSAR 仕様は
 *                        SWS_CanNm_00208 で `CanNm_Init(const CanNm_ConfigType*
 *                        cannmConfigPtr)` を要求する）。
 *
 * \AUTOSARReq     {SWS_CanNm_00208}
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanNm_Init(const CanNm_ConfigType* ConfigPtr);

/**
 * \brief   CanNm モジュールを未初期化状態に戻す。
 *
 * \ServiceID      {0x10}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanNm_DeInit(void);

/**
 * \brief   パッシブ起動。他ノードの通信による起床を、Network Mode へ反映する（[SWS_CanNm_00211]）。
 *
 * \details Bus-Sleep Mode / Prepare Bus-Sleep Mode から呼ばれたとき、Repeat Message State へ入る
 *          （[SWS_CanNm_00128]）。CanNm_NetworkRequest() とは違い、ネットワークの要求は立てない
 *          （CanNm_NetworkRequested は変えない）。そのため、Repeat Message Time が満了すると
 *          Normal Operation State ではなく Ready Sleep State へ進み、誰も通信を要求しなければ、
 *          そのままスリープへ向かう。既に Network Mode のときは何もせず、E_NOT_OK を返す
 *          （[SWS_CanNm_00147]）。
 *
 * \param[in]  nmChannelHandle  NM チャネルハンドル（CANNM_MAIN_NETWORK_HANDLE 以外は拒否）。
 *
 * \retval  E_OK      Repeat Message State へ入った。
 * \retval  E_NOT_OK  未初期化、チャネルが不正、または既に Network Mode。
 *
 * \AUTOSARReq     {SWS_CanNm_00211, SWS_CanNm_00128, SWS_CanNm_00147, SWS_CanNm_00254}
 * \ServiceID      {0x01}
 * \Reentrancy     {Reentrant (but not for the same NM-channel)}
 * \Synchronicity  {Asynchronous}
 */
Std_ReturnType CanNm_PassiveStartUp(NetworkHandleType nmChannelHandle);

/**
 * \brief   通信が必要であることを CanNm へ伝える（[SWS_CanNm_00104] 相当）。
 *
 * \details Bus-Sleep/Prepare Bus-Sleep Mode から呼ばれた場合は Repeat Message
 *          State へ、Ready Sleep State から呼ばれた場合は Normal Operation
 *          State へ遷移する。既に Repeat Message/Normal Operation State なら
 *          何もしない（冪等）。
 *
 * \param[in]  Channel  NM チャネルハンドル（CANNM_MAIN_NETWORK_HANDLE 以外は拒否）。
 *
 * \retval  E_OK      要求を受理した。
 * \retval  E_NOT_OK  未初期化、または Channel が不正。
 *
 * \AUTOSARReq     {SWS_CanNm_00213, SWS_CanNm_00192}
 * \ServiceID      {0x02}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType CanNm_NetworkRequest(NetworkHandleType Channel);

/**
 * \brief   通信が不要になったことを CanNm へ伝える（[SWS_CanNm_00105] 相当）。
 *
 * \details Normal Operation State から呼ばれた場合は Ready Sleep State へ
 *          遷移する（NM フレーム送信を停止するが、NM-Timeout Timer が
 *          満了するまでは Prepare Bus-Sleep Mode へは移行しない）。
 *
 * \param[in]  Channel  NM チャネルハンドル（CANNM_MAIN_NETWORK_HANDLE 以外は拒否）。
 *
 * \retval  E_OK      要求を受理した。
 * \retval  E_NOT_OK  未初期化、または Channel が不正。
 *
 * \AUTOSARReq     {SWS_CanNm_00214, SWS_CanNm_00192}
 * \ServiceID      {0x03}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType CanNm_NetworkRelease(NetworkHandleType Channel);

/**
 * \brief   Repeat Message State への遷移を要求する（[SWS_CanNm_00120] 相当）。
 *
 * \details 本プロジェクトでは診断・デバッグ用途を想定するのみで、通常の
 *          運用フローからは呼ばない。Repeat Message State/Prepare
 *          Bus-Sleep Mode/Bus-Sleep Mode から呼ばれた場合は無視する
 *          （[SWS_CanNm_00137]）。
 *
 * \param[in]  Channel  NM チャネルハンドル（CANNM_MAIN_NETWORK_HANDLE 以外は拒否）。
 *
 * \retval  E_OK      Repeat Message State へ遷移した。
 * \retval  E_NOT_OK  未初期化、Channel が不正、または現在の状態では受理できない。
 *
 * \AUTOSARReq     {SWS_CanNm_00221, SWS_CanNm_00192}
 * \ServiceID      {0x08}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType CanNm_RepeatMessageRequest(NetworkHandleType Channel);

/**
 * \brief   CanNm の周期処理。タイマ満了判定と NM フレームの（再）送信を行う。
 *
 * \ServiceID      {0x13}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanNm_MainFunction(void);

/**
 * \brief   診断 CommunicationControl (UDS SID 0x28) からの NM PDU 送信無効化要求を反映する
 *          （[SWS_CanNm_00215] 相当）。
 *
 * \details 無効化中は Repeat Message/Normal Operation State でも NM フレームを
 *          送信しない（[SWS_CanNm_00100] の passive mode 相当の抑制。状態機械
 *          自体は通常どおり遷移する）。実仕様（[SWS_CanNm_00172]）は現在
 *          Network Mode でない場合に E_NOT_OK を要求するが、本プロジェクトは
 *          そのゲートを実装しない（Bus-Sleep 中に呼ばれても抑制フラグ自体は
 *          そのまま更新して良く、次回 Network Mode 復帰時に正しく反映される
 *          ため。CanNm_NetworkRequest/Release と同じ「現在の状態に関わらず常に
 *          受理する」簡略方針）。
 *
 * \param[in]  Channel  NM チャネルハンドル（CANNM_MAIN_NETWORK_HANDLE 以外は拒否）。
 *
 * \retval  E_OK      要求を受理した。
 * \retval  E_NOT_OK  未初期化、または Channel が不正。
 *
 * \AUTOSARReq     {SWS_CanNm_00215, SWS_CanNm_00192}
 * \ServiceID      {0x0C}
 * \Reentrancy     {Reentrant (but not for the same NM-channel)}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType CanNm_DisableCommunication(NetworkHandleType Channel);

/**
 * \brief   診断 CommunicationControl (UDS SID 0x28) からの NM PDU 送信再有効化要求を反映する
 *          （[SWS_CanNm_00216] 相当）。
 *
 * \details `CanNm_DisableCommunication()` で立てた抑制を解除する。ゲート省略の
 *          方針は同関数のコメントを参照。
 *
 * \param[in]  Channel  NM チャネルハンドル（CANNM_MAIN_NETWORK_HANDLE 以外は拒否）。
 *
 * \retval  E_OK      要求を受理した。
 * \retval  E_NOT_OK  未初期化、または Channel が不正。
 *
 * \AUTOSARReq     {SWS_CanNm_00216, SWS_CanNm_00192}
 * \ServiceID      {0x0D}
 * \Reentrancy     {Reentrant (but not for the same NM-channel)}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType CanNm_EnableCommunication(NetworkHandleType Channel);

/**
 * \brief   自ノードに設定されたノード識別子を取得する（[SWS_CanNm_00220]）。
 *
 * \details 送信する NM フレームに乗せる自ノードの ID（`CANNM_SOURCE_NODE_ID`）を
 *          そのまま返す。受信した NM フレームの送信元 ID を返す
 *          `CanNm_GetNodeIdentifier()` とは区別されるので注意。
 *
 * \param[in]   Channel      NM チャネルハンドル（CANNM_MAIN_NETWORK_HANDLE 以外は拒否）。
 * \param[out]  nmNodeIdPtr  自ノードの ID の格納先。NULL 禁止。
 *
 * \retval  E_OK      正常に取得した。
 * \retval  E_NOT_OK  未初期化、Channel が不正、または nmNodeIdPtr が NULL。
 *
 * \AUTOSARReq     {SWS_CanNm_00220, SWS_CanNm_00192}
 * \ServiceID      {0x07}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType CanNm_GetLocalNodeIdentifier(NetworkHandleType Channel, uint8* nmNodeIdPtr);

/**
 * \brief   直近に受信した NM フレームの送信元ノード識別子を取得する（[SWS_CanNm_00219]）。
 *
 * \details `CanNm_RxIndication()` が受信の都度更新するキャッシュ値をそのまま
 *          返す。一度も NM フレームを受信していない場合は 0 を返す
 *          （実仕様は「未設定/取得失敗時は E_NOT_OK」も許容するが、本
 *          プロジェクトは他の getter 系 API と同じく既定値を返すだけの
 *          簡略実装とする）。
 *
 * \param[in]   Channel      NM チャネルハンドル（CANNM_MAIN_NETWORK_HANDLE 以外は拒否）。
 * \param[out]  nmNodeIdPtr  直近受信 NM フレームの送信元 ID の格納先。NULL 禁止。
 *
 * \retval  E_OK      正常に取得した。
 * \retval  E_NOT_OK  未初期化、Channel が不正、または nmNodeIdPtr が NULL。
 *
 * \AUTOSARReq     {SWS_CanNm_00219, SWS_CanNm_00192}
 * \ServiceID      {0x06}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType CanNm_GetNodeIdentifier(NetworkHandleType Channel, uint8* nmNodeIdPtr);

/**
 * \brief   現在の CanNm 状態とモードを取得する（[SWS_CanNm_00091] 相当）。
 *
 * \param[in]   Channel   NM チャネルハンドル（CANNM_MAIN_NETWORK_HANDLE 以外は拒否）。
 * \param[out]  StatePtr  現在の内部状態の格納先。NULL 可（不要なら渡さなくてよい）。
 * \param[out]  ModePtr   現在の操作モードの格納先。NULL 可。
 *
 * \retval  E_OK      取得した。
 * \retval  E_NOT_OK  未初期化、または Channel が不正。
 *
 * \AUTOSARReq     {SWS_CanNm_00223, SWS_CanNm_00192}
 * \ServiceID      {0x0B}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType CanNm_GetState(NetworkHandleType Channel, CanNm_StateType* StatePtr, CanNm_ModeType* ModePtr);

/**
 * \brief   CanNm モジュールのバージョン情報を取得する。
 *
 * \details CanNm_Init と並び、未初期化時でも CANNM_E_UNINIT を報告しない例外 API
 *          （他 BSW モジュールと共通の慣例）のため、初期化状態は確認せず
 *          NULL ポインタチェックのみ行う。
 *
 * \param[out]  versioninfo  バージョン情報の格納先。NULL 禁止。
 *
 * \ServiceID      {0xF1}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanNm_GetVersionInfo(Std_VersionInfoType* versioninfo);

/* ======================================================================
 * Callback Functions and Notifications
 * ====================================================================== */

/**
 * \brief   NM フレームの受信を通知する（CanIf から呼ばれる）。
 *
 * \details Network Mode 中は NM-Timeout Timer を再起動する
 *          （[SWS_CanNm_00098]）。Prepare Bus-Sleep Mode 中は Network Mode
 *          （Repeat Message State）へ自動遷移する（[SWS_CanNm_00124]）。
 *          Bus-Sleep Mode 中は CanNm 自身は状態遷移せず、CANNM_E_NET_START_IND
 *          の DET 報告に加え `Nm_NetworkStartIndication()`（[SWS_Nm_00154]）
 *          で上位層へ通知する（[SWS_CanNm_00127]/[SWS_CanNm_00336]。実際に
 *          ネットワークへ復帰させ CanNm 自身を起こす処理は Nm 経由で ComM
 *          側が行う、Nm_NetworkStartIndication() の Doxygen 参照）。
 *
 * \param[in]  RxPduId     受信 PDU ID（本プロジェクトでは単一チャネルのため未使用）。
 * \param[in]  PduInfoPtr  受信データ。NULL 禁止。
 *
 * \ServiceID      {0x42}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanNm_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr);

/**
 * \brief   NM フレームの送信完了を通知する（CanIf から呼ばれる）。
 *
 * \details 送信成功時、Network Mode 中は NM-Timeout Timer を再起動する
 *          （[SWS_CanNm_00099]）。
 *
 * \param[in]  TxPduId  送信完了した PDU ID（本プロジェクトでは単一チャネルのため未使用）。
 * \param[in]  result   E_OK=送信成功。
 *
 * \ServiceID      {0x40}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CanNm_TxConfirmation(PduIdType TxPduId, Std_ReturnType result);

/**
 * \brief   NM フレームの送信データを、下位層（CanIf）へ複製する（トリガ送信。[SWS_CanNm_00351]）。
 *
 * \details CanIf が、送信の直前に、CanNm へ NM フレームの内容を取りに来る。CanNm が組み立てる NM フレーム
 *          （CBV と Source Node ID。CanNm_TransmitPdu() が CanIf_Transmit() へ渡すものと同じ内容）を、
 *          呼び出し元のバッファへ複製し、SduLength を複製した長さへ更新する。バッファが NM フレームの長さ
 *          （CANNM_DLC）より小さいときは、何も複製せず E_NOT_OK を返し、PduInfoPtr を変えない。
 *
 *          簡略化: CanNmComUserDataSupport（NM ユーザデータを Com から PduR_CanNmTriggerTransmit() で集める機能）は
 *          持たないので、NM フレームは、CBV と Source Node ID だけで構成する。本プロジェクトの本番の設定は、
 *          CanNm の TX PDU をトリガ送信に対応させない（CanIf.json の triggerTransmit が無い）ので、本番コードからは呼ばれない。
 *
 * \param[in]     TxPduId     データを要求された CanNm の TX PDU の ID（CanNmConf_CanNmTxPdu_CanNm_Tx）。
 * \param[in,out] PduInfoPtr  SduDataPtr=複製先のバッファ、SduLength=その大きさ。戻るとき、複製した長さ。
 *                            NULL 禁止。SduDataPtr も NULL 禁止。
 *
 * \retval  E_OK      NM フレームを複製した。
 * \retval  E_NOT_OK  未初期化、NULL ポインタ、TxPduId が不正、またはバッファが小さい。何も複製していない。
 *
 * \AUTOSARReq     {SWS_CanNm_91001, SWS_CanNm_00351, SWS_CanNm_00195}
 * \ServiceID      {0x41}
 * \Reentrancy     {Reentrant for different PduIds. Non reentrant for the same PduId.}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType CanNm_TriggerTransmit(PduIdType TxPduId, PduInfoType* PduInfoPtr);

/* ======================================================================
 * Test Functions
 * ====================================================================== */

#ifdef CANNM_UNIT_TEST
/**
 * \brief   CanNm の状態を指定の値へ直接設定する（単体テスト専用）。
 *
 * \details 状態遷移の API（CanNm_NetworkRequest()・CanNm_MainFunction() など）を
 *          経由せずに、指定の状態から判定の分岐を調べるために使う（遷移そのものの
 *          検証はコールチェーンのテストの役割）。状態以外（タイマー・各種フラグ・
 *          上位層への通知）は変えない。テスト側で、判定に必要な他の値を別途そろえること
 *          （標準外の関数。EcuM_Test_SetState() と同じ設計方針）。
 *
 * \param[in]  state  設定する CanNm の状態。
 */
void CanNm_Test_SetState(CanNm_StateType state);
#endif

#ifdef __cplusplus
}
#endif

#endif /* CANNM_H */
