/**
 * \file    NvM_InitDiag_NonRedundantCrcMismatch_test.cpp
 * \brief   非冗長ブロックの CRC 不一致を、NVM_INTEGRITY_FAILED（FAILED）として Dem へ報告することの単体テスト
 * \details 非冗長の DEM_STATUS ブロックの CRC を壊すと、NvM_Init() はデフォルトで復元し、最初の NvM_MainFunction() が
 *          NVM_INTEGRITY_FAILED を FAILED で報告する（[SWS_NvM_00864]）。冗長ブロックは正常のため、
 *          NVM_LOSS_OF_REDUNDANCY は PASSED（冗長性は保たれている、[SWS_NvM_00876]）。
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

/* NvM_InitDiag_NonRedundantCrcMismatch_Test のフィクスチャ。
 * SetUp(): DET と Dem の wrap を初期化し、全ブロックが有効な EEPROM を用意する。
 * TearDown(): NvM を未初期化へ戻す。 */
class NvM_InitDiag_NonRedundantCrcMismatch_Test : public ::testing::Test
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

TEST_F(NvM_InitDiag_NonRedundantCrcMismatch_Test, NvM_MainFunction_OK_NonRedundantCrcMismatchReportsIntegrityFailed)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    NvMTest_CorruptByte(static_cast<uint16>(NVM_BLOCK_DEM_STATUS_EEPROM_ADDR + NVM_BLOCK_DEM_STATUS_LENGTH));

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    NvMTest_ReinitAndReport();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(LastStatusOfEvent_Dem_SetEventStatus[DemConf_DemEventParameter_NVM_INTEGRITY_FAILED], DEM_EVENT_STATUS_FAILED);
    EXPECT_EQ(LastStatusOfEvent_Dem_SetEventStatus[DemConf_DemEventParameter_NVM_LOSS_OF_REDUNDANCY], DEM_EVENT_STATUS_PASSED);
    EXPECT_EQ(CallCount_Dem_SetEventStatus, 2U);   // 2 つのイベントを 1 回ずつ報告する
}

}  // namespace
