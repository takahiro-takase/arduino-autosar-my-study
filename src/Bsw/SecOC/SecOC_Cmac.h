/**
 * \file    SecOC_Cmac.h
 * \brief   AES-CMAC (NIST SP 800-38B) 実装
 * \details AES-128 をブロック暗号プリミティブとして使う CMAC（Cipher-based
 *          Message Authentication Code）。SecOC Profile 1
 *          (24Bit-CMAC-8Bit-FV、docs/AUTOSAR_SWS_SecureOnboardCommunication.pdf
 *          の [SWS_SecOC_00192]) が規定する認証アルゴリズムそのもの。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef SECOC_CMAC_H
#define SECOC_CMAC_H

#include "Platform_Types.h"
#include "SecOC_Aes128.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SECOC_CMAC_SIZE  16U  /* CMAC の完全な出力長（128bit）。切り詰めは呼び出し側の責務 */

/**
 * \brief   AES-128-CMAC を計算する（NIST SP 800-38B、任意長メッセージ対応）。
 *
 * \details サブ鍵 K1/K2 の導出（SP 800-38B 6.1 章 Subkey Generation、
 *          定数 Rb=0x87）、パディング（メッセージ長が 16 バイトの正の倍数
 *          なら K1、それ以外（0 バイトを含む）なら 0x80 + ゼロ埋め後 K2 を
 *          最終ブロックへ XOR）、CBC-MAC 連鎖（X0=0、Xi=AES_Encrypt(key,
 *          Xi-1 XOR Mi)）を行う。メッセージ長に制限はない（16 バイト単位に
 *          分割して逐次処理する）。
 *
 * \param[in]  key      16 バイトの AES-128 鍵。NULL 禁止。
 * \param[in]  message  MAC を計算するメッセージ。NULL 禁止（ただし
 *                      messageLen==0 の場合は空メッセージとして扱う）。
 * \param[in]  messageLen  message のバイト長。
 * \param[out] mac      計算結果（16 バイト、完全長）を書き込むバッファ。
 *                      NULL 禁止。切り詰めは呼び出し側が行うこと。
 */
void SecOC_Cmac_Calculate(const uint8 key[SECOC_AES128_KEY_SIZE],
                           const uint8* message,
                           uint16       messageLen,
                           uint8        mac[SECOC_CMAC_SIZE]);

#ifdef __cplusplus
}
#endif

#endif /* SECOC_CMAC_H */
