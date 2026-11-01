/**
 * \file    Dem_GetOperationCycleState_test.cpp
 * \brief   Dem_GetOperationCycleState() の単体テスト
 * \details Dem_Init() が開始した電源サイクルの状態（START）と、Dem_SetOperationCycleState() で変えた状態（END）を取得する。
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

/* Dem_GetOperationCycleState() の単体テスト用フィクスチャ。
 * SetUp(): wrap を初期化し、Dem_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): 未初期化の状態へ戻す。 */
class Dem_GetOperationCycleState_Test : public ::testing::Test
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
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Dem_GetOperationCycleState_Test, Dem_GetOperationCycleState_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dem_Test_ResetInitState();
    Dem_OperationCycleStateType state = DEM_CYCLE_STATE_END;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetOperationCycleState(DemConf_DemOperationCycle_POWER, &state);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_UNINIT);
}

TEST_F(Dem_GetOperationCycleState_Test, Dem_GetOperationCycleState_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetOperationCycleState(DemConf_DemOperationCycle_POWER, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_PARAM_POINTER);
}

TEST_F(Dem_GetOperationCycleState_Test, Dem_GetOperationCycleState_NG_UnknownCycle)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dem_OperationCycleStateType state = DEM_CYCLE_STATE_END;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetOperationCycleState(DEM_OPERATION_CYCLE_COUNT, &state);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_WRONG_CONFIGURATION);
    EXPECT_EQ(state, DEM_CYCLE_STATE_END);   // 出力は、書き換えない
}

// Dem_Init() が、自動開始（DemOperationCycleAutostart）の電源サイクルを開始する（[SWS_Dem_00853]）
TEST_F(Dem_GetOperationCycleState_Test, Dem_GetOperationCycleState_OK_StartedAfterInit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dem_OperationCycleStateType state = DEM_CYCLE_STATE_END;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetOperationCycleState(DemConf_DemOperationCycle_POWER, &state);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(state, DEM_CYCLE_STATE_START);
}

TEST_F(Dem_GetOperationCycleState_Test, Dem_GetOperationCycleState_OK_EndedAfterEnd)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SetOperationCycleState(DemConf_DemOperationCycle_POWER, DEM_CYCLE_STATE_END), E_OK);
    Dem_OperationCycleStateType state = DEM_CYCLE_STATE_START;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetOperationCycleState(DemConf_DemOperationCycle_POWER, &state);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(state, DEM_CYCLE_STATE_END);
}

// Dem_Init() を呼び直すと、終了していても、再び開始済みになる（次の起動 = 新しい電源サイクル）
TEST_F(Dem_GetOperationCycleState_Test, Dem_GetOperationCycleState_OK_ReInitStartsCycleAgain)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SetOperationCycleState(DemConf_DemOperationCycle_POWER, DEM_CYCLE_STATE_END), E_OK);
    FakeDetHw_LogSuppressed = 1U;
    Dem_Init(NULL);
    FakeDetHw_LogSuppressed = 0U;
    Dem_OperationCycleStateType state = DEM_CYCLE_STATE_END;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetOperationCycleState(DemConf_DemOperationCycle_POWER, &state);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(state, DEM_CYCLE_STATE_START);
}

}  // namespace
