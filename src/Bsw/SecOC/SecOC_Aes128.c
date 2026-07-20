/**
 * \file    SecOC_Aes128.c
 * \brief   AES-128 単一ブロック暗号 実装 (FIPS-197 準拠)
 * \details 鍵拡張（Key Expansion）とラウンド関数（SubBytes/ShiftRows/
 *          MixColumns/AddRoundKey）を素直に実装した教科書的な AES-128。
 *          速度・コードサイズ最適化（T-box 等）は行わない。RA4M1
 *          （48MHz Cortex-M4）であれば 1 ブロック暗号化は十分高速であり、
 *          SecOC の検証で必要な呼び出し回数（1 フレームあたり 1 回）に対して
 *          最適化の必要はないと判断した。
 *
 *          状態配列 State[16] は FIPS-197 の規約に従い列優先で格納する
 *          （State[row + 4*col] = 入力バイト列の (row + 4*col) 番目）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include "SecOC_Aes128.h"
#include "Det.h"

#define TAG "SecOC_Aes"

#define AES128_NK   4U   /* 鍵長 [32bit word] */
#define AES128_NB   4U   /* ブロック長 [32bit word]（AES は常に 4） */
#define AES128_NR   10U  /* ラウンド数（AES-128 は 10） */
#define AES128_EXPANDED_KEY_SIZE  ((AES128_NR + 1U) * AES128_NB * 4U)  /* 176 byte */

/* FIPS-197 Figure 7: AES S-box */
static const uint8 SecOC_Aes128_Sbox[256] = {
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
};

/* FIPS-197 5.2: AES-128 のラウンド定数 Rcon[1..10]（word の先頭バイトのみ非ゼロ） */
static const uint8 SecOC_Aes128_Rcon[10] = {
    0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x1B,0x36
};

/* -----------------------------------------------------------------------
 * GF(2^8) 演算（AES の既約多項式 x^8+x^4+x^3+x+1 = 0x11B）
 * ----------------------------------------------------------------------- */

/**
 * \brief  GF(2^8) 上での乗算（AES の MixColumns で使用）。
 * \details ロシアン農民乗算法（シフト＋条件付き XOR）。ルックアップテーブルを
 *          使わず素直に計算する（可読性優先。1 ブロックあたり最大 16 回×8bit
 *          ループなので RA4M1 では無視できるコスト）。
 */
static uint8 SecOC_Aes128_GMul(uint8 a, uint8 b)
{
    uint8 p = 0U;
    for (uint8 i = 0U; i < 8U; i++)
    {
        if ((b & 1U) != 0U)
            p ^= a;
        const uint8 hiBitSet = (uint8)(a & 0x80U);
        a = (uint8)(a << 1);
        if (hiBitSet != 0U)
            a ^= 0x1BU;
        b = (uint8)(b >> 1);
    }
    return p;
}

/* -----------------------------------------------------------------------
 * 鍵拡張
 * ----------------------------------------------------------------------- */

static void SecOC_Aes128_KeyExpansion(const uint8 key[SECOC_AES128_KEY_SIZE],
                                       uint8 expandedKey[AES128_EXPANDED_KEY_SIZE])
{
    uint8 temp[4];

    /* 先頭 Nk word はそのまま鍵をコピー */
    for (uint8 i = 0U; i < AES128_NK * 4U; i++)
        expandedKey[i] = key[i];

    for (uint8 wordIdx = AES128_NK; wordIdx < AES128_NB * (AES128_NR + 1U); wordIdx++)
    {
        const uint16 prevOffset = (uint16)((wordIdx - 1U) * 4U);
        temp[0] = expandedKey[prevOffset + 0U];
        temp[1] = expandedKey[prevOffset + 1U];
        temp[2] = expandedKey[prevOffset + 2U];
        temp[3] = expandedKey[prevOffset + 3U];

        if ((wordIdx % AES128_NK) == 0U)
        {
            /* RotWord: [a0,a1,a2,a3] -> [a1,a2,a3,a0] */
            const uint8 rotated0 = temp[0];
            temp[0] = temp[1];
            temp[1] = temp[2];
            temp[2] = temp[3];
            temp[3] = rotated0;

            /* SubWord: 各バイトへ S-box を適用 */
            temp[0] = SecOC_Aes128_Sbox[temp[0]];
            temp[1] = SecOC_Aes128_Sbox[temp[1]];
            temp[2] = SecOC_Aes128_Sbox[temp[2]];
            temp[3] = SecOC_Aes128_Sbox[temp[3]];

            temp[0] ^= SecOC_Aes128_Rcon[(wordIdx / AES128_NK) - 1U];
        }

        const uint16 curOffset  = (uint16)(wordIdx * 4U);
        const uint16 backOffset = (uint16)((wordIdx - AES128_NK) * 4U);
        expandedKey[curOffset + 0U] = (uint8)(expandedKey[backOffset + 0U] ^ temp[0]);
        expandedKey[curOffset + 1U] = (uint8)(expandedKey[backOffset + 1U] ^ temp[1]);
        expandedKey[curOffset + 2U] = (uint8)(expandedKey[backOffset + 2U] ^ temp[2]);
        expandedKey[curOffset + 3U] = (uint8)(expandedKey[backOffset + 3U] ^ temp[3]);
    }
}

