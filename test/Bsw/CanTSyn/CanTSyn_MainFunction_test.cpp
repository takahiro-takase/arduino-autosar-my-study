/**
 * \file    CanTSyn_MainFunction_test.cpp
 * \brief   CanTSyn_MainFunction() の単体テスト
 * \details 待機中に CANTSYN_TX_PERIOD_MS が経過していれば SYNC（Type は CANTSYN_TX_TYPE_SYNC、CRC 付きなら Byte1 に CRC、時間領域 0、SC、秒）を送信し、周期内は送らない。
 *          SYNC の送信完了待ちが CANTSYN_MASTER_CONFIRMATION_TIMEOUT_MS を超えたら送信を取り消して待機へ戻り（[SWS_CanTSyn_00033]）、
 *          次の周期で SYNC をやり直す（SC は進む）。CanIf_Transmit() が拒否した場合も SC を進めて待機のまま次の周期でやり直す。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "CanTSyn.h"
#include "CanTSyn_Cfg.h"
#include "Fake_Millis.h"
#include "Fake_Det_Hw.h"
#include "Wrap_CanIf.h"
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

/** CRC-8H2F（多項式 0x2F、初期値 0xFF、最終 XOR 0xFF）の、テスト用の別実装。
 *  CanTSyn.c の実装と照合するための基準で、自身も検査値 "123456789" → 0xDF で確かめる。 */
uint8 ReferenceCrc8H2F(const uint8* data, uint32 length)
{
    uint8 crc = 0xFFU;
    for (uint32 i = 0U; i < length; i++)
    {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++)
        {
            crc = (crc & 0x80U) ? static_cast<uint8>((crc << 1) ^ 0x2FU) : static_cast<uint8>(crc << 1);
        }
    }
    return static_cast<uint8>(crc ^ 0xFFU);
}

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* CanTSyn_MainFunction() の単体テスト用フィクスチャ。
 * SetUp(): 模擬時刻・wrap・DET 記録を初期化し、CanIf_Transmit() を成功固定にして CanTSyn_Init(NULL) を呼ぶ。
 * TearDown(): wrap を戻す。 */
class CanTSyn_MainFunction_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        WrapCanIf_Reset();
        FakeMillis_Reset();
        FakeDetHw_Reset();
        FakeDetHw_LogSuppressed = 1U;
        FailFromCallCount_CanIf_Transmit = 1U;   // 1 回目から強制戻り値（成功）
        ForcedReturn_CanIf_Transmit      = E_OK;
        FakeMillis_Value                 = 12345UL;
        CanTSyn_Init(NULL);
        FakeDetHw_Reset();                       // Init の記録を検証対象から除く
        FakeDetHw_LogSuppressed = 0U;
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        WrapCanIf_Reset();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(CanTSyn_MainFunction_Test, CanTSyn_MainFunction_NG_NoResendWithinPeriod)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanTSyn_MainFunction();                              // 最初の SYNC
    CanTSyn_TxConfirmation(CANTSYN_TX_PDU_ID, E_OK);     // FUP
    CanTSyn_TxConfirmation(CANTSYN_TX_PDU_ID, E_OK);     // SYNC/FUP の1組が完了して待機へ戻る
    ASSERT_EQ(CallCount_CanIf_Transmit, 2U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    FakeMillis_Value += CANTSYN_TX_PERIOD_MS - 1UL;
    CanTSyn_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_CanIf_Transmit, 2U);   // 周期に満たないので SYNC は送らない
}

TEST_F(CanTSyn_MainFunction_Test, CanTSyn_MainFunction_NG_ConfirmationTimeoutThenResendNextPeriod)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanTSyn_MainFunction();   // SYNC を送る（送信完了は来ない）
    ASSERT_EQ(CallCount_CanIf_Transmit, 1U);
    FakeMillis_Value += CANTSYN_MASTER_CONFIRMATION_TIMEOUT_MS;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTSyn_MainFunction();                              // タイムアウト → 待機へ戻る
    CanTSyn_TxConfirmation(CANTSYN_TX_PDU_ID, E_OK);     // 遅れて届いた通知は無視される
    FakeMillis_Value += CANTSYN_TX_PERIOD_MS;
    CanTSyn_MainFunction();                              // 次の周期で SYNC からやり直す

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_CanIf_Transmit, 2U);   // FUP は送られず、新しい SYNC だけが増える
    EXPECT_EQ(LastData_CanIf_Transmit[0], CANTSYN_TX_TYPE_SYNC);
    EXPECT_EQ(LastData_CanIf_Transmit[2], 0x01U);   // SC は進んでいる
}

