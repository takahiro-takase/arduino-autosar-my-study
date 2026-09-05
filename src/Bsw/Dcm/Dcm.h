/**
 * \file    Dcm.h
 * \brief   DCM 公開インタフェース (AUTOSAR SWS_DCM 準拠)
 * \details EcuM が呼び出す DCM_Init() を宣言する。
 *          PduR から呼び出されるコールバック群は Dcm_Cbk.h で宣言する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef DCM_H
#define DCM_H

#include "Dcm_Cbk.h"
#include "Dcm_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   Dcm_Init() の設定引数型（不透明型）。
 *
 * \details SWS_Dcm_00037 は post-build 設定データへのポインタを要求するが、
 *          本プロジェクトは単一 ECU 構成で post-build バリアント切替を持たない
 *          ため、中身を定義しない不透明型とし、ポインタとしてのみ扱う
 *          （`KeyM_ConfigType` と同じ簡略化パターン。KeyM.h 冒頭コメント参照）。
 */
typedef struct Dcm_ConfigType_Tag Dcm_ConfigType;

/** セッション制御タイプ型 [SWS_Dcm_00339]。値は ISO 14229-1
 *  diagnosticSessionType パラメータに準拠する（DCM_SESSION_* 定数参照）。 */
typedef uint8 Dcm_SesCtrlType;

/**
 * \brief   セキュリティレベル型 [SWS_Dcm_00338]。
 * \details 実仕様は `SecurityLevel = (SecurityAccessType + 1) / 2` という
 *          変換式を規定するが、本プロジェクトは SecurityAccess Level1 のみ
 *          対応する簡略実装のため、内部状態をそのまま 0(Locked)/1(Unlocked)
 *          の2値で表す（`Dcm_Cbk.c` の `Dcm_SecurityLevel` 参照）。
 */
typedef uint8 Dcm_SecLevelType;

/**
 * \brief   アクティブなプロトコル種別型 [SWS_Dcm_00979]。
 *
 * \details 実仕様は OBD/UDS × CAN/FlexRay/IP/LIN、ROE、周期送信等
 *          20通り以上を定義するが、本プロジェクトは UDS on CAN
 *          (ISO15765-3/ISO14229-1) の単一プロトコル構成のため、実際に
 *          使用する `DCM_UDS_ON_CAN` のみ定義する（値は PDF のベクタ座標
 *          解析で確認済み、[SWS_Dcm_00979] Table 8.92）。
 */
typedef uint8 Dcm_ProtocolType;

#define DCM_UDS_ON_CAN 0x03U

