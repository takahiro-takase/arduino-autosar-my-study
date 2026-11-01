/**
 * \file    CanNm_PassiveStartUp_test.cpp
 * \brief   CanNm_PassiveStartUp() の単体テスト
 * \details [SWS_CanNm_00211]/[SWS_CanNm_00128]/[SWS_CanNm_00147]。Bus-Sleep Mode / Prepare Bus-Sleep Mode から、
 *          ネットワークを要求せずに Repeat Message State へ入る。Repeat Message Time が満了すると、
 *          Normal Operation State ではなく Ready Sleep State へ進む（CanNm_NetworkRequest() との違い）。
 *          Repeat Message State への進入は ComM_Nm_NetworkMode() へカスケードするが、本ファイルは
 *          CanNm.c 単体の検証が目的で ComM_Init() を呼ばない軽量フィクスチャ（CanNm_NetworkRequest_test.cpp と同じ構成）。
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

/* CanNm_PassiveStartUp() の単体テスト用フィクスチャ。
 * SetUp(): ComM_DeInit()（カスケードを防ぐ）の後、CanNm_Init(NULL) と Nm_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): CanNm_DeInit() で未初期化へ戻す。 */
class CanNm_PassiveStartUp_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        ComM_DeInit();                 // 他のテストファイルの残留で、ComM へカスケードしないよう未初期化にする
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

    static CanNm_StateType CurrentState()
    {
        CanNm_StateType state = CANNM_STATE_BUS_SLEEP;
        CanNm_ModeType  mode;
        (void)CanNm_GetState(CANNM_MAIN_NETWORK_HANDLE, &state, &mode);
        return state;
    }

    static const NetworkHandleType kInvalidChannel = CANNM_MAIN_NETWORK_HANDLE + 1U;
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(CanNm_PassiveStartUp_Test, CanNm_PassiveStartUp_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanNm_DeInit();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CanNm_PassiveStartUp(CANNM_MAIN_NETWORK_HANDLE);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_UNINIT);
}

TEST_F(CanNm_PassiveStartUp_Test, CanNm_PassiveStartUp_NG_InvalidChannel)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CanNm_PassiveStartUp(kInvalidChannel);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_INVALID_CHANNEL);
}

TEST_F(CanNm_PassiveStartUp_Test, CanNm_PassiveStartUp_NG_AlreadyInNetworkModeIsNotExecuted)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(CanNm_NetworkRequest(CANNM_MAIN_NETWORK_HANDLE), E_OK);   // Repeat Message State（Network Mode）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CanNm_PassiveStartUp(CANNM_MAIN_NETWORK_HANDLE);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);                                            // [SWS_CanNm_00147]
    EXPECT_EQ(CurrentState(), CANNM_STATE_REPEAT_MESSAGE);
}

TEST_F(CanNm_PassiveStartUp_Test, CanNm_PassiveStartUp_OK_BusSleepEntersRepeatMessage)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(CurrentState(), CANNM_STATE_BUS_SLEEP);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CanNm_PassiveStartUp(CANNM_MAIN_NETWORK_HANDLE);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(CurrentState(), CANNM_STATE_REPEAT_MESSAGE);   // [SWS_CanNm_00128]
}

TEST_F(CanNm_PassiveStartUp_Test, CanNm_PassiveStartUp_OK_PrepareBusSleepEntersRepeatMessage)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanNm_Test_SetState(CANNM_STATE_PREPARE_BUS_SLEEP);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CanNm_PassiveStartUp(CANNM_MAIN_NETWORK_HANDLE);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(CurrentState(), CANNM_STATE_REPEAT_MESSAGE);
}

TEST_F(CanNm_PassiveStartUp_Test, CanNm_PassiveStartUp_OK_WithoutRequestGoesToReadySleepAfterRepeatMessageTime)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(CanNm_PassiveStartUp(CANNM_MAIN_NETWORK_HANDLE), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    FakeMillis_Value += CANNM_REPEAT_MESSAGE_MS + 1UL;
    CanNm_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    // ネットワークを要求していないので、Normal Operation State ではなく Ready Sleep State へ進む
    EXPECT_EQ(CurrentState(), CANNM_STATE_READY_SLEEP);
}

}  // namespace
