/**
 * \file    Nm.h
 * \brief   ネットワークマネジメントインタフェース 公開インタフェース
 *          (AUTOSAR SWS_NetworkManagementInterface 準拠)
 * \details ComM と、バス固有の Network Management モジュール（本プロジェクトの
 *          場合 CanNm）との間に位置する、汎用・バス非依存の調整層。
 *
 *          AUTOSAR 通信スタックにおける Nm の位置
 *          （`docs/autosar/4.3.1/AUTOSAR_SWS_CANNetworkManagement.pdf`
 *          Figure 4-1 参照）:
 *            ComM
 *             ↓ Nm_NetworkRequest()/Nm_NetworkRelease()
 *            Nm    ← 本モジュール
 *             ↓ CanNm_NetworkRequest()/CanNm_NetworkRelease()
 *            CanNm
 *             ↓
 *            CanIf
 *
 *          本プロジェクトは CAN 1 チャネルのみの単一 ECU 構成のため、実
 *          AUTOSAR の Nm が持つ「複数ネットワークにまたがる協調シャットダウン」
 *          （NM Coordinator 機能、7.2章）は対応除外とし、ComM と CanNm の間を
 *          ほぼそのまま中継する薄い層として実装する（対応除外の詳細は
 *          Nm_Cfg.h 冒頭コメント参照）。
 *
 *          型のマッピング: `Nm_StateType`/`Nm_ModeType`（本ヘッダ）は
 *          `CanNm_StateType`/`CanNm_ModeType`（CanNm.h）とは別の enum で、
 *          仕様上も数値が異なる（例: Nm_StateType は NM_STATE_UNINIT=0x00
 *          から始まる8値、CanNm_StateType は CANNM_STATE_BUS_SLEEP=0から
 *          始まる5値）。`Nm_GetState()` が変換する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef NM_H
#define NM_H

#include "Platform_Types.h"
#include "Std_Types.h"
#include "ComStack_Types.h"
#include "Nm_Cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   Nm の操作モード（[SWS_Nm_00274]、4値）。
 *
 * \details CanNm_ModeType（3値、Repeat Message/Normal Operation の区別は
 *          内部状態 Nm_StateType 側にのみ存在）とは異なり、Nm_ModeType には
 *          NM Coordinator 機能専用の NM_MODE_SYNCHRONIZE が追加で定義される
 *          （[SWS_Nm_00274]）。本プロジェクトは NM Coordinator 機能を実装
 *          しないため、この値が実際に使われることはない。
 */
typedef enum
{
    NM_MODE_BUS_SLEEP = 0,         /**< Bus-Sleep Mode */
    NM_MODE_PREPARE_BUS_SLEEP,     /**< Prepare-Bus Sleep Mode */
    NM_MODE_SYNCHRONIZE,           /**< Synchronize Mode（NM Coordinator専用、本プロジェクト未使用） */
    NM_MODE_NETWORK                /**< Network Mode */
} Nm_ModeType;

/**
 * \brief   Nm の内部状態（[SWS_Nm_00275]、8値）。
 *
 * \details CanNm_StateType（5値、CANNM_STATE_BUS_SLEEP=0起点）とは数値も
 *          個数も異なる別 enum（[SWS_Nm_00275] は NM_STATE_UNINIT=0x00 起点
 *          の8値）。NM_STATE_UNINIT/SYNCHRONIZE/OFFLINE は NM Coordinator
 *          機能または未初期化状態専用のため、本プロジェクトの
 *          `Nm_GetState()` が返すことはない。
 */
typedef enum
{
    NM_STATE_UNINIT            = 0x00,  /**< Uninitialized State（本プロジェクト未使用） */
    NM_STATE_BUS_SLEEP         = 0x01,  /**< Bus-Sleep State */
    NM_STATE_PREPARE_BUS_SLEEP = 0x02,  /**< Prepare-Bus State */
    NM_STATE_READY_SLEEP       = 0x03,  /**< Ready Sleep State */
    NM_STATE_NORMAL_OPERATION  = 0x04,  /**< Normal Operation State */
    NM_STATE_REPEAT_MESSAGE    = 0x05,  /**< Repeat Message State */
    NM_STATE_SYNCHRONIZE       = 0x06,  /**< Synchronize State（NM Coordinator専用、本プロジェクト未使用） */
    NM_STATE_OFFLINE           = 0x07   /**< Offline State（本プロジェクト未使用） */
} Nm_StateType;

