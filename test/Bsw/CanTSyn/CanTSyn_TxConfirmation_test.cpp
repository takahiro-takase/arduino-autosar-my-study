/**
 * \file    CanTSyn_TxConfirmation_test.cpp
 * \brief   CanTSyn_TxConfirmation() の単体テスト
 * \details SYNC の送信完了で FUP（Type は CANTSYN_TX_TYPE_FUP、CRC 付きなら Byte1 に CRC、SC は SYNC と同じ、SYNC を渡してからの経過時間を含む T4 のナノ秒部分）を送信する（[SWS_CanTSyn_00045]）。
 *          T4 が 1 秒以上になれば超えた秒数を OVS（Byte3 の Bit1-0）へ入れる。PDU ID が不正なら CANTSYN_E_INVALID_PDUID を報告して何もしない。
 *          SYNC の送信失敗通知・FUP の送信拒否・待機中の通知では FUP を送らず待機へ戻る。FUP の送信完了で SYNC/FUP の1組が完了する。
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

/* CanTSyn_TxConfirmation() の単体テスト用フィクスチャ。
 * SetUp(): 模擬時刻（12950ms: T0 の ns 部分が 950ms になる値）・wrap・DET 記録を初期化し、CanIf_Transmit() を成功固定にして CanTSyn_Init(NULL) を呼ぶ。
 * TearDown(): wrap を戻す。 */
class CanTSyn_TxConfirmation_Test : public ::testing::Test
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
        FakeMillis_Value                 = 12950UL;
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

TEST_F(CanTSyn_TxConfirmation_Test, CanTSyn_TxConfirmation_NG_InvalidPduId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanTSyn_MainFunction();   // SYNC を送る
    ASSERT_EQ(CallCount_CanIf_Transmit, 1U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTSyn_TxConfirmation(CANTSYN_TX_PDU_ID + 1U, E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastModuleId, CANTSYN_MODULE_ID);
    EXPECT_EQ(FakeDetHw_LastApiId, CANTSYN_API_ID_TX_CONFIRMATION);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANTSYN_E_INVALID_PDUID);
    EXPECT_EQ(CallCount_CanIf_Transmit, 1U);   // FUP は送らない
}

TEST_F(CanTSyn_TxConfirmation_Test, CanTSyn_TxConfirmation_NG_SyncSendFailedNoFup)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanTSyn_MainFunction();   // SYNC を送る
    ASSERT_EQ(CallCount_CanIf_Transmit, 1U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTSyn_TxConfirmation(CANTSYN_TX_PDU_ID, E_NOT_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_CanIf_Transmit, 1U);   // SYNC が送れなかったので FUP は送らない
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(CanTSyn_TxConfirmation_Test, CanTSyn_TxConfirmation_NG_SyncConfirmationTooLateNoFup)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanTSyn_MainFunction();   // SYNC を送る
    ASSERT_EQ(CallCount_CanIf_Transmit, 1U);
    FakeMillis_Value += CANTSYN_MASTER_CONFIRMATION_TIMEOUT_MS;   // 待ち時間ちょうどで通知が届く

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTSyn_TxConfirmation(CANTSYN_TX_PDU_ID, E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_CanIf_Transmit, 1U);   // T4 が信用できないので FUP は送らない
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(CanTSyn_TxConfirmation_Test, CanTSyn_TxConfirmation_NG_FupRejectedReturnsToIdle)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanTSyn_MainFunction();                   // SYNC を送る
    ForcedReturn_CanIf_Transmit = E_NOT_OK;   // 次の CanIf_Transmit()（FUP）は拒否される

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTSyn_TxConfirmation(CANTSYN_TX_PDU_ID, E_OK);   // FUP を送ろうとして拒否される
    ForcedReturn_CanIf_Transmit = E_OK;
    CanTSyn_TxConfirmation(CANTSYN_TX_PDU_ID, E_OK);   // 待機中の通知は無視される

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_CanIf_Transmit, 2U);   // SYNC と、拒否された FUP の 2 回だけ
}

