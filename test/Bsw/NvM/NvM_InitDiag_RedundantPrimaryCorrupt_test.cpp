/**
 * \file    NvM_InitDiag_RedundantPrimaryCorrupt_test.cpp
 * \brief   冗長ブロックのプライマリ面の破損を、NVM_LOSS_OF_REDUNDANCY（FAILED）として Dem へ報告することの単体テスト
 * \details 冗長ブロック（DEM_EXTENDED）のプライマリ面の CRC だけを壊すと、ミラー面から復旧して有効なデータを得る。
 *          最初の NvM_MainFunction() が NVM_LOSS_OF_REDUNDANCY を FAILED で報告する（[SWS_NvM_00868]）。
 *          有効なデータは得られたため、NVM_INTEGRITY_FAILED は PASSED（[SWS_NvM_00872]）。
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

/* NvM_InitDiag_RedundantPrimaryCorrupt_Test のフィクスチャ。
 * SetUp(): DET と Dem の wrap を初期化し、全ブロックが有効な EEPROM を用意する。
 * TearDown(): NvM を未初期化へ戻す。 */
class NvM_InitDiag_RedundantPrimaryCorrupt_Test : public ::testing::Test
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

TEST_F(NvM_InitDiag_RedundantPrimaryCorrupt_Test, NvM_MainFunction_OK_PrimaryCorruptReportsLossOfRedundancy)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    NvMTest_CorruptByte(static_cast<uint16>(NVM_BLOCK_DEM_EXTENDED_EEPROM_ADDR + NVM_BLOCK_DEM_EXTENDED_LENGTH));

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    NvMTest_ReinitAndReport();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(LastStatusOfEvent_Dem_SetEventStatus[DemConf_DemEventParameter_NVM_LOSS_OF_REDUNDANCY], DEM_EVENT_STATUS_FAILED);
    EXPECT_EQ(LastStatusOfEvent_Dem_SetEventStatus[DemConf_DemEventParameter_NVM_INTEGRITY_FAILED], DEM_EVENT_STATUS_PASSED);
    EXPECT_EQ(CallCount_Dem_SetEventStatus, 2U);
}

}  // namespace