/* -----------------------------------------------------------------------
 * ラウンド関数（State[row + 4*col] の列優先レイアウト、FIPS-197 3.4 準拠）
 * ----------------------------------------------------------------------- */

static void SecOC_Aes128_AddRoundKey(uint8 state[16], const uint8* roundKey)
{
    for (uint8 i = 0U; i < 16U; i++)
        state[i] ^= roundKey[i];
}

static void SecOC_Aes128_SubBytes(uint8 state[16])
{
    for (uint8 i = 0U; i < 16U; i++)
        state[i] = SecOC_Aes128_Sbox[state[i]];
}

static void SecOC_Aes128_ShiftRows(uint8 state[16])
{
    uint8 tmp[16];
    for (uint8 row = 0U; row < 4U; row++)
    {
        for (uint8 col = 0U; col < 4U; col++)
        {
            /* row 行を左へ row 回巡回シフト */
            tmp[row + 4U * col] = state[row + 4U * ((col + row) % 4U)];
        }
    }
    for (uint8 i = 0U; i < 16U; i++)
        state[i] = tmp[i];
}

static void SecOC_Aes128_MixColumns(uint8 state[16])
{
    for (uint8 col = 0U; col < 4U; col++)
    {
        const uint8 s0 = state[4U * col + 0U];
        const uint8 s1 = state[4U * col + 1U];
        const uint8 s2 = state[4U * col + 2U];
        const uint8 s3 = state[4U * col + 3U];

        state[4U * col + 0U] = (uint8)(SecOC_Aes128_GMul(s0, 2U) ^ SecOC_Aes128_GMul(s1, 3U) ^ s2 ^ s3);
        state[4U * col + 1U] = (uint8)(s0 ^ SecOC_Aes128_GMul(s1, 2U) ^ SecOC_Aes128_GMul(s2, 3U) ^ s3);
        state[4U * col + 2U] = (uint8)(s0 ^ s1 ^ SecOC_Aes128_GMul(s2, 2U) ^ SecOC_Aes128_GMul(s3, 3U));
        state[4U * col + 3U] = (uint8)(SecOC_Aes128_GMul(s0, 3U) ^ s1 ^ s2 ^ SecOC_Aes128_GMul(s3, 2U));
    }
}

void SecOC_Aes128_EncryptBlock(const uint8 key[SECOC_AES128_KEY_SIZE],
                                const uint8 plaintext[SECOC_AES128_BLOCK_SIZE],
                                uint8 ciphertext[SECOC_AES128_BLOCK_SIZE])
{
    uint8 expandedKey[AES128_EXPANDED_KEY_SIZE];
    uint8 state[16];

    SecOC_Aes128_KeyExpansion(key, expandedKey);

    for (uint8 i = 0U; i < 16U; i++)
        state[i] = plaintext[i];

    SecOC_Aes128_AddRoundKey(state, &expandedKey[0]);

    for (uint8 round = 1U; round < AES128_NR; round++)
    {
        SecOC_Aes128_SubBytes(state);
        SecOC_Aes128_ShiftRows(state);
        SecOC_Aes128_MixColumns(state);
        SecOC_Aes128_AddRoundKey(state, &expandedKey[round * 16U]);
    }

    /* 最終ラウンド（AES-128 の 10 ラウンド目）は MixColumns を行わない */
    SecOC_Aes128_SubBytes(state);
    SecOC_Aes128_ShiftRows(state);
    SecOC_Aes128_AddRoundKey(state, &expandedKey[AES128_NR * 16U]);

    for (uint8 i = 0U; i < 16U; i++)
        ciphertext[i] = state[i];
}

Std_ReturnType SecOC_Aes128_SelfTest(void)
{
    /* FIPS-197 Appendix B の公式既知テストベクタ (Known Answer Test) */
    static const uint8 kKey[SECOC_AES128_KEY_SIZE] = {
        0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f
    };
    static const uint8 kPlaintext[SECOC_AES128_BLOCK_SIZE] = {
        0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff
    };
    static const uint8 kExpectedCiphertext[SECOC_AES128_BLOCK_SIZE] = {
        0x69,0xc4,0xe0,0xd8,0x6a,0x7b,0x04,0x30,0xd8,0xcd,0xb7,0x80,0x70,0xb4,0xc5,0x5a
    };

    uint8 actual[SECOC_AES128_BLOCK_SIZE];
    SecOC_Aes128_EncryptBlock(kKey, kPlaintext, actual);

    for (uint8 i = 0U; i < SECOC_AES128_BLOCK_SIZE; i++)
    {
        if (actual[i] != kExpectedCiphertext[i])
        {
            DET_LOGE(TAG, "AES-128 KAT FAIL at byte %u", (unsigned)i);
            return E_NOT_OK;
        }
    }

    DET_LOGI(TAG, "AES-128 KAT PASS (FIPS-197 Appendix B)");
    return E_OK;
}
