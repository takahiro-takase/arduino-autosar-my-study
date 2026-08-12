/**
 * \file    Crypto_Cmac.c
 * \brief   AES-CMAC (NIST SP 800-38B) 実装
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include "Crypto_Cmac.h"
#include "Det.h"

#define TAG "Crypto_Cmac"

#define CRYPTO_CMAC_RB  0x87U  /* NIST SP 800-38B 5.3: ブロック長128bit用の定数 Rb */

/**
 * \brief  16 バイトを 1 ビット左シフトする（ビッグエンディアン、128bit 整数として）。
 * \return 元の値の MSB（シフトで溢れたビット）。
 */
static uint8 Crypto_Cmac_LeftShiftOneBit(const uint8 in[16], uint8 out[16])
{
    DET_LOGT(TAG, "called");
    const uint8 msb = (uint8)((in[0] & 0x80U) != 0U ? 1U : 0U);
    uint8 carry = 0U;
    for (sint8 i = 15; i >= 0; i--)
    {
        const uint8 nextCarry = (uint8)((in[i] & 0x80U) != 0U ? 1U : 0U);
        out[i] = (uint8)((uint8)(in[i] << 1) | carry);
        carry = nextCarry;
    }
    return msb;
}

/**
 * \brief  SP 800-38B 6.1 "Subkey Generation" の generate_subkey(L) を計算する。
 */
static void Crypto_Cmac_GenerateSubkey(const uint8 in[16], uint8 out[16])
{
    DET_LOGT(TAG, "called");
    const uint8 msb = Crypto_Cmac_LeftShiftOneBit(in, out);
    if (msb != 0U)
        out[15] ^= CRYPTO_CMAC_RB;
}

static void Crypto_Cmac_XorBlock(const uint8 a[16], const uint8 b[16], uint8 out[16])
{
    DET_LOGT(TAG, "called");
    for (uint8 i = 0U; i < 16U; i++)
        out[i] = (uint8)(a[i] ^ b[i]);
}

void Crypto_Cmac_Calculate(const uint8 key[CRYPTO_AES128_KEY_SIZE],
                            const uint8* message,
                            uint16       messageLen,
                            uint8        mac[CRYPTO_CMAC_SIZE])
{
    DET_LOGT(TAG, "called");
    uint8 zero[16] = { 0U };
    uint8 l[16];
    uint8 k1[16];
    uint8 k2[16];

    /* Step 1-3: L = AES_Encrypt(key, 0^128); K1 = generate_subkey(L); K2 = generate_subkey(K1) */
    Crypto_Aes128_EncryptBlock(key, zero, l);
    Crypto_Cmac_GenerateSubkey(l, k1);
    Crypto_Cmac_GenerateSubkey(k1, k2);

    /* Step 4: メッセージをブロック数へ分割（0 バイトでも 1 ブロック扱い） */
    uint16 blockCount = (uint16)((messageLen + 15U) / 16U);
    uint8  lastBlockIsComplete;
    if (blockCount == 0U)
    {
        blockCount = 1U;
        lastBlockIsComplete = 0U;
    }
    else
    {
        lastBlockIsComplete = (uint8)(((messageLen % 16U) == 0U) ? 1U : 0U);
    }

    /* Step 5: 最終ブロック Mn* を用意（完全なら K1、不完全なら 0x80+ゼロ埋め後 K2 を XOR） */
    uint8 mLast[16] = { 0U };
    const uint16 lastBlockOffset = (uint16)((blockCount - 1U) * 16U);
    const uint16 lastBlockLen    = (uint16)(messageLen - lastBlockOffset);

    if (lastBlockIsComplete != 0U)
    {
        for (uint16 i = 0U; i < 16U; i++)
            mLast[i] = message[lastBlockOffset + i];
        Crypto_Cmac_XorBlock(mLast, k1, mLast);
    }
    else
    {
        for (uint16 i = 0U; i < lastBlockLen; i++)
            mLast[i] = message[lastBlockOffset + i];
        mLast[lastBlockLen] = 0x80U;  /* SP800-38B: パディング先頭ビットは 1、残りは 0 */
        Crypto_Cmac_XorBlock(mLast, k2, mLast);
    }

    /* Step 6: CBC-MAC 連鎖。X0 = 0^128 */
    uint8 x[16];
    for (uint8 i = 0U; i < 16U; i++)
        x[i] = 0U;

    for (uint16 blockIdx = 0U; blockIdx < (uint16)(blockCount - 1U); blockIdx++)
    {
        uint8 y[16];
        Crypto_Cmac_XorBlock(x, &message[blockIdx * 16U], y);
        Crypto_Aes128_EncryptBlock(key, y, x);
    }

    uint8 y[16];
    Crypto_Cmac_XorBlock(x, mLast, y);
    Crypto_Aes128_EncryptBlock(key, y, mac);
}
