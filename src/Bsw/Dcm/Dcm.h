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
