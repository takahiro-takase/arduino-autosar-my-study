/**
 * \file    KeyM_PBCfg.h
 * \brief   Key Manager ポストビルド設定 型・宣言
 * \details 鍵名テーブルの型と外部参照を宣言する。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツールが生成する
 *          ファイルに相当する（DaVinci: KeyM/KeyMGeneral/KeyMCryptoKey
 *          コンテナ相当）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.4.0 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef KEYM_PBCFG_H
#define KEYM_PBCFG_H

#include "Platform_Types.h"
#include "KeyM_Cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   鍵名 1 件分の設定（DaVinci KeyMCryptoKey コンテナの簡略版）。
 *
 * \details 実 AUTOSAR の KeyMCryptoKey は KeyMCryptoKeyGenerationType（STORED/
 *          DERIVE）や KeyMCryptoKeyCsmVerifyJobRef 等、鍵導出・検証まで含む
 *          多数のパラメータを持つが、本プロジェクトは
 *          KEYM_STORED_KEY（鍵を直接上書きする方式）のみを使うため、
 *          「鍵名」と「対応する Csm 側 keyId（= CRYPTO_KEY_* 定数）」だけを
 *          持つフラット構造体に簡略化する（Csm_JobConfigType と同じ方針）。
 */
typedef struct
{
    uint8  KeyName;         /**< KEYM_CRYPTO_KEY_NAME_* 定数（1 バイト ASCII） */
    uint32 CsmKeyTargetRef; /**< Csm/CryIf/Crypto 側の鍵 ID（CRYPTO_KEY_* 定数） */
} KeyM_CryptoKeyConfigType;

/** 鍵名テーブル（KeyM_PBCfg.c で定義）。KEYM_CRYPTO_KEY_COUNT 件。 */
extern const KeyM_CryptoKeyConfigType KeyM_CryptoKeyConfigData[KEYM_CRYPTO_KEY_COUNT];

#ifdef __cplusplus
}
#endif

#endif /* KEYM_PBCFG_H */
