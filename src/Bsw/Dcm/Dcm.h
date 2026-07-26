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
 * \brief   DCM モジュールを初期化する。
 *
 * \details セッション状態を Default Session にリセットする
 *          (AUTOSAR SWS_Dcm_00769)。
 *          EcuM_Init() から Com_Init() の後に呼び出すこと。
 *
 * \pre        PduR_Init() が正常に完了していること。
 *
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Dcm_Init(void);

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
