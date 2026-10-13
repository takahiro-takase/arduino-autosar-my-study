/**
 * \file    NvM_InitDiag_AllValid_test.cpp
 * \brief   全ブロックが正常なとき、NVM_INTEGRITY_FAILED と NVM_LOSS_OF_REDUNDANCY の両方を PASSED で報告することの単体テスト
 * \details EEPROM を壊さないまま NvM_Init() をやり直すと、最初の NvM_MainFunction() が NVM_INTEGRITY_FAILED を
 *          PASSED（[SWS_NvM_00872]）、NVM_LOSS_OF_REDUNDANCY を PASSED（[SWS_NvM_00876]）で報告する。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "NvM_TestUtil.h"
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

/* NvM_InitDiag_AllValid_Test のフィクスチャ。
 * SetUp(): DET と Dem の wrap を初期化し、全ブロックが有効な EEPROM を用意する。
 * TearDown(): NvM を未初期化へ戻す。 */
class NvM_InitDiag_AllValid_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;
        WrapDem_Reset();
        FakeDetHw_Reset();
        NvMTest_PrepareValidEeprom();
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        NvM_Test_ResetInitState();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(NvM_InitDiag_AllValid_Test, NvM_MainFunction_OK_AllValidReportsBothPassed)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // 壊さない（SetUp() で用意した、全ブロックが有効な EEPROM のまま）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    NvMTest_ReinitAndReport();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(LastStatusOfEvent_Dem_SetEventStatus[DemConf_DemEventParameter_NVM_INTEGRITY_FAILED], DEM_EVENT_STATUS_PASSED);
    EXPECT_EQ(LastStatusOfEvent_Dem_SetEventStatus[DemConf_DemEventParameter_NVM_LOSS_OF_REDUNDANCY], DEM_EVENT_STATUS_PASSED);
    EXPECT_EQ(CallCount_Dem_SetEventStatus, 2U);
}

}  // namespace