/**
 * \brief   Nm_Init() の設定引数型（不透明型）。
 *
 * \details [SWS_Nm_00283] は常に NULL_PTR を渡すことを要求する（post-build
 *          設定を使う場合でも本プロジェクトは単一 ECU 構成で post-build
 *          バリアント切替を持たないため、`CanNm_ConfigType` と同じ簡略化
 *          パターンで中身を定義しない不透明型とする）。
 */
typedef struct Nm_ConfigType_Tag Nm_ConfigType;

/**
 * \brief   Nm モジュールを初期化する。
 *
 * \details [SWS_Nm_00127]: CanIf_Init() の後に呼び出すこと。実装の説明
 *          （8.3.1.1節）は「<BusNm>_Init を呼ぶ」とは規定していない
 *          （NetworkRequest 等とは異なり Description 欄に委譲の記載が無い）
 *          ため、本実装も CanNm_Init() を呼ばず、Nm 自身の初期化状態のみを
 *          管理する。CanNm_Init() は EcuM_Init() から別途・独立に呼び出す
 *          （EcuM.c 参照）。
 *
 * \param[in]  ConfigPtr  常に NULL を渡すこと（[SWS_Nm_00283]）。
 *
 * \AUTOSARReq     {SWS_Nm_00030, SWS_Nm_00127, SWS_Nm_00283}
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Nm_Init(const Nm_ConfigType* ConfigPtr);

/**
 * \brief   通信が必要であることを Nm へ伝える（[SWS_Nm_00032]）。
 *
 * \details `CanNm_NetworkRequest()` を呼ぶ（8.3.1.3節 Description: "This
 *          function calls the <BusNm>_NetworkRequest"）。
 *
 * \param[in]  Channel  NM チャネルハンドル（NM_MAIN_NETWORK_HANDLE 以外は拒否）。
 *
 * \retval  E_OK      要求を受理した。
 * \retval  E_NOT_OK  未初期化、Channel が不正、または CanNm 側が失敗した。
 *
 * \AUTOSARReq     {SWS_Nm_00032, SWS_Nm_00129}
 * \ServiceID      {0x02}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Nm_NetworkRequest(NetworkHandleType Channel);

/**
 * \brief   通信が不要になったことを Nm へ伝える（[SWS_Nm_00046]）。
 *
 * \details `CanNm_NetworkRelease()` を呼ぶ（8.3.1.4節 Description 参照）。
 *
 * \param[in]  Channel  NM チャネルハンドル（NM_MAIN_NETWORK_HANDLE 以外は拒否）。
 *
 * \retval  E_OK      要求を受理した。
 * \retval  E_NOT_OK  未初期化、Channel が不正、または CanNm 側が失敗した。
 *
 * \AUTOSARReq     {SWS_Nm_00046, SWS_Nm_00131}
 * \ServiceID      {0x03}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Nm_NetworkRelease(NetworkHandleType Channel);

/**
 * \brief   診断 CommunicationControl からの NM PDU 送信無効化要求を反映する
 *          （[SWS_Nm_00033]）。
 *
 * \details `CanNm_DisableCommunication()` を呼ぶ（8.3.2.1節 Description
 *          参照）。DCM（本プロジェクトでは BswM 経由）が診断セッション中に
 *          NM フレーム送信を止めたい場合に使う。
 *
 * \param[in]  Channel  NM チャネルハンドル（NM_MAIN_NETWORK_HANDLE 以外は拒否）。
 *
 * \retval  E_OK      要求を受理した。
 * \retval  E_NOT_OK  未初期化、Channel が不正、または CanNm 側が失敗した。
 *
 * \AUTOSARReq     {SWS_Nm_00033, SWS_Nm_00133}
 * \ServiceID      {0x04}
 * \Reentrancy     {Reentrant (but not for the same NM-channel)}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Nm_DisableCommunication(NetworkHandleType Channel);

/**
 * \brief   診断 CommunicationControl からの NM PDU 送信再有効化要求を反映する
 *          （[SWS_Nm_00034]）。
 *
 * \details `CanNm_EnableCommunication()` を呼ぶ（8.3.2.2節 Description
 *          参照）。
 *
 * \param[in]  Channel  NM チャネルハンドル（NM_MAIN_NETWORK_HANDLE 以外は拒否）。
 *
 * \retval  E_OK      要求を受理した。
 * \retval  E_NOT_OK  未初期化、Channel が不正、または CanNm 側が失敗した。
 *
 * \AUTOSARReq     {SWS_Nm_00034, SWS_Nm_00135}
 * \ServiceID      {0x05}
 * \Reentrancy     {Reentrant (but not for the same NM-channel)}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Nm_EnableCommunication(NetworkHandleType Channel);

/**
 * \brief   Repeat Message State への遷移を要求する（[SWS_Nm_00038]）。
 *
 * \details `CanNm_RepeatMessageRequest()` を呼ぶ（8.3.3.4節 Description
 *          参照）。本プロジェクトでは診断・デバッグ用途を想定するのみで、
 *          通常の運用フローからは呼ばない（CanNm.h の同名コメント参照）。
 *
 * \param[in]  Channel  NM チャネルハンドル（NM_MAIN_NETWORK_HANDLE 以外は拒否）。
 *
 * \retval  E_OK      Repeat Message State へ遷移した。
 * \retval  E_NOT_OK  未初期化、Channel が不正、または CanNm 側が失敗した。
 *
 * \AUTOSARReq     {SWS_Nm_00038, SWS_Nm_00143}
 * \ServiceID      {0x09}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Nm_RepeatMessageRequest(NetworkHandleType Channel);

/**
 * \brief   直近に受信した NM フレームの送信元ノード識別子を取得する
 *          （[SWS_Nm_00039]）。
 *
 * \details `CanNm_GetNodeIdentifier()` を呼ぶ（8.3.3.5節 Description
 *          参照）。
 *
 * \param[in]   Channel      NM チャネルハンドル（NM_MAIN_NETWORK_HANDLE 以外は拒否）。
 * \param[out]  nmNodeIdPtr  直近受信 NM フレームの送信元 ID の格納先。NULL 禁止。
 *
 * \retval  E_OK      正常に取得した。
 * \retval  E_NOT_OK  未初期化、Channel が不正、nmNodeIdPtr が NULL、
 *                    または CanNm 側が失敗した。
 *
 * \AUTOSARReq     {SWS_Nm_00039, SWS_Nm_00145}
 * \ServiceID      {0x0A}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Nm_GetNodeIdentifier(NetworkHandleType Channel, uint8* nmNodeIdPtr);

/**
 * \brief   自ノードに設定されたノード識別子を取得する（[SWS_Nm_00040]）。
 *
 * \details `CanNm_GetLocalNodeIdentifier()` を呼ぶ（8.3.3.6節 Description
 *          参照）。
 *
 * \param[in]   Channel      NM チャネルハンドル（NM_MAIN_NETWORK_HANDLE 以外は拒否）。
 * \param[out]  nmNodeIdPtr  自ノードの ID の格納先。NULL 禁止。
 *
 * \retval  E_OK      正常に取得した。
 * \retval  E_NOT_OK  未初期化、Channel が不正、nmNodeIdPtr が NULL、
 *                    または CanNm 側が失敗した。
 *
 * \AUTOSARReq     {SWS_Nm_00040, SWS_Nm_00147}
 * \ServiceID      {0x0B}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Nm_GetLocalNodeIdentifier(NetworkHandleType Channel, uint8* nmNodeIdPtr);

/**
 * \brief   現在の Nm 状態とモードを取得する（[SWS_Nm_00043]）。
 *
 * \details `CanNm_GetState()` を呼び（8.3.3.8節 Description 参照）、結果の
 *          `CanNm_StateType`/`CanNm_ModeType` を `Nm_StateType`/`Nm_ModeType`
 *          へ写像する（Nm.h 冒頭コメント参照）。
 *
 * \param[in]   Channel      NM チャネルハンドル（NM_MAIN_NETWORK_HANDLE 以外は拒否）。
 * \param[out]  nmStatePtr   現在の内部状態の格納先。NULL 可（不要なら渡さなくてよい）。
 * \param[out]  nmModePtr    現在の操作モードの格納先。NULL 可。
 *
 * \retval  E_OK      取得した。
 * \retval  E_NOT_OK  未初期化、Channel が不正、または CanNm 側が失敗した。
 *
 * \AUTOSARReq     {SWS_Nm_00043, SWS_Nm_00151}
 * \ServiceID      {0x0E}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Nm_GetState(NetworkHandleType Channel, Nm_StateType* nmStatePtr, Nm_ModeType* nmModePtr);

/**
 * \brief   Nm モジュールのバージョン情報を取得する（[SWS_Nm_00044]）。
 *
 * \details Nm_Init と並び、未初期化時でも NM_E_UNINIT を報告しない例外 API
 *          （他 BSW モジュールと共通の慣例）のため、初期化状態は確認せず
 *          NULL ポインタチェックのみ行う。
 *
 * \param[out]  nmVerInfoPtr  バージョン情報の格納先。NULL 禁止。
 *
 * \AUTOSARReq     {SWS_Nm_00044}
 * \ServiceID      {0x0F}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Nm_GetVersionInfo(Std_VersionInfoType* nmVerInfoPtr);

/**
 * \brief   Bus-Sleep Mode 中に NM フレーム受信を通知するコールバック
 *          （CanNm から呼ばれる、[SWS_Nm_00154]）。
 *
 * \details [SWS_Nm_00155]: 上位層（ComM）へ `ComM_Nm_NetworkStartIndication()`
 *          を呼んで転送する。
 *
 * \param[in]  Channel  通知元の NM チャネルハンドル。
 *
 * \AUTOSARReq     {SWS_Nm_00154, SWS_Nm_00155}
 * \ServiceID      {0x11}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Asynchronous}
 */
