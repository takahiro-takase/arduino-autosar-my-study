/**
 * \file    Bsw_Nm_GetState_test.cpp
 * \brief   Nm_GetState() の単体テスト
 * \details CanNm の状態とモードを、Nm の型（Nm_StateType / Nm_ModeType）へ変換して返す。CanNm_Init() 直後は Bus-Sleep Mode で、
 *          状態は NM_STATE_BUS_SLEEP、モードは NM_MODE_BUS_SLEEP になる。ポインタのどちらかが NULL なら、その値は書かない。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Nm.h"
#include "Nm_Cfg.h"
#include "CanNm.h"
#include "CanNm_Cfg.h"
#include "Fake_Det_Hw.h"
}

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

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

/* Nm_GetState() の単体テスト用フィクスチャ。
 * SetUp(): DET の記録を初期化し、CanNm_Init(NULL)・Nm_Init(NULL) を呼ぶ（Init 自体の記録は消す）。
 * TearDown(): Nm・CanNm を未初期化へ戻す。 */
class Bsw_Nm_GetState_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;
        FakeDetHw_Reset();
        CanNm_Init(NULL);
        Nm_Init(NULL);
        FakeDetHw_Reset();
        FakeDetHw_LogSuppressed = 0U;
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Nm_Test_ResetInitState();
        CanNm_DeInit();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Bsw_Nm_GetState_Test, Nm_GetState_OK_MapsBusSleepStateAndMode)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Nm_StateType state = NM_STATE_NORMAL_OPERATION;
    Nm_ModeType mode = NM_MODE_NETWORK;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Nm_GetState(NM_MAIN_NETWORK_HANDLE, &state, &mode);
    const Std_ReturnType retNoState = Nm_GetState(NM_MAIN_NETWORK_HANDLE, NULL, &mode);
    const Std_ReturnType retNoMode = Nm_GetState(NM_MAIN_NETWORK_HANDLE, &state, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(state, NM_STATE_BUS_SLEEP);
    EXPECT_EQ(mode, NM_MODE_BUS_SLEEP);
    EXPECT_EQ(retNoState, E_OK);   // NULL の側は書かない
    EXPECT_EQ(retNoMode, E_OK);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

}  // namespace
