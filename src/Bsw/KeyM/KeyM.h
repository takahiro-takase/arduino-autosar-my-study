/**
 * \file    KeyM.h
 * \brief   Key Manager 公開インタフェース (AUTOSAR SWS_KeyManager 準拠)
 * \details Csm/CryIf/Crypto レイヤ分離（SecOC から暗号処理を切り出した既存実装）の続き。
 *          実車では鍵マスター（診断ツール／バックエンド）から投入される新しい
 *          鍵材料を、鍵更新セッション（Start→Update→Finalize）として受け取り、
 *          Csm の鍵操作 API（Csm_KeyElementSet/Csm_KeySetValid）を呼び出す。
 *          本プロジェクトでは Dcm の WriteDataByIdentifier（新設 DID）が
 *          鍵マスター役を模擬する。
 *
 *          実 AUTOSAR の KeyM は下記を含む大規模なモジュールだが、本プロジェクトは
 *          スコープを大幅に絞る（ユーザー承認済み、Csm/CryIf/Crypto レイヤ分離時の
 *          Crypto_JobType 簡略化と同じ「簡略化フラット化」方針）。
 *          - Certificate submodule（X.509 証明書チェーン）は完全に対応除外。
 *            本プロジェクトに PKI の土台が一切ないため。
 *          - Crypto key submodule のうち、KeyM_Start/KeyM_Update/KeyM_Finalize の
 *            3 API のみ実装。KeyM_Prepare/KeyM_Verify、SHE M1M2M3 形式の鍵更新
 *            （keyNameLength=0 の経路）、KEYM_DERIVE_KEY（Csm_KeyDerive 経由の
 *            鍵導出）は対応除外。
 *          - 非同期コールバック機構（KeyM_CryptoKeyFinalizeCallbackNotification 等）
 *            は実装しない。本プロジェクトは Arduino 単一ループの同期呼び出しのみで
 *            構成されており、Csm_MacGenerate 等も同様に同期実装のため。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.4.0 仕様（docs/4.4.0/AUTOSAR_SWS_KeyManager.pdf）
 *          を参考にした学習用実装です。AUTOSAR 認証済み実装ではなく、製品への
 *          適用は想定していません。
 */
#ifndef KEYM_H
#define KEYM_H

#include "Std_Types.h"
#include "KeyM_Cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   KeyM_Init() の設定引数型（不透明型）。
 *
 * \details [SWS_KeyM_00158] により ConfigPtr は常に NULL_PTR でなければならない
 *          （ポストビルド設定を持たない）。本プロジェクトは中身を定義しない
 *          不完全型とし、ポインタとしてのみ扱う。
 */
typedef struct KeyM_ConfigType_Tag KeyM_ConfigType;

/**
 * \brief   鍵更新セッションの開始モード (AUTOSAR KeyM_StartType の全値)
 * \details 値は docs/4.4.0/AUTOSAR_SWS_KeyManager.pdf 8.7.2.1 章を実測して確認済み。
 *          [SWS_KeyM_00087] によりデフォルト動作ではこの値の中身を検査しない
 *          （どちらの値でもセッションを開始できる）。
 */
typedef enum
{
    KEYM_START_OEM_PRODUCTIONMODE = 0x01U,
    KEYM_START_WORKSHOPMODE       = 0x02U
} KeyM_StartType;

