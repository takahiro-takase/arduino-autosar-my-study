/**
 * \file    Dem_GetDebouncingOfEvent_test.cpp
 * \brief   Dem_GetDebouncingOfEvent() の単体テスト
 * \details 内部のデバウンスカウンタから、デバウンスの状態のビット（Temporarily Defective / Finally Defective / Temporarily Healed / Test Complete / DTR Update）を導く。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Dem.h"
#include "Dem_Cfg.h"
#include "Fake_Det_Hw.h"
#include "Wrap_Dem.h"
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

/* Dem_GetDebouncingOfEvent() の単体テスト用フィクスチャ。
 * SetUp(): wrap を初期化し、Dem_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 *          kMulti は、確定に複数回の報告が要るイベント（DemConf_DemEventParameter_CAN_BUSOFF）、kSingle は、1 回で確定するイベント。
 * TearDown(): 未初期化の状態へ戻す。 */
class Dem_GetDebouncingOfEvent_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        WrapDem_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        Dem_Init(NULL);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Dem_Test_ResetInitState();
    }

    static const Dem_EventIdType kMulti  = DemConf_DemEventParameter_CAN_BUSOFF;
    static const Dem_EventIdType kSingle = DemConf_DemEventParameter_BUTTON_STUCK;
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Dem_GetDebouncingOfEvent_Test, Dem_GetDebouncingOfEvent_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dem_Test_ResetInitState();
    Dem_DebouncingStateType state = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetDebouncingOfEvent(kMulti, &state);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_UNINIT);
}

TEST_F(Dem_GetDebouncingOfEvent_Test, Dem_GetDebouncingOfEvent_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetDebouncingOfEvent(kMulti, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_PARAM_POINTER);
}

TEST_F(Dem_GetDebouncingOfEvent_Test, Dem_GetDebouncingOfEvent_NG_InvalidEventId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dem_DebouncingStateType state = 0xA5U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetDebouncingOfEvent(DEM_EVENT_COUNT, &state);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_WRONG_CONFIGURATION);
    EXPECT_EQ(state, 0xA5U);   // 出力は、書き換えない
}

// 中立（FDC = 0）: どのビットも立てない
TEST_F(Dem_GetDebouncingOfEvent_Test, Dem_GetDebouncingOfEvent_OK_NeutralHasNoBits)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（Init 直後は、カウンタが 0）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dem_DebouncingStateType state = 0xFFU;
    const Std_ReturnType ret = Dem_GetDebouncingOfEvent(kMulti, &state);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(state, static_cast<Dem_DebouncingStateType>(0U));
}

// 失敗側へ進行中（0 < FDC < 127）: Temporarily Defective のみ
TEST_F(Dem_GetDebouncingOfEvent_Test, Dem_GetDebouncingOfEvent_OK_PrefailedIsTemporarilyDefective)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    static_assert(DEM_DEBOUNCE_LIMIT_CAN_BUSOFF > 1, "確定に複数回の報告が要るイベントが前提");
    ASSERT_EQ(Dem_SetEventStatus(kMulti, DEM_EVENT_STATUS_PREFAILED), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dem_DebouncingStateType state = 0xFFU;
    const Std_ReturnType ret = Dem_GetDebouncingOfEvent(kMulti, &state);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(state, static_cast<Dem_DebouncingStateType>(DEM_TEMPORARILY_DEFECTIVE));
}

// 失敗が確定（FDC = 127）: Finally Defective・Test Complete・DTR Update
TEST_F(Dem_GetDebouncingOfEvent_Test, Dem_GetDebouncingOfEvent_OK_FailedIsFinallyDefectiveAndComplete)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SetEventStatus(kMulti, DEM_EVENT_STATUS_FAILED), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dem_DebouncingStateType state = 0xFFU;
    const Std_ReturnType ret = Dem_GetDebouncingOfEvent(kMulti, &state);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(state, static_cast<Dem_DebouncingStateType>(DEM_FINALLY_DEFECTIVE | DEM_TEST_COMPLETE | DEM_DTR_UPDATE));
}

// 成功側へ進行中（-128 < FDC < 0）: Temporarily Healed のみ
TEST_F(Dem_GetDebouncingOfEvent_Test, Dem_GetDebouncingOfEvent_OK_PrepassedIsTemporarilyHealed)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SetEventStatus(kMulti, DEM_EVENT_STATUS_PREPASSED), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dem_DebouncingStateType state = 0xFFU;
    const Std_ReturnType ret = Dem_GetDebouncingOfEvent(kMulti, &state);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(state, static_cast<Dem_DebouncingStateType>(DEM_TEMPORARILY_HEALED));
}

// 成功が確定（FDC = -128）: Test Complete・DTR Update（Defective のビットは立たない）
TEST_F(Dem_GetDebouncingOfEvent_Test, Dem_GetDebouncingOfEvent_OK_PassedIsCompleteWithoutDefective)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SetEventStatus(kMulti, DEM_EVENT_STATUS_PASSED), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dem_DebouncingStateType state = 0xFFU;
    const Std_ReturnType ret = Dem_GetDebouncingOfEvent(kMulti, &state);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(state, static_cast<Dem_DebouncingStateType>(DEM_TEST_COMPLETE | DEM_DTR_UPDATE));
}

// 確定が 1 回で済むイベントの成功側も同じ: PREPASSED で、すぐ確定する（Temporarily Healed の段階を経ない）
TEST_F(Dem_GetDebouncingOfEvent_Test, Dem_GetDebouncingOfEvent_OK_SingleStepEventIsCompleteAtOnceOnPassedSide)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    static_assert(DEM_DEBOUNCE_LIMIT_BUTTON_STUCK == 1, "1 回で確定するイベントが前提");
    ASSERT_EQ(Dem_SetEventStatus(kSingle, DEM_EVENT_STATUS_PREPASSED), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dem_DebouncingStateType state = 0xFFU;
    const Std_ReturnType ret = Dem_GetDebouncingOfEvent(kSingle, &state);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(state, static_cast<Dem_DebouncingStateType>(DEM_TEST_COMPLETE | DEM_DTR_UPDATE));
}

// 確定が 1 回で済むイベントは、PREFAILED で、すぐ確定する（Temporarily Defective の段階を経ない）
TEST_F(Dem_GetDebouncingOfEvent_Test, Dem_GetDebouncingOfEvent_OK_SingleStepEventIsFinallyDefectiveAtOnce)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    static_assert(DEM_DEBOUNCE_LIMIT_BUTTON_STUCK == 1, "1 回で確定するイベントが前提");
    ASSERT_EQ(Dem_SetEventStatus(kSingle, DEM_EVENT_STATUS_PREFAILED), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dem_DebouncingStateType state = 0xFFU;
    const Std_ReturnType ret = Dem_GetDebouncingOfEvent(kSingle, &state);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(state, static_cast<Dem_DebouncingStateType>(DEM_FINALLY_DEFECTIVE | DEM_TEST_COMPLETE | DEM_DTR_UPDATE));
}

}  // namespace
