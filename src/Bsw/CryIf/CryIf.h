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
 * \brief   CryIf モジュールが初期化済みかを返す。
 *
 * \details AUTOSAR 標準の SWS_CryptoInterface には存在しない本プロジェクト
 *          独自の拡張関数（2026-09 追加）。上位層 Csm が [SWS_Csm_91010]
 *          （「CSM API が未初期化の CryIf を呼ぶ場合、操作を実行せず
 *          CSM_E_SERVICE_NOT_STARTED を DET 報告しなければならない」）を
 *          満たすために、CryIf_ProcessJob() 等を実際に呼ぶ前に状態を
 *          問い合わせる目的で新設した（Csm.c 参照）。本関数自体は前提条件を
 *          持たないため DET 報告は行わない（常に成功する単純な状態参照）。
 *
 * \return  TRUE: 初期化済み。FALSE: 未初期化。
 *
 * \ServiceID      {0x02}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
boolean CryIf_IsInitialized(void);

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

/**
 * \brief   鍵要素を対応する Crypto Driver Object へディスパッチする。
 *
 * \details 単一 Crypto Driver Object のみのため `Crypto_KeyElementSet()` への
 *          実質パススルー（[SWS_CryIf_00055]）。ただし cryIfKeyId の範囲
 *          チェックは下位層に委ねず CryIf 自身が行う（[SWS_CryIf_00050]。
 *          2026-09 追加。CryIf は独自の鍵 ID 空間を持たず Crypto と共有
 *          しているため CRYPTO_KEY_COUNT を直接参照する）。
 *
 * \param[in]  cryIfKeyId    鍵 ID。Crypto Driver 側の cryptoKeyId へそのまま渡す。
 * \param[in]  keyElementId  鍵要素 ID。
 * \param[in]  keyPtr        新しい鍵バイト列。NULL 禁止。
 * \param[in]  keyLength     keyPtr のバイト長。0 禁止。
 *
 * \retval  E_OK      鍵要素を書き換えた。
 * \retval  E_NOT_OK  未初期化、NULL、keyLength=0、cryIfKeyId が範囲外
 *                    （[SWS_CryIf_00050]）、または下位層が失敗。
 *
 * \AUTOSARReq     {SWS_CryIf_91004, SWS_CryIf_00050}
 * \ServiceID      {0x04}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType CryIf_KeyElementSet(uint32 cryIfKeyId, uint32 keyElementId,
                                    const uint8* keyPtr, uint32 keyLength);

/**
 * \brief   鍵を対応する Crypto Driver Object 上で有効化する。
 *
 * \details 単一 Crypto Driver Object のみのため `Crypto_KeySetValid()` への
 *          実質パススルー（[SWS_CryIf_00058]）。ただし cryIfKeyId の範囲
 *          チェックは下位層に委ねず CryIf 自身が行う（[SWS_CryIf_00057]。
 *          2026-09 追加）。
 *
 * \param[in]  cryIfKeyId  有効化する鍵の ID。
 *
 * \retval  E_OK      有効化した。
 * \retval  E_NOT_OK  未初期化、cryIfKeyId が範囲外（[SWS_CryIf_00057]）、
 *                    または下位層が失敗。
 *
 * \AUTOSARReq     {SWS_CryIf_91005, SWS_CryIf_00057}
 * \ServiceID      {0x05}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType CryIf_KeySetValid(uint32 cryIfKeyId);

/**
 * \brief   鍵要素を対応する Crypto Driver Object から読み出す。
 *
 * \details 単一 Crypto Driver Object のみのため `Crypto_KeyElementGet()` への
 *          実質パススルー（[SWS_CryIf_00065]）。ただし cryIfKeyId の範囲
 *          チェックは下位層に委ねず CryIf 自身が行う（[SWS_CryIf_00060]。
 *          2026-09 追加）。
 *
 * \param[in]     cryIfKeyId       鍵 ID。Crypto Driver 側の cryptoKeyId へそのまま渡す。
 * \param[in]     keyElementId     鍵要素 ID。
 * \param[out]    resultPtr        読み出した鍵バイト列の格納先。NULL 禁止。
 * \param[in,out] resultLengthPtr  in: resultPtr のバッファ長。out: 実際に
 *                                 書き込んだバイト数。NULL 禁止、値0も禁止。
 *
 * \retval  E_OK      鍵要素を読み出した。
 * \retval  E_NOT_OK  未初期化、NULL、`*resultLengthPtr`=0、cryIfKeyId が範囲外
 *                    （[SWS_CryIf_00060]）、または下位層が失敗。
 *
 * \AUTOSARReq     {SWS_CryIf_91006, SWS_CryIf_00060}
 * \ServiceID      {0x06}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType CryIf_KeyElementGet(uint32 cryIfKeyId, uint32 keyElementId,
                                    uint8* resultPtr, uint32* resultLengthPtr);

#ifdef __cplusplus
}
#endif

#endif /* CRYIF_H */