/**
 * \brief   Key Manager モジュールを初期化する。
 *
 * \details [SWS_KeyM_00046] 相当: 鍵材料は Crypto_Init() が Crypto_PBCfg.c の
 *          初期値から RAM キースロットへロードする（本プロジェクトの簡略化。
 *          実際の NVM 永続化は行わない）。KeyM_Init() 自体はセッション状態を
 *          初期化するのみで、鍵材料には触れない。
 *
 * \param[in]  ConfigPtr  常に NULL_PTR を渡すこと（[SWS_KeyM_00158]）。
 *
 * \AUTOSARReq     {SWS_KeyM_00043}
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void KeyM_Init(const KeyM_ConfigType* ConfigPtr);

/**
 * \brief   Key Manager モジュールを未初期化状態に戻す。
 *
 * \details [SWS_KeyM_00048] は RAM 上の鍵材料の実消去を要求するが、本プロジェクトの
 *          簡略化では KeyM は鍵材料そのものを保持しない（Crypto 層が保持する）
 *          ため、KeyM 側では内部セッション状態のリセットのみ行う。実消去には
 *          Crypto 層に専用の破棄 API が必要になるが、本プロジェクトでは
 *          未実装（スコープ外）。
 *
 * \AUTOSARReq     {SWS_KeyM_00047}
 * \ServiceID      {0x02}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void KeyM_Deinit(void);

/**
 * \brief   Key Manager モジュールのバージョン情報を取得する。
 *
 * \details [SWS_KeyM_00144] により未初期化時は KEYM_E_UNINIT を報告する
 *          （Csm/CryIf と同じく GetVersionInfo が UNINIT 例外にならない）。
 *
 * \param[out]  versioninfo  バージョン情報の格納先。NULL 禁止。
 *
 * \ServiceID      {0x03}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void KeyM_GetVersionInfo(Std_VersionInfoType* versioninfo);

/**
 * \brief   鍵更新セッションを開始する。
 *
 * \details [SWS_KeyM_00087] によりデフォルトでは RequestData の中身を検査せず、
 *          常にセッションを開くだけの動作をする（本プロジェクトも同様）。
 *          既にセッションが開いている状態で呼んでも E_OK を返し継続する
 *          （[SWS_KeyM_00086]）。
 *
 * \param[in]     StartType           セッション開始モード（本実装では未使用）。
 * \param[in]     RequestData         未使用（本実装では無視する）。
 * \param[in]     RequestDataLength   未使用。
 * \param[out]    ResponseData        未使用（本実装では書き込まない）。
 * \param[in,out] ResponseDataLength  未使用（本実装では変更しない）。
 *
 * \retval  E_OK      セッションを開いた（以降 KeyM_Update() が受理される）。
 * \retval  E_NOT_OK  未初期化。
 *
 * \AUTOSARReq     {SWS_KeyM_00050}
 * \ServiceID      {0x04}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType KeyM_Start(KeyM_StartType StartType,
                          const uint8* RequestData, uint16 RequestDataLength,
                          uint8* ResponseData, uint16* ResponseDataLength);

/**
 * \brief   鍵の更新を実行する。
 *
 * \details [SWS_KeyM_00091]/[SWS_KeyM_00013] 相当: `KeyNamePtr` を
 *          `KeyM_CryptoKeyConfigData`（KeyM_PBCfg.c）から検索し、一致する
 *          エントリの `CsmKeyTargetRef` へ `RequestDataPtr` の内容を
 *          `Csm_KeyElementSet()` で書き込む（KEYM_STORED_KEY 方式のみ対応。
 *          SHE M1M2M3 形式・KEYM_DERIVE_KEY は未対応）。
 *          セッションが開いていない場合（`KeyM_Start()` 未呼び出し）は拒否する。
 *          書き込んだ鍵は `KeyM_Finalize()` が呼ばれるまで無効のまま
 *          （`Csm_MacGenerate`/`Csm_MacVerify` から使用不可）。
 *
 * \param[in]   KeyNamePtr           鍵名（KEYM_CRYPTO_KEY_NAME_* の1バイト値）。NULL 禁止。
 * \param[in]   KeyNameLength        KeyNamePtr の長さ。1 のみ対応（0 は SHE 形式扱いのため未対応）。
 * \param[in]   RequestDataPtr       新しい鍵バイト列。NULL 禁止。
 * \param[in]   RequestDataLength    RequestDataPtr のバイト長。CRYPTO_AES128_KEY_SIZE(16) 以外は拒否。
 * \param[out]  ResultDataPtr        未使用（本実装では書き込まない）。
 * \param[in]   ResultDataMaxLength  未使用。
 *
 * \retval  E_OK      鍵を更新した（Finalize されるまで無効状態）。
 * \retval  E_NOT_OK  未初期化、セッション未開始、鍵名不一致、NULL、長さ不正、
 *                    または下位層（Csm）が失敗。
 *
 * \AUTOSARReq     {SWS_KeyM_00052}
 * \ServiceID      {0x06}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType KeyM_Update(const uint8* KeyNamePtr, uint16 KeyNameLength,
                           const uint8* RequestDataPtr, uint16 RequestDataLength,
                           uint8* ResultDataPtr, uint16 ResultDataMaxLength);

/**
 * \brief   鍵更新セッションを終了し、更新した鍵を有効化する。
 *
 * \details [SWS_KeyM_00103]/[SWS_KeyM_00106] 相当: セッション中に
 *          `KeyM_Update()` で更新された全ての鍵に対して `Csm_KeySetValid()`
 *          を呼ぶ（1件が失敗しても残りの鍵の有効化は継続する）。呼び出し後は
 *          更新フラグをすべてクリアし、セッションを閉じる（成否に関わらず）。
 *
 * \param[in]     RequestDataPtr         未使用（本実装では無視する）。
 * \param[in]     RequestDataLength      未使用。
 * \param[out]    ResponseDataPtr        未使用（本実装では書き込まない）。
 * \param[in]     ResponseMaxDataLength  未使用。
 *
 * \retval  E_OK      セッション中に更新された全ての鍵を有効化できた
 *                    （更新された鍵が1つもない場合も E_OK）。
 * \retval  E_NOT_OK  未初期化、セッション未開始、または1つ以上の鍵の有効化に失敗。
 *
 * \AUTOSARReq     {SWS_KeyM_00053}
 * \ServiceID      {0x07}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType KeyM_Finalize(const uint8* RequestDataPtr, uint16 RequestDataLength,
                             uint8* ResponseDataPtr, uint16 ResponseMaxDataLength);

#ifdef __cplusplus
}
#endif

#endif /* KEYM_H */
