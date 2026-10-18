/**
 * \file    CanTSyn_MainFunction_test.cpp
 * \brief   CanTSyn_MainFunction() の単体テスト
 * \details 待機中に CANTSYN_TX_PERIOD_MS が経過していれば SYNC（Type=0x10、時間領域 0、SC、秒）を送信し、周期内は送らない。
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
    EXPECT_EQ(LastData_CanIf_Transmit[0], CANTSYN_TYPE_SYNC);
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
    EXPECT_EQ(LastData_CanIf_Transmit[0], CANTSYN_TYPE_SYNC);
    EXPECT_EQ(LastData_CanIf_Transmit[2], 0x01U);   // 時間領域 0、SC=1
    EXPECT_EQ(LastData_CanIf_Transmit[4], 0U);      // 秒 13（ビッグエンディアン）
    EXPECT_EQ(LastData_CanIf_Transmit[5], 0U);
    EXPECT_EQ(LastData_CanIf_Transmit[6], 0U);
    EXPECT_EQ(LastData_CanIf_Transmit[7], 13U);
}

}  // namespace
