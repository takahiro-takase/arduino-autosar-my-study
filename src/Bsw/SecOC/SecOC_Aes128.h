/**
 * \file    SecOC_Aes128.h
 * \brief   AES-128 単一ブロック暗号 (FIPS-197 準拠)
 * \details SecOC の Authenticator 計算（AES-CMAC の下位プリミティブ）専用。
 *          暗号化のみを提供し、復号は実装しない（CMAC は暗号化のみで構成できる）。
 *          外部ライブラリに依存しない自前実装（実 AUTOSAR は Csm/CryIf 経由で
 *          Crypto Driver を呼ぶが、本実装は学習のためこの層を割愛する）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef SECOC_AES128_H
#define SECOC_AES128_H

#include "Platform_Types.h"
#include "Std_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SECOC_AES128_BLOCK_SIZE  16U  /* AES のブロック長は鍵長によらず常に 128bit = 16byte */
#define SECOC_AES128_KEY_SIZE    16U  /* AES-128 の鍵長 */

/**
 * \brief   AES-128 の 1 ブロック（16 バイト）を暗号化する。
 *
 * \details FIPS-197 準拠の鍵拡張（11 ラウンド鍵、176 バイト）とラウンド処理
 *          （SubBytes/ShiftRows/MixColumns/AddRoundKey、最終ラウンドのみ
 *          MixColumns を省略）を行う。ブロック暗号のみを提供し、CBC/ECB 等の
 *          利用モードは呼び出し側（SecOC_Cmac）の責務とする。
 *
 * \param[in]  key         16 バイトの AES-128 鍵。NULL 禁止。
 * \param[in]  plaintext   暗号化する 16 バイトの平文ブロック。NULL 禁止。
 * \param[out] ciphertext  暗号化結果を書き込む 16 バイトのバッファ。NULL 禁止。
 *                         plaintext と同じポインタを渡してよい（内部で
 *                         作業用コピーを保持するため、in-place 呼び出しに
 *                         対応する）。
 */
void SecOC_Aes128_EncryptBlock(const uint8 key[SECOC_AES128_KEY_SIZE],
                                const uint8 plaintext[SECOC_AES128_BLOCK_SIZE],
                                uint8 ciphertext[SECOC_AES128_BLOCK_SIZE]);

/**
 * \brief   FIPS-197 Appendix B の公式既知テストベクタで自己診断する。
 *
 * \details key=000102030405060708090a0b0c0d0e0f,
 *          plaintext=00112233445566778899aabbccddeeff に対し、
 *          ciphertext=69c4e0d86a7b0430d8cdb78070b4c55a が得られるかを検証する。
 *          `SecOC_Init()` から起動時セルフテストとして呼ばれ、実装の正しさを
 *          実機ログで確認できるようにする（他の Com 機能と同じ「実機で検証
 *          可能」を重視する方針に合わせる）。
 *
 * \retval  E_OK      既知テストベクタと一致した（実装は正しい）。
 * \retval  E_NOT_OK  不一致（実装に誤りがある）。
 */
Std_ReturnType SecOC_Aes128_SelfTest(void);

#ifdef __cplusplus
}
#endif

#endif /* SECOC_AES128_H */
