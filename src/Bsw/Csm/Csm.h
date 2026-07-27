/**
 * \file    Csm.h
 * \brief   Crypto Service Manager 公開インタフェース (AUTOSAR SWS_CryptoServiceManager 準拠)
 * \details SecOC が唯一直接呼ぶ暗号スタックの入口。SecOC は `jobId` という
 *          抽象ハンドルしか知らず、実際にどの鍵・どのアルゴリズムを使うかは
 *          `Csm_PBCfg.c` の CsmJob 設定と、その下の CryIf/Crypto レイヤが
 *          決める（E2EXf が Com から E2E ロジックを切り離したのと同じ
 *          「呼び出し元は下位実装を一切知らない」設計）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef CSM_H
#define CSM_H

#include "Std_Types.h"
#include "Crypto_Types.h"
#include "Csm_Cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   Csm モジュールを初期化する。
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Csm_Init(void);

/**
 * \brief   Csm モジュールのバージョン情報を取得する。
 *
 * \details [SWS_Csm_91008] により、本関数は Csm_Init 以外の全 API と同様
 *          未初期化時に CSM_E_UNINIT を報告する（他の多くのモジュールで
 *          GetVersionInfo が UNINIT チェック対象外なのとは異なる。実測で
 *          確認済み）。
 *
 * \param[out]  versioninfo  バージョン情報の格納先。NULL 禁止。
 *
 * \ServiceID      {0x3B}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Csm_GetVersionInfo(Std_VersionInfoType* versioninfo);

/**
 * \brief   指定ジョブで MAC を生成する。
 *
 * \details `jobId` に紐づく鍵（Csm_PBCfg.c の Csm_JobConfigData で決まる）を
 *          使い、`dataPtr` の AES-128-CMAC を計算する。
 *          `*macLengthPtr` には「切り詰め後に欲しいバイト数」を渡すこと
 *          （呼び出し前後で値は変化しない。実 AUTOSAR は成功時に「実際に
 *          書き込んだ長さ」で上書きするが、本プロジェクトは常に要求どおりの
 *          長さを生成できるため実質的に不変）。
 *
 * \param[in]     jobId        CSM_JOB_ID_* 定数。CRYPTO_MACGENERATE 用に
 *                              設定されたジョブでなければならない。
 * \param[in]     mode          本プロジェクトは常に CRYPTO_OPERATIONMODE_SINGLECALL。
 * \param[in]     dataPtr       MAC 計算対象データ。NULL 禁止。
 * \param[in]     dataLength    dataPtr のバイト長。
 * \param[out]    macPtr        生成した MAC の書き込み先。NULL 禁止。
 * \param[in,out] macLengthPtr  in: 欲しい切り詰めバイト数。out: 実際に書き込んだ
 *                              バイト数（本プロジェクトでは常に in と同じ）。NULL 禁止。
 *
 * \retval  E_OK      MAC を生成した。
 * \retval  E_NOT_OK  未初期化、NULL ポインタ、jobId が範囲外/種別不一致、
 *                    または要求長が CMAC 出力長(16byte)を超える。
 *
 * \AUTOSARReq     {SWS_Csm_00982}
 * \ServiceID      {0x60}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Csm_MacGenerate(uint32 jobId, Crypto_OperationModeType mode,
                                const uint8* dataPtr, uint32 dataLength,
                                uint8* macPtr, uint32* macLengthPtr);

/**
 * \brief   指定ジョブで MAC を検証する。
 *
 * \details [SWS_Csm_01050] の実測どおり、`macLength` はビット単位で渡す
 *          （Csm_MacGenerate の macLengthPtr がバイト単位なのと非対称な点に
 *          注意。本関数の内部でバイトへ変換してから下位層（Crypto_JobType）
 *          へ渡す）。検証結果（一致/不一致）は戻り値ではなく `*verifyPtr`
 *          に書き込まれる。戻り値はあくまで「検証処理自体が実行できたか」を
 *          表す（ジョブ ID 不正等は E_NOT_OK、MAC 不一致は E_OK かつ
 *          `*verifyPtr = CRYPTO_E_VER_NOT_OK`）。
 *
 * \param[in]   jobId       CSM_JOB_ID_* 定数。CRYPTO_MACVERIFY 用に設定
 *                          されたジョブでなければならない。
 * \param[in]   mode        本プロジェクトは常に CRYPTO_OPERATIONMODE_SINGLECALL。
 * \param[in]   dataPtr     MAC 検証対象データ。NULL 禁止。
 * \param[in]   dataLength  dataPtr のバイト長。
 * \param[in]   macPtr      検証したい（受信側の）MAC。NULL 禁止。
 * \param[in]   macLength   macPtr のビット長（8 の倍数であること）。
 * \param[out]  verifyPtr   検証結果の格納先。NULL 禁止。
 *
 * \retval  E_OK      検証処理を実行した（結果は verifyPtr 参照）。
 * \retval  E_NOT_OK  未初期化、NULL ポインタ、jobId が範囲外/種別不一致、
 *                    または macLength が 8 の倍数でない/CMAC 出力長を超える。
 *
 * \AUTOSARReq     {SWS_Csm_01050}
 * \ServiceID      {0x61}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Csm_MacVerify(uint32 jobId, Crypto_OperationModeType mode,
                              const uint8* dataPtr, uint32 dataLength,
                              const uint8* macPtr, uint32 macLength,
                              Crypto_VerifyResultType* verifyPtr);

/**
 * \brief   鍵要素（AES-128 鍵本体）を書き換える。
 *
 * \details KeyM の鍵更新セッション（KeyM_Update()）から呼ばれる想定。書き換え
 *          直後は当該鍵が無効化され、`Csm_KeySetValid()` が呼ばれるまで
 *          `Csm_MacGenerate()`/`Csm_MacVerify()` から使用できない
 *          （[SWS_KeyM_00016]/[SWS_Csm_00957] 参照）。
 *
 * \param[in]  keyId         書き換える鍵の ID（CRYPTO_KEY_* 定数）。
 * \param[in]  keyElementId  CRYPTO_KEY_ELEMENT_ID_CIPHER_KEY 固定。
 * \param[in]  keyPtr        新しい鍵バイト列。NULL 禁止。
 * \param[in]  keyLength     keyPtr のバイト長。CRYPTO_AES128_KEY_SIZE(16) 以外は拒否。
 *
 * \retval  E_OK      鍵を書き換えた。
 * \retval  E_NOT_OK  未初期化、NULL、または下位層が失敗。
 *
 * \AUTOSARReq     {SWS_Csm_00957}
 * \ServiceID      {0x78}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Csm_KeyElementSet(uint32 keyId, uint32 keyElementId,
                                  const uint8* keyPtr, uint32 keyLength);

/**
 * \brief   鍵を有効状態にする。
 *
 * \details KeyM の鍵更新セッション終了（KeyM_Finalize()）から呼ばれる想定。
 *
 * \param[in]  keyId  有効化する鍵の ID（CRYPTO_KEY_* 定数）。
 *
 * \retval  E_OK      有効化した。
 * \retval  E_NOT_OK  未初期化、または下位層が失敗。
 *
 * \AUTOSARReq     {SWS_Csm_00958}
 * \ServiceID      {0x67}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Csm_KeySetValid(uint32 keyId);

#ifdef __cplusplus
}
#endif

#endif /* CSM_H */
