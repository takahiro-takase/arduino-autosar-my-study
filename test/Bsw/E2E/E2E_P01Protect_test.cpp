/**
 * \file    E2E_P01Protect_test.cpp
 * \brief   E2E_P01Protect() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 E2E_test.cpp）。
 *
 *          分割前の共通の説明は、旧 E2E_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "E2E.h"
#include "E2E_P01.h"
#include "E2E_P05.h"
#include "Wrap_E2E.h"
}

/* ======================================================================
 * Definitions
 * ====================================================================== */

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * Global Variables
 * ====================================================================== */

namespace
{

uint8_t ReferenceCrc8(const uint8_t *data, uint8_t len, uint8_t crc)
{
    for (uint8_t i = 0; i < len; i++)
    {
        crc = static_cast<uint8_t>(crc ^ data[i]);
        for (uint8_t bit = 0; bit < 8; bit++)
        {
            if (crc & 0x80U)
                crc = static_cast<uint8_t>((crc << 1) ^ 0x1DU);
            else
                crc = static_cast<uint8_t>(crc << 1);
        }
    }
    return crc;
}

uint8_t ReferenceCrc8ForFrame(const uint8_t *data, uint8_t dataLength, uint8_t crcOffset, uint16_t dataId)
{
    uint8_t crc = 0x00U;
    const uint8_t idLow  = static_cast<uint8_t>(dataId & 0xFFU);
    const uint8_t idHigh = static_cast<uint8_t>((dataId >> 8U) & 0xFFU);
    crc = ReferenceCrc8(&idLow,  1U, crc);
    crc = ReferenceCrc8(&idHigh, 1U, crc);
    if (crcOffset > 0U)
        crc = ReferenceCrc8(data, crcOffset, crc);
    if (static_cast<uint8_t>(crcOffset + 1U) < dataLength)
        crc = ReferenceCrc8(&data[crcOffset + 1U], static_cast<uint8_t>(dataLength - crcOffset - 1U), crc);
    return crc;
}

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* E2E Profile01（E2E_P01Protect / E2E_P01Check など）の単体テスト用フィクスチャ。
 * SetUp(): Profile01 の設定（DataID=0x0110、データ長 5 バイト、最大カウンタ差 1、CRC・カウンタのオフセット、同期カウンタ初期値 2）を用意する。 */
class E2E_P01Protect_Test : public ::testing::Test
{
protected:
    E2E_P01ConfigType config;

    void SetUp() override
    {
        /* AbsInfo (CAN 0x110, DLC=5) 相当: byte[0]=CRC8, byte[1]=Counter, byte[2-4]=データ */
        config.DataID          = 0x0110U;
        config.DataLength       = 5U;
        config.MaxDeltaCounter  = 1U;
        config.CounterOffset    = 1U;
        config.CRCOffset        = 0U;
        config.SyncCounterInit  = 2U;
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(E2E_P01Protect_Test, E2E_P01Protect_OK_ComputesCrcMatchingReferenceImplementationAndStartsAtCounterZero)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    E2E_P01ProtectStateType state;
    ASSERT_EQ(E2E_P01ProtectInit(&state), E2E_E_OK);

    uint8_t data[5] = {0U, 0U, 0x12U, 0x34U, 0x56U};

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = E2E_P01Protect(&config, &state, data);

    const uint8_t expectedCrc = ReferenceCrc8ForFrame(data, config.DataLength, config.CRCOffset, config.DataID);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_EQ(ret, E2E_E_OK);
    EXPECT_EQ(data[0], expectedCrc);
    EXPECT_EQ(data[1], 0U); /* 1回目の Counter は 0 */
}

TEST_F(E2E_P01Protect_Test, E2E_P01Protect_OK_IncrementsCounterAndWrapsAt14SkippingReservedValue15)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    E2E_P01ProtectStateType state;
    ASSERT_EQ(E2E_P01ProtectInit(&state), E2E_E_OK);
    state.Counter = 14U; /* 折り返し直前 (4bit、15は予約値のためスキップ) */

    uint8_t data[5] = {0U, 0U, 0U, 0U, 0U};

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = E2E_P01Protect(&config, &state, data);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_EQ(ret, E2E_E_OK);

    EXPECT_EQ(data[1], 14U);      /* 送信された Counter は折り返し前の 14 */
    EXPECT_EQ(state.Counter, 0U); /* 次回用の内部 Counter は 15 を飛ばして 0 */
}

}  // namespace
