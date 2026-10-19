/**
 * \file    CanTSyn_SetTransmissionMode_test.cpp
 * \brief   CanTSyn_SetTransmissionMode() の単体テスト
 * \details CtrlIdx が 0 以外なら CANTSYN_E_INV_CTRL_IDX、Mode が範囲外なら CANTSYN_E_PARAM を DET へ報告して設定を変えない。
 *          CANTSYN_TX_OFF で SYNC の周期送信を止め（途中の SYNC/FUP シーケンスも破棄する）、CANTSYN_TX_ON で再開する。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "CanTSyn.h"
#include "CanTSyn_Cfg.h"
#include "StbM.h"
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

/* CanTSyn_SetTransmissionMode() の単体テスト用フィクスチャ。
 * SetUp(): 模擬時刻・wrap・DET 記録を初期化し、CanIf_Transmit() を成功固定にして CanTSyn_Init(NULL) を呼ぶ。
 * TearDown(): wrap を戻す。 */
class CanTSyn_SetTransmissionMode_Test : public ::testing::Test
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
        StbM_Init(NULL);                         // 時刻（マスターの時間ベース）を提供する
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

TEST_F(CanTSyn_SetTransmissionMode_Test, CanTSyn_SetTransmissionMode_NG_InvalidCtrlIdx)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み、送信機能は既定でオン）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTSyn_SetTransmissionMode(1U, CANTSYN_TX_OFF);
    CanTSyn_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastModuleId, CANTSYN_MODULE_ID);
    EXPECT_EQ(FakeDetHw_LastApiId, CANTSYN_API_ID_SET_TRANSMISSION_MODE);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANTSYN_E_INV_CTRL_IDX);
    EXPECT_EQ(CallCount_CanIf_Transmit, 1U);   // 設定は変わらず、SYNC は送信される
}

TEST_F(CanTSyn_SetTransmissionMode_Test, CanTSyn_SetTransmissionMode_NG_InvalidMode)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み、送信機能は既定でオン）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTSyn_SetTransmissionMode(0U, static_cast<CanTSyn_TransmissionModeType>(2));
    CanTSyn_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastModuleId, CANTSYN_MODULE_ID);
    EXPECT_EQ(FakeDetHw_LastApiId, CANTSYN_API_ID_SET_TRANSMISSION_MODE);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANTSYN_E_PARAM);
    EXPECT_EQ(CallCount_CanIf_Transmit, 1U);   // 設定は変わらず、SYNC は送信される
}

TEST_F(CanTSyn_SetTransmissionMode_Test, CanTSyn_SetTransmissionMode_OK_TxOffStopsCyclicSync)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTSyn_SetTransmissionMode(0U, CANTSYN_TX_OFF);
    FakeMillis_Value += CANTSYN_TX_PERIOD_MS * 3UL;
    CanTSyn_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_CanIf_Transmit, 0U);   // 周期が過ぎても SYNC を送らない
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(CanTSyn_SetTransmissionMode_Test, CanTSyn_SetTransmissionMode_OK_TxOffDiscardsSequenceInProgress)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanTSyn_MainFunction();   // SYNC を送る（SYNC の送信完了待ちへ）
    ASSERT_EQ(CallCount_CanIf_Transmit, 1U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTSyn_SetTransmissionMode(0U, CANTSYN_TX_OFF);
    CanTSyn_TxConfirmation(CANTSYN_TX_PDU_ID, E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_CanIf_Transmit, 1U);   // 送信完了が来ても FUP は送らない
}

TEST_F(CanTSyn_SetTransmissionMode_Test, CanTSyn_SetTransmissionMode_OK_TxOnResumesCyclicSync)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanTSyn_SetTransmissionMode(0U, CANTSYN_TX_OFF);
    FakeMillis_Value += CANTSYN_TX_PERIOD_MS;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTSyn_SetTransmissionMode(0U, CANTSYN_TX_ON);
    CanTSyn_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_CanIf_Transmit, 1U);
    EXPECT_EQ(LastData_CanIf_Transmit[0], CANTSYN_TX_TYPE_SYNC);
}

}  // namespace
