/**
 * \file    CryIf.h
 * \brief   Crypto Interface 公開インタフェース (AUTOSAR SWS_CryptoInterface 準拠)
 * \details Csm（上位）と Crypto Driver（下位）の間に位置するルーティング層。
 *          実 AUTOSAR は複数の Crypto Driver Object へジョブを振り分けるが、
 *          本プロジェクトは Crypto Driver が 1 個のみのため実質パススルーで
 *          ある（CanIf → Can の関係と同様、Csm は CryIf 経由でのみ Crypto
 *          Driver を呼び、直接は呼ばない）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef CRYIF_H
#define CRYIF_H

#include "Std_Types.h"
#include "Crypto_Types.h"
#include "CryIf_Cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   Crypto Interface モジュールを初期化する。
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CryIf_Init(void);

/**
 * \brief   Crypto Interface モジュールのバージョン情報を取得する。
 *
 * \details [SWS_CryIf_00016] により未初期化時は CRYIF_E_UNINIT を報告する
 *          （Com_GetVersionInfo 等と同じ挙動で、GetVersionInfo が UNINIT
 *          例外になる他の多くのモジュールとは異なる点に注意）。
 *
 * \param[out]  versioninfo  バージョン情報の格納先。NULL 禁止。
 *
 * \ServiceID      {0x01}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void CryIf_GetVersionInfo(Std_VersionInfoType* versioninfo);

/**
 * \brief   ジョブを対応する Crypto Driver Object へディスパッチする。
 *
 * \details 本プロジェクトは単一 Crypto Driver Object のみのため、
 *          channelId の妥当性チェック後は Crypto_ProcessJob() へそのまま
 *          委譲し戻り値をそのまま返す（[SWS_CryIf_00044]）。
 *
 * \param[in]     channelId  Crypto チャネル ID。CRYIF_CHANNEL_ID 以外は不正値。
 * \param[in,out] job        ジョブ記述（Crypto_JobType）。NULL 禁止。
 *
 * \retval  E_OK      要求されたプリミティブを実行した。
 * \retval  E_NOT_OK  未初期化、channelId が範囲外、job が NULL、
 *                    または Crypto_ProcessJob() が失敗した。
 *
 * \AUTOSARReq     {SWS_CryIf_91003}
 * \ServiceID      {0x03}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType CryIf_ProcessJob(uint32 channelId, Crypto_JobType* job);

#ifdef __cplusplus
}
#endif

#endif /* CRYIF_H */
