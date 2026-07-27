/**
 * \file    Crypto.h
 * \brief   Crypto Driver 公開インタフェース (AUTOSAR SWS_CryptoDriver 準拠)
 * \details Csm/CryIf/Crypto レイヤの最下層。実際の暗号計算（AES-128-CMAC、
 *          Crypto_Aes128.c/Crypto_Cmac.c への自前実装）と鍵テーブル
 *          （Crypto_PBCfg.c）を保持する。CryIf からのみ呼ばれ、SecOC は
 *          本モジュールの存在を一切知らない（Csm_MacGenerate/Csm_MacVerify
 *          のみを呼ぶ）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef CRYPTO_H
#define CRYPTO_H

#include "Std_Types.h"
#include "Crypto_Types.h"
#include "Crypto_Cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   Crypto Driver モジュールを初期化する。
 *
 * \details AES-128 の自己診断（Crypto_Aes128_SelfTest()、FIPS-197 既知
 *          テストベクタ）を実行し、結果をログ出力する
 *          （元 SecOC_Init() が行っていたセルフテストを移設）。
 *
 * \AUTOSARReq     {SWS_Crypto_00045}
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Crypto_Init(void);

/**
 * \brief   Crypto Driver モジュールのバージョン情報を取得する。
 *
 * \details [SWS_Crypto_00047] のみが規定されており、未初期化チェックは
 *          要求されていない（NULL ポインタチェックのみ）。
 *
 * \param[out]  versioninfo  バージョン情報の格納先。NULL 禁止。
 *
 * \ServiceID      {0x01}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Crypto_GetVersionInfo(Std_VersionInfoType* versioninfo);

/**
 * \brief   ジョブに設定されたプリミティブ（MAC 生成/検証）を実行する。
 *
 * \details `job->service` が `CRYPTO_MACGENERATE` なら `job->cryptoKeyId` の
 *          鍵で `job->inputPtr` の AES-128-CMAC を計算し、上位 `job->macLength`
 *          バイトを `job->macPtr` へ書き込む。`CRYPTO_MACVERIFY` なら同様に
 *          再計算した上で、`job->macPtr` の内容（受信側 MAC）と定数時間で
 *          比較し、結果を `*job->verifyResultPtr` へ書き込む（不一致は
 *          E_NOT_OK ではなく `CRYPTO_E_VER_NOT_OK` で表現する。ジョブ自体の
 *          実行が失敗したかどうかとは別軸）。
 *
 *          本プロジェクトは同期処理のみのため、実 AUTOSAR が定義する
 *          CRYPTO_E_BUSY/CRYPTO_E_QUEUE_FULL/CRYPTO_E_KEY_NOT_VALID 等の
 *          非同期・複数ジョブキュー由来の戻り値は扱わない（E_OK/E_NOT_OK
 *          のみ）。
 *
 * \param[in]     objectId  Crypto Driver Object の ID。本プロジェクトは
 *                          単一オブジェクトのみのため CRYPTO_OBJECT_ID
 *                          以外は不正値として扱う。
 * \param[in,out] job       ジョブ記述（Crypto_JobType）。NULL 禁止。
 *
 * \retval  E_OK      要求されたプリミティブを実行した
 *                    （MACVERIFY の場合、検証結果自体は verifyResultPtr 参照）。
 * \retval  E_NOT_OK  未初期化、objectId/cryptoKeyId が範囲外、job が NULL、
 *                    またはサポートしないサービス種別。
 *
 * \AUTOSARReq     {SWS_Crypto_91003}
 * \ServiceID      {0x03}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Crypto_ProcessJob(uint32 objectId, Crypto_JobType* job);

/**
 * \brief   鍵要素（AES-128 鍵本体）を書き換える。
 *
 * \details 書き換え直後は当該 `cryptoKeyId` の鍵を無効化し、`Crypto_KeySetValid()`
 *          が呼ばれるまで `Crypto_ProcessJob()` での使用を拒否する
 *          （[SWS_KeyM_00016]/[SWS_KeyM_00008] の「鍵更新セッションが Finalize
 *          されるまで新しい鍵は使えない」という設計をそのまま反映）。
 *          `keyElementId` は本プロジェクトが唯一持つ鍵要素
 *          `CRYPTO_KEY_ELEMENT_ID_CIPHER_KEY`(=1) 以外を拒否する。
 *
 * \param[in]  cryptoKeyId    書き換える鍵の ID（CRYPTO_KEY_* 定数）。
 * \param[in]  keyElementId   CRYPTO_KEY_ELEMENT_ID_CIPHER_KEY 固定。
 * \param[in]  keyPtr         新しい鍵バイト列。NULL 禁止。
 * \param[in]  keyLength      keyPtr のバイト長。CRYPTO_AES128_KEY_SIZE(16) 以外は拒否。
 *
 * \retval  E_OK      鍵を書き換えた（無効状態になる）。
 * \retval  E_NOT_OK  未初期化、cryptoKeyId/keyElementId が不正、NULL、または長さ不一致。
 *
 * \AUTOSARReq     {SWS_Crypto_91004}
 * \ServiceID      {0x04}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Crypto_KeyElementSet(uint32 cryptoKeyId, uint32 keyElementId,
                                     const uint8* keyPtr, uint32 keyLength);

/**
 * \brief   鍵を有効状態にする。
 *
 * \details `Crypto_KeyElementSet()` で書き換えた鍵を実際に使用可能にする。
 *          KeyM の鍵更新セッション終了（Finalize）から呼ばれる想定。
 *
 * \param[in]  cryptoKeyId  有効化する鍵の ID（CRYPTO_KEY_* 定数）。
 *
 * \retval  E_OK      有効化した。
 * \retval  E_NOT_OK  未初期化、または cryptoKeyId が範囲外。
 *
 * \AUTOSARReq     {SWS_Crypto_91014}
 * \ServiceID      {0x05}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Crypto_KeySetValid(uint32 cryptoKeyId);

#ifdef __cplusplus
}
#endif

#endif /* CRYPTO_H */