TEST_F(CanTSyn_MainFunction_Test, CanTSyn_MainFunction_NG_TransmitRejectedRetriesNextPeriod)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ForcedReturn_CanIf_Transmit = E_NOT_OK;   // CanIf が SYNC の送信を拒否する

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTSyn_MainFunction();                   // 拒否される（待機のまま）
    ForcedReturn_CanIf_Transmit = E_OK;
    FakeMillis_Value += CANTSYN_TX_PERIOD_MS;
    CanTSyn_MainFunction();                   // 次の周期でやり直す

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_CanIf_Transmit, 2U);
    EXPECT_EQ(LastData_CanIf_Transmit[2], 0x01U);   // 拒否された送信要求でも SC は進む
}

TEST_F(CanTSyn_MainFunction_Test, CanTSyn_MainFunction_OK_SendsSyncAfterPeriod)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanTSyn_MainFunction();                              // 最初の SYNC
    CanTSyn_TxConfirmation(CANTSYN_TX_PDU_ID, E_OK);     // FUP
    CanTSyn_TxConfirmation(CANTSYN_TX_PDU_ID, E_OK);     // SYNC/FUP の1組が完了して待機へ戻る
    FakeMillis_Value += CANTSYN_TX_PERIOD_MS;            // 12345ms + 1000ms = 13345ms

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTSyn_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_CanIf_Transmit, 3U);
    EXPECT_EQ(LastPduId_CanIf_Transmit, CANTSYN_CANIF_TX_PDU_ID);
    EXPECT_EQ(LastLength_CanIf_Transmit, CANTSYN_DLC);
    EXPECT_EQ(LastData_CanIf_Transmit[0], CANTSYN_TX_TYPE_SYNC);
    EXPECT_EQ(LastData_CanIf_Transmit[2], 0x01U);   // 時間領域 0、SC=1
    EXPECT_EQ(LastData_CanIf_Transmit[4], 0U);      // 秒 13（ビッグエンディアン）
    EXPECT_EQ(LastData_CanIf_Transmit[5], 0U);
    EXPECT_EQ(LastData_CanIf_Transmit[6], 0U);
    EXPECT_EQ(LastData_CanIf_Transmit[7], 13U);
#if (CANTSYN_TX_CRC_SECURED != 0U)
    // CRC-8H2F（Byte2〜Byte7 = 01 00 00 00 00 0D と、SYNC の DataID(SC=1) = (0x240+1)&0xFF = 0x41）= 0xEE
    EXPECT_EQ(LastData_CanIf_Transmit[1], 0xEEU);
#else
    EXPECT_EQ(LastData_CanIf_Transmit[1], 0x00U);   // CRC なしでは User Byte 1（既定値 0）
#endif
}

#if (CANTSYN_TX_CRC_SECURED != 0U)
TEST_F(CanTSyn_MainFunction_Test, CanTSyn_MainFunction_OK_CrcMatchesReferenceForAllSequenceCounters)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    const uint8 check[9] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
    ASSERT_EQ(ReferenceCrc8H2F(check, 9U), 0xDFU);   // 基準の別実装を、既知の検査値で確かめる

    // 直近に送られたフレームの Byte2〜Byte7 に DataID を続けた CRC の期待値（基準の別実装で計算）
    auto expectedCrc = [](uint8 dataId) {
        uint8 input[7];
        for (uint8 i = 0U; i < 6U; i++)
        {
            input[i] = LastData_CanIf_Transmit[2U + i];
        }
        input[6] = dataId;
        return ReferenceCrc8H2F(input, 7U);
    };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    // SC=0〜15 の SYNC と FUP を順に送らせる（SC が 1 周して 0 へ戻るところまで）。
    for (uint8 sc = 0U; sc < 17U; sc++)
    {
        FakeMillis_Value += CANTSYN_TX_PERIOD_MS;
        CanTSyn_MainFunction();                              // SYNC
        const uint8 expectedSc = static_cast<uint8>(sc & 0x0FU);

        /* ----------------------- */
        /* ---- 評価 (Assert) ---- */
        /* ----------------------- */
        EXPECT_EQ(LastData_CanIf_Transmit[2] & 0x0FU, expectedSc);
        EXPECT_EQ(LastData_CanIf_Transmit[1], expectedCrc(static_cast<uint8>((0x240U + expectedSc) & 0xFFU)))   // SYNC の DataID
            << "SYNC sc=" << static_cast<int>(expectedSc);

        CanTSyn_TxConfirmation(CANTSYN_TX_PDU_ID, E_OK);     // FUP
        EXPECT_EQ(LastData_CanIf_Transmit[0], CANTSYN_TX_TYPE_FUP);
        EXPECT_EQ(LastData_CanIf_Transmit[1], expectedCrc(static_cast<uint8>((0x240U + 0x10U + expectedSc) & 0xFFU)))   // FUP の DataID
            << "FUP sc=" << static_cast<int>(expectedSc);

        CanTSyn_TxConfirmation(CANTSYN_TX_PDU_ID, E_OK);     // FUP の送信完了（1 組が完了）
    }
}
#endif

}  // namespace