/**
 * \brief   DCM モジュールを初期化する。
 *
 * \details セッション状態を Default Session にリセットする
 *          (AUTOSAR SWS_Dcm_00034)。
 *          EcuM_Init() から Com_Init() の後に呼び出すこと。
 *
 * \param[in]  ConfigPtr  常に NULL を渡すこと（本プロジェクトは post-build 設定を
 *                        持たないため）。
 *
 * \pre        PduR_Init() が正常に完了していること。
 *
 * \AUTOSARReq     {SWS_Dcm_00037}
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Dcm_Init(const Dcm_ConfigType* ConfigPtr);

/**
 * \brief   DCM 周期処理。S3 タイマ (セッションタイムアウト) を監視する。
 *
 * \details defaultSession 以外のとき、最後に診断要求を受信してから
 *          DCM_S3_TIMEOUT_MS 以上経過していれば defaultSession へ復帰させる。
 *          Os タスクテーブルから周期的に呼び出すこと。
 *
 * \ServiceID      {0x25}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Dcm_MainFunction(void);

/**
 * \brief   VIN (Vehicle Identification Number) を取得する（[SWS_Dcm_00950]）。
 *
 * \details 実仕様は、ECU 統合者（車両側の実装）が本関数を提供し、Dcm が
 *          起動時に一度だけ呼び出してキャッシュする「Dcm が呼び出す側」の
 *          関数である（[SWS_Dcm_01174]）。本プロジェクトは車両情報の実体を
 *          外部に持たないため、Dcm.c 自身が固定値を返す簡略実装とし、
 *          `Dcm_Init()` が同じ「起動時に一度だけ取得しキャッシュする」
 *          呼び出しパターンを踏襲する（UDS SID 0x22 DID 0xF190 の応答は
 *          このキャッシュから返す。`Dcm_ReadDid()` 参照）。
 *
 * \param[out]  Data  VIN 格納先（`DCM_VIN_LENGTH` (17) バイト以上の
 *                    バッファであること）。NULL 禁止。
 *
 * \retval  E_OK      Data に有効な VIN を格納した。
 * \retval  E_NOT_OK  Data が NULL（本プロジェクトではこれ以外に失敗しない）。
 *
 * \AUTOSARReq     {SWS_Dcm_00950}
 * \ServiceID      {0x07}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dcm_GetVin(uint8* Data);

/**
 * \brief   現在アクティブなセッション制御タイプを取得する。
 *
 * \param[out]  SesCtrlType  取得値の格納先。NULL 禁止。
 *
 * \retval  E_OK      正常取得（実仕様上、値取得自体は常に成功する）。
 * \retval  E_NOT_OK  未初期化、または SesCtrlType が NULL。
 *
 * \AUTOSARReq     {SWS_Dcm_00339}
 * \ServiceID      {0x06}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dcm_GetSesCtrlType(Dcm_SesCtrlType* SesCtrlType);

/**
 * \brief   現在アクティブなセキュリティレベルを取得する。
 *
 * \param[out]  SecLevel  取得値の格納先。NULL 禁止。
 *
 * \retval  E_OK      正常取得（実仕様上、値取得自体は常に成功する）。
 * \retval  E_NOT_OK  未初期化、または SecLevel が NULL。
 *
 * \AUTOSARReq     {SWS_Dcm_00338}
 * \ServiceID      {0x0d}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dcm_GetSecurityLevel(Dcm_SecLevelType* SecLevel);

/**
 * \brief   現在アクティブなプロトコル・コネクション・テスター送信元アドレスを取得する。
 *
 * \details 実仕様は複数プロトコル(OBD/UDS×CAN/FlexRay/IP等)・複数コネクションを
 *          動的に追跡するが、本プロジェクトは UDS on CAN の単一プロトコル・
 *          単一コネクション・物理アドレッシング固定構成のため、`ActiveProtocolType`
 *          は常に `DCM_UDS_ON_CAN`、`ConnectionId`は常に`DCM_CONNECTION_ID`(0)、
 *          `TesterSourceAddress`は常に`DCM_TESTER_SOURCE_ADDRESS`(UDS診断要求の
 *          CAN ID)を返す固定値実装とする（Dcm_Cfg.h 参照）。
 *
 * \param[out]  ActiveProtocolType    アクティブなプロトコル種別の格納先。NULL 禁止。
 * \param[out]  ConnectionId          コネクション識別子の格納先。NULL 禁止。
 * \param[out]  TesterSourceAddress   テスターの送信元アドレスの格納先。NULL 禁止。
 *
 * \retval  E_OK      正常取得（実仕様上、値取得自体は常に成功する）。
 * \retval  E_NOT_OK  未初期化、またはいずれかの引数が NULL。
 *
 * \AUTOSARReq     {SWS_Dcm_00340}
 * \ServiceID      {0x0f}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dcm_GetActiveProtocol(Dcm_ProtocolType* ActiveProtocolType, uint16* ConnectionId, uint16* TesterSourceAddress);

/**
 * \brief   現在のセッションを defaultSession へ強制的に戻す。
 *
 * \details [SWS_Dcm_01062]: アプリケーションが任意のタイミングで
 *          extendedSession を終了させるための API（仕様の例: 車速が
 *          しきい値を超えた場合の自動終了）。DCM 内部の defaultSession 復帰
 *          処理列（SecurityAccess ロック・RoutineControl/TransferData 中断・
 *          CommunicationControl/DTC設定のリセット・ComM 通知）を一括で実行する。
 *
 * \retval  E_OK      正常完了（実仕様上、実行自体は常に成功する）。
 * \retval  E_NOT_OK  未初期化。
 *
 * \AUTOSARReq     {SWS_Dcm_00520}
 * \ServiceID      {0x2a}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Dcm_ResetToDefaultSession(void);

/**
 * \brief   DCM モジュールのバージョン情報を取得する。
 *
 * \details Dcm_Init と並び、未初期化時でも DCM_E_UNINIT を報告しない
 *          例外 API（他 BSW モジュールと共通の慣例）のため、初期化状態は
 *          確認せず NULL ポインタチェックのみ行う。
 *
 * \param[out]  versioninfo  バージョン情報の格納先。NULL 禁止。
 *
 * \ServiceID      {0x24}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Dcm_GetVersionInfo(Std_VersionInfoType* versioninfo);

#ifdef __cplusplus
}
#endif

#endif /* DCM_H */