void Nm_NetworkStartIndication(NetworkHandleType Channel);

/**
 * \brief   Network Mode 突入を通知するコールバック（CanNm から呼ばれる、
 *          [SWS_Nm_00156]）。
 *
 * \details [SWS_Nm_00158]: 上位層（ComM）へ `ComM_Nm_NetworkMode()` を
 *          呼んで転送する。
 *
 * \param[in]  Channel  通知元の NM チャネルハンドル。
 *
 * \AUTOSARReq     {SWS_Nm_00156, SWS_Nm_00158}
 * \ServiceID      {0x12}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Asynchronous}
 */
void Nm_NetworkMode(NetworkHandleType Channel);

/**
 * \brief   Prepare Bus-Sleep Mode 突入を通知するコールバック（CanNm から
 *          呼ばれる、[SWS_Nm_00159]）。
 *
 * \details [SWS_Nm_00161]: 上位層（ComM）へ `ComM_Nm_PrepareBusSleepMode()`
 *          を呼んで転送する。
 *
 * \param[in]  Channel  通知元の NM チャネルハンドル。
 *
 * \AUTOSARReq     {SWS_Nm_00159, SWS_Nm_00161}
 * \ServiceID      {0x13}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Asynchronous}
 */
void Nm_PrepareBusSleepMode(NetworkHandleType Channel);

/**
 * \brief   Bus-Sleep Mode 突入を通知するコールバック（CanNm から呼ばれる、
 *          [SWS_Nm_00162]）。
 *
 * \details [SWS_Nm_00163]: 上位層（ComM）へ `ComM_Nm_BusSleepMode()` を
 *          呼んで転送する。
 *
 * \param[in]  Channel  通知元の NM チャネルハンドル。
 *
 * \AUTOSARReq     {SWS_Nm_00162, SWS_Nm_00163}
 * \ServiceID      {0x14}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Asynchronous}
 */
void Nm_BusSleepMode(NetworkHandleType Channel);

#ifdef __cplusplus
}
#endif

#endif /* NM_H */
