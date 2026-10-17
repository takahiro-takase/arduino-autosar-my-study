/**
 * \file    CanNm_RxIndication_test.cpp
 * \brief   CanNm_RxIndication() の単体テスト
 * \details 2026-10-16、モジュールの単体テストは関数ごとにファイルを分ける方針に改め、旧 CanNm_test.cpp（1 ファイルに 3 つのフィクスチャを統合していた）を関数単位へ分割した。
 *          未初期化は CANNM_E_UNINIT、NULL の PduInfoPtr は CANNM_E_PARAM_POINTER を報告する。
 *          受信による ComM_Nm_NetworkStartIndication() へのカスケード（[SWS_CanNm_00127]）を避けるため、ComM は明示的に未初期化にする。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "ComM.h"
#include "CanNm.h"
#include "CanNm_Cfg.h"
#include "Nm.h"
#include "Fake_Millis.h"
#include "Fake_Det_Hw.h"
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

/* CanNm_RxIndication() の単体テスト用フィクスチャ。
 * SetUp(): ComM_DeInit()（RxIndication のカスケードを防ぐ）の後、CanNm_Init(NULL) と Nm_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): CanNm_DeInit() で未初期化へ戻す。 */
class CanNm_RxIndication_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        /* CanNm_RxIndication() -> ComM_Nm_NetworkStartIndication() の呼び出し
         * （[SWS_CanNm_00127]）は ComM が初期化済みだとカスケードする。
         * 本フィクスチャは意図的に ComM_Init() を呼ばない軽量構成のため、他の
         * テストファイルが ComM を初期化したまま残す可能性
         * （feedback_native_chain_shared_static_hang 参照）を排除するべく、
         * 明示的に ComM_DeInit() で未初期化状態を保証する
         * （未初期化なら DET 報告のみで無害に即 return）。 */
        ComM_DeInit();
        CanNm_Init(NULL);
        Nm_Init(NULL);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        CanNm_DeInit();
    }

    static const NetworkHandleType kInvalidChannel = CANNM_MAIN_NETWORK_HANDLE + 1U;
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(CanNm_RxIndication_Test, CanNm_RxIndication_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanNm_DeInit();
    uint8 pdu[2] = { 0x00U, 0x2AU };
    PduInfoType info = { pdu, 2U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanNm_RxIndication(0U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_UNINIT);
}

TEST_F(CanNm_RxIndication_Test, CanNm_RxIndication_NG_NullSduDataPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PduInfoType info = { NULL, 0U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanNm_RxIndication(0U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_PARAM_POINTER);
}

TEST_F(CanNm_RxIndication_Test, CanNm_RxIndication_NG_NullPduInfoPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanNm_RxIndication(0U, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_PARAM_POINTER);
}

TEST_F(CanNm_RxIndication_Test, CanNm_RxIndication_OK_RepeatMessageBitInNormalOperationReAnnounces)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanNm_Test_SetState(CANNM_STATE_NORMAL_OPERATION);
    uint8 pdu[2] = { CANNM_CBV_BIT_REPEAT_MESSAGE_REQUEST, 0x2AU };  // 他ノードの Repeat Message Request Bit（[SWS_CanNm_00119]）
    PduInfoType info = { pdu, 2U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanNm_RxIndication(0U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    CanNm_StateType state;
    CanNm_ModeType  mode;
    ASSERT_EQ(CanNm_GetState(CANNM_MAIN_NETWORK_HANDLE, &state, &mode), E_OK);
    EXPECT_EQ(state, CANNM_STATE_REPEAT_MESSAGE);  // 自ノードも Repeat Message State へ入り直す
    EXPECT_GT(FakeDetHw_ReportCount, 0U);  // 入り直しで Nm_NetworkMode() が呼ばれ、未初期化の ComM が DET を報告する
}

TEST_F(CanNm_RxIndication_Test, CanNm_RxIndication_OK_RepeatMessageBitInReadySleepReAnnounces)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanNm_Test_SetState(CANNM_STATE_READY_SLEEP);
    uint8 pdu[2] = { CANNM_CBV_BIT_REPEAT_MESSAGE_REQUEST, 0x2AU };  // 他ノードの Repeat Message Request Bit（[SWS_CanNm_00111]）
    PduInfoType info = { pdu, 2U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanNm_RxIndication(0U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    CanNm_StateType state;
    CanNm_ModeType  mode;
    ASSERT_EQ(CanNm_GetState(CANNM_MAIN_NETWORK_HANDLE, &state, &mode), E_OK);
    EXPECT_EQ(state, CANNM_STATE_REPEAT_MESSAGE);
    EXPECT_GT(FakeDetHw_ReportCount, 0U);
}

TEST_F(CanNm_RxIndication_Test, CanNm_RxIndication_OK_RepeatMessageBitInRepeatMessageDoesNotReAnnounce)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanNm_Test_SetState(CANNM_STATE_REPEAT_MESSAGE);
    uint8 pdu[2] = { CANNM_CBV_BIT_REPEAT_MESSAGE_REQUEST, 0x2AU };
    PduInfoType info = { pdu, 2U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanNm_RxIndication(0U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    CanNm_StateType state;
    CanNm_ModeType  mode;
    ASSERT_EQ(CanNm_GetState(CANNM_MAIN_NETWORK_HANDLE, &state, &mode), E_OK);
    EXPECT_EQ(state, CANNM_STATE_REPEAT_MESSAGE);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);  // すでに Repeat Message State のため入り直さない（Nm_NetworkMode() を呼ばない）
}

TEST_F(CanNm_RxIndication_Test, CanNm_RxIndication_OK_WithoutRepeatMessageBitDoesNotReAnnounce)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanNm_Test_SetState(CANNM_STATE_NORMAL_OPERATION);
    uint8 pdu[2] = { 0x00U, 0x2AU };  // Repeat Message Request Bit なし
    PduInfoType info = { pdu, 2U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanNm_RxIndication(0U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    CanNm_StateType state;
    CanNm_ModeType  mode;
    ASSERT_EQ(CanNm_GetState(CANNM_MAIN_NETWORK_HANDLE, &state, &mode), E_OK);
    EXPECT_EQ(state, CANNM_STATE_NORMAL_OPERATION);  // 状態は変わらない
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

}  // namespace
