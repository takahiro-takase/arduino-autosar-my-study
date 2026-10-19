/**
 * \file    CanTSyn_Init_test.cpp
 * \brief   CanTSyn_Init() の単体テスト
 * \details 初期化後の最初の CanTSyn_MainFunction() で SYNC を送ること、送信機能が既定でオンであること、
 *          再初期化（[SWS_CanTSyn_00006]）で SC（シーケンスカウンタ）が 0 へ戻ることを確認する。
 *          CanIf_Transmit() は wrap で成功を強制し、渡されたフレームの中身を検証する。
 *          未初期化での API 呼び出しの DET 報告（CANTSYN_E_UNINIT）は、CanTSyn に DeInit が無く
 *          他のテストが先に初期化済みにするため、本テスト群からは検証できない（docs/unit_test.md 参照）。
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

/* CanTSyn_Init() の単体テスト用フィクスチャ。
 * SetUp(): 模擬時刻・wrap・DET 記録を初期化し、CanIf_Transmit() を成功固定にする（CanTSyn_Init() は各テストが呼ぶ）。
 * TearDown(): wrap を戻す。 */
class CanTSyn_Init_Test : public ::testing::Test
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
        StbM_Init(NULL);
    }

    void TearDown() override
    {
        WrapCanIf_Reset();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(CanTSyn_Init_Test, CanTSyn_Init_OK_FirstMainFunctionSendsSync)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanTSyn_Init(NULL);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTSyn_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_CanIf_Transmit, 1U);   // 初回の呼び出しで SYNC を送る
    EXPECT_EQ(LastPduId_CanIf_Transmit, CANTSYN_CANIF_TX_PDU_ID);
    EXPECT_EQ(LastLength_CanIf_Transmit, CANTSYN_DLC);
    EXPECT_EQ(LastData_CanIf_Transmit[0], CANTSYN_TX_TYPE_SYNC);
    EXPECT_EQ(LastData_CanIf_Transmit[2], 0x00U);   // 時間領域 0、SC=0（[SWS_CanTSyn_00007]）
    EXPECT_EQ(LastData_CanIf_Transmit[7], 12U);     // 12345ms の秒部分 = 12（ビッグエンディアン）
#if (CANTSYN_TX_CRC_SECURED != 0U)
    EXPECT_EQ(LastData_CanIf_Transmit[0], 0x20U);   // CRC 付きの SYNC
    // CRC-8H2F（Byte2〜Byte7 = 00 00 00 00 00 0C と、SYNC の DataID(SC=0) = 0x40）= 0xDC
    EXPECT_EQ(LastData_CanIf_Transmit[1], 0xDCU);
#else
    EXPECT_EQ(LastData_CanIf_Transmit[0], 0x10U);   // CRC なしの SYNC
#endif
}

TEST_F(CanTSyn_Init_Test, CanTSyn_Init_OK_ReinitializeResetsSequenceCounter)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanTSyn_Init(NULL);
    CanTSyn_MainFunction();                       // SYNC (SC=0)
    CanTSyn_TxConfirmation(CANTSYN_TX_PDU_ID, E_OK);  // FUP
    CanTSyn_TxConfirmation(CANTSYN_TX_PDU_ID, E_OK);  // 1 組が完了して待機へ戻る
    FakeMillis_Value += CANTSYN_TX_PERIOD_MS;
    CanTSyn_MainFunction();                       // SYNC (SC=1)
    ASSERT_EQ(LastData_CanIf_Transmit[2], 0x01U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTSyn_Init(NULL);                           // 初期化済みでの再初期化（[SWS_CanTSyn_00006]）
    WrapCanIf_Reset();
    FailFromCallCount_CanIf_Transmit = 1U;
    ForcedReturn_CanIf_Transmit      = E_OK;
    CanTSyn_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(LastData_CanIf_Transmit[0], CANTSYN_TX_TYPE_SYNC);
    EXPECT_EQ(LastData_CanIf_Transmit[2], 0x00U);  // SC が 0 へ戻る
}

}  // namespace
