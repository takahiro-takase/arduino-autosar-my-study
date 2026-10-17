/**
 * \file    E2E_P05Protect_test.cpp
 * \brief   E2E_P05Protect() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 E2E_test.cpp）。
 *
 *          E2E_P05.c は実 HW 依存の無い自己完結したロジックのため、フェイク
 *          実装は不要で公開 API (E2E_P05Protect/E2E_P05Check とその Init) を
 *          直接呼んで検証する。E2E_CalcCrc16() 等の内部ヘルパーは static の
 *          ため、CRC の正しさは「本テストファイル内に SWS_E2E_00406 の
 *          擬似コードを独立に書き起こした参照実装」との突き合わせで検証する
 *          (E2E_P05.c 側の実装をコピーするのではなく、仕様書の記述から
 *          改めて書き起こすことで、実装のコピペミスを検出できるようにする)。
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

uint16_t ReferenceCrc16(const uint8_t *data, uint8_t len, uint16_t crc)
{
    for (uint8_t i = 0; i < len; i++)
    {
        crc = static_cast<uint16_t>(crc ^ (static_cast<uint16_t>(data[i]) << 8));
        for (uint8_t bit = 0; bit < 8; bit++)
        {
            if (crc & 0x8000U)
                crc = static_cast<uint16_t>((crc << 1) ^ 0x1021U);
            else
                crc = static_cast<uint16_t>(crc << 1);
        }
    }
    return crc;
}

uint16_t ReferenceCrcForFrame(const uint8_t *data, uint8_t dataLength, uint8_t offset, uint16_t dataId)
{
    uint16_t crc = 0xFFFFU;
    crc = ReferenceCrc16(&data[offset + 2U], static_cast<uint8_t>(dataLength - offset - 2U), crc);
    const uint8_t idLow  = static_cast<uint8_t>(dataId & 0xFFU);
    const uint8_t idHigh = static_cast<uint8_t>((dataId >> 8U) & 0xFFU);
    crc = ReferenceCrc16(&idLow,  1U, crc);
    crc = ReferenceCrc16(&idHigh, 1U, crc);
    return crc;
}

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* E2E Profile05（E2E_P05Protect / E2E_P05Check など）の単体テスト用フィクスチャ。
 * SetUp(): Profile05 の設定（DataID=0x0220、データ長 5 バイト、最大カウンタ差 1）を用意する。 */
class E2E_P05Protect_Test : public ::testing::Test
{
protected:
    E2E_P05ConfigType config;

    void SetUp() override
    {
        config.DataID          = 0x0220U;
        config.DataLength       = 5U; /* CRC16(2B) + Counter(1B) + userdata(2B) */
        config.MaxDeltaCounter  = 1U;
        config.Offset           = 0U;
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(E2E_P05Protect_Test, E2E_P05Protect_NG_NullConfigReturnsInputErrNull)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    E2E_P05ProtectStateType state;
    E2E_P05ProtectInit(&state);
    uint8_t data[5] = { 0U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = E2E_P05Protect(nullptr, &state, data, sizeof(data));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E2E_E_INPUTERR_NULL);
}

TEST_F(E2E_P05Protect_Test, E2E_P05Protect_NG_NullStateReturnsInputErrNull)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8_t data[5] = { 0U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = E2E_P05Protect(&config, nullptr, data, sizeof(data));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E2E_E_INPUTERR_NULL);
}

TEST_F(E2E_P05Protect_Test, E2E_P05Protect_NG_NullDataReturnsInputErrNull)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    E2E_P05ProtectStateType state;
    E2E_P05ProtectInit(&state);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = E2E_P05Protect(&config, &state, nullptr, 5U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E2E_E_INPUTERR_NULL);
    EXPECT_EQ(state.Counter, 0U);   // 状態は進めない
}

TEST_F(E2E_P05Protect_Test, E2E_P05Protect_NG_LengthShorterThanDataLengthReturnsInputErrWrong)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    E2E_P05ProtectStateType state;
    E2E_P05ProtectInit(&state);
    uint8_t data[5] = { 0U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = E2E_P05Protect(&config, &state, data, config.DataLength - 1U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E2E_E_INPUTERR_WRONG);
    EXPECT_EQ(state.Counter, 0U);   // 状態は進めない
}

TEST_F(E2E_P05Protect_Test, E2E_P05ProtectInit_NG_NullStateReturnsInputErrNull)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = E2E_P05ProtectInit(nullptr);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E2E_E_INPUTERR_NULL);
}

TEST_F(E2E_P05Protect_Test, E2E_P05Protect_OK_ComputesCrcMatchingReferenceImplementation)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    E2E_P05ProtectStateType state;
    E2E_P05ProtectInit(&state);

    uint8_t data[5] = {0U, 0U, 0U, 0x12U, 0x34U}; /* byte[3-4] = ユーザーデータ */

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    E2E_P05Protect(&config, &state, data, sizeof(data));

    const uint16_t receivedCrc  = static_cast<uint16_t>(data[0]) | static_cast<uint16_t>(data[1] << 8);
    const uint16_t expectedCrc  = ReferenceCrcForFrame(data, config.DataLength, config.Offset, config.DataID);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(receivedCrc, expectedCrc);
    EXPECT_EQ(data[2], 0U); /* 1回目の Counter は 0 */
}

TEST_F(E2E_P05Protect_Test, E2E_P05Protect_OK_IncrementsCounterAndWrapsAt0xFF)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    E2E_P05ProtectStateType state;
    E2E_P05ProtectInit(&state);
    state.Counter = 0xFFU; /* 折り返し直前まで進めておく */

    uint8_t data[5] = {0U, 0U, 0U, 0U, 0U};

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    E2E_P05Protect(&config, &state, data, sizeof(data));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(data[2], 0xFFU);   /* 送信されたCounterは折り返し前の0xFF */
    EXPECT_EQ(state.Counter, 0U); /* 次回用の内部Counterは0に折り返す */
}

}  // namespace
