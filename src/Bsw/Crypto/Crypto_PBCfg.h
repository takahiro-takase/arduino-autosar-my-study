/**
 * \file    Crypto_PBCfg.h
 * \brief   Crypto Driver ポストビルド設定 宣言
 * \details 鍵テーブル実体（Crypto_PBCfg.c）への外部参照を宣言する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef CRYPTO_PBCFG_H
#define CRYPTO_PBCFG_H

#include "Crypto_Aes128.h"
#include "Crypto_Cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 鍵テーブル（CRYPTO_KEY_* 定数で添字アクセスする）。Crypto_PBCfg.c で定義。 */
extern const uint8 Crypto_KeyTable[CRYPTO_KEY_COUNT][CRYPTO_AES128_KEY_SIZE];

#ifdef __cplusplus
}
#endif

#endif /* CRYPTO_PBCFG_H */