TEST_F(CanTSyn_TxConfirmation_Test, CanTSyn_TxConfirmation_NG_IdleNotificationIgnored)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() 直後は待機中）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTSyn_TxConfirmation(CANTSYN_TX_PDU_ID, E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_CanIf_Transmit, 0U);   // 待機中の通知は無視する
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(CanTSyn_TxConfirmation_Test, CanTSyn_TxConfirmation_OK_FupCarriesOverflowOfSeconds)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanTSyn_MainFunction();     // 12950ms に SYNC（T0 の ns 部分 = 950000000ns）
    FakeMillis_Value += CANTSYN_MASTER_CONFIRMATION_TIMEOUT_MS - 1UL;  // SYNC の送信完了まで 99ms かかった（待ち時間の内）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTSyn_TxConfirmation(CANTSYN_TX_PDU_ID, E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(LastData_CanIf_Transmit[0], CANTSYN_TX_TYPE_FUP);
    EXPECT_EQ(LastData_CanIf_Transmit[3], 0x01U);   // T4 = 950ms + 99ms = 1049ms → OVS=1
    // 残りのナノ秒 = 49000000ns = 0x02EBAE40（ビッグエンディアン）
    EXPECT_EQ(LastData_CanIf_Transmit[4], 0x02U);
    EXPECT_EQ(LastData_CanIf_Transmit[5], 0xEBU);
    EXPECT_EQ(LastData_CanIf_Transmit[6], 0xAEU);
    EXPECT_EQ(LastData_CanIf_Transmit[7], 0x40U);
#if (CANTSYN_TX_CRC_SECURED != 0U)
    // CRC-8H2F（Byte2〜Byte7 = 00 01 02 EB AE 40 と、FUP の DataID(SC=0) = (0x240+0x10)&0xFF = 0x50）= 0xF0
    EXPECT_EQ(LastData_CanIf_Transmit[1], 0xF0U);
#endif
}

TEST_F(CanTSyn_TxConfirmation_Test, CanTSyn_TxConfirmation_OK_FupCarriesNanosecondsAndSameSc)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanTSyn_MainFunction();    // 12950ms に SYNC（T0 の ns 部分 = 950ms = 950000000ns、SC=0）
    FakeMillis_Value += 20UL;  // SYNC の送信完了まで 20ms かかった

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTSyn_TxConfirmation(CANTSYN_TX_PDU_ID, E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_CanIf_Transmit, 2U);
    EXPECT_EQ(LastPduId_CanIf_Transmit, CANTSYN_CANIF_TX_PDU_ID);
    EXPECT_EQ(LastLength_CanIf_Transmit, CANTSYN_DLC);
    EXPECT_EQ(LastData_CanIf_Transmit[0], CANTSYN_TX_TYPE_FUP);
    EXPECT_EQ(LastData_CanIf_Transmit[2], 0x00U);   // SYNC と同じ時間領域・SC
    EXPECT_EQ(LastData_CanIf_Transmit[3], 0x00U);   // SGW=0、OVS=0
    // T4 = 950ms + 20ms = 970ms = 970000000ns = 0x39D10680（ビッグエンディアン）
    EXPECT_EQ(LastData_CanIf_Transmit[4], 0x39U);
    EXPECT_EQ(LastData_CanIf_Transmit[5], 0xD1U);
    EXPECT_EQ(LastData_CanIf_Transmit[6], 0x06U);
    EXPECT_EQ(LastData_CanIf_Transmit[7], 0x80U);
#if (CANTSYN_TX_CRC_SECURED != 0U)
    // CRC-8H2F（Byte2〜Byte7 = 00 00 39 D1 06 80 と、FUP の DataID(SC=0) = 0x50）= 0xE6
    EXPECT_EQ(LastData_CanIf_Transmit[1], 0xE6U);
#endif
}

TEST_F(CanTSyn_TxConfirmation_Test, CanTSyn_TxConfirmation_OK_FupConfirmationCompletesSequence)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanTSyn_MainFunction();                            // SYNC
    CanTSyn_TxConfirmation(CANTSYN_TX_PDU_ID, E_OK);   // FUP を送る
    ASSERT_EQ(CallCount_CanIf_Transmit, 2U);
    FakeMillis_Value += CANTSYN_TX_PERIOD_MS;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTSyn_TxConfirmation(CANTSYN_TX_PDU_ID, E_OK);   // FUP の送信完了 → 待機へ戻る
    CanTSyn_MainFunction();                            // 次の SYNC を送れる

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_CanIf_Transmit, 3U);
    EXPECT_EQ(LastData_CanIf_Transmit[0], CANTSYN_TX_TYPE_SYNC);
    EXPECT_EQ(LastData_CanIf_Transmit[2], 0x01U);   // 次の SYNC は SC=1
}

}  // namespace
