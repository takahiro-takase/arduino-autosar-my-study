/**
 * \file    NvM_InitDiag_RedundantBothCorrupt_test.cpp
 * \brief   冗長ブロックの両面の破損を、NVM_INTEGRITY_FAILED（FAILED）だけで報告することの単体テスト
 * \details 冗長ブロック（DEM_EXTENDED）の両面の CRC を壊すと、デフォルト値で復元する。有効なデータが得られなかったため、
 *          最初の NvM_MainFunction() が NVM_INTEGRITY_FAILED を FAILED で報告する。NVM_LOSS_OF_REDUNDANCY は
 *          「片方は生きている」ことが前提の規定のため、二重に報告しない（より重篤な INTEGRITY_FAILED に一本化、
 *          冗長ブロックが他に無いため、報告自体が行われない）。
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

/* NvM_InitDiag_RedundantBothCorrupt_Test のフィクスチャ。
 * SetUp(): DET と Dem の wrap を初期化し、全ブロックが有効な EEPROM を用意する。
 * TearDown(): NvM を未初期化へ戻す。 */
class NvM_InitDiag_RedundantBothCorrupt_Test : public ::testing::Test
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

TEST_F(NvM_InitDiag_RedundantBothCorrupt_Test, NvM_MainFunction_OK_BothCopiesCorruptReportsIntegrityFailedOnly)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    NvMTest_CorruptByte(static_cast<uint16>(NVM_BLOCK_DEM_EXTENDED_EEPROM_ADDR + NVM_BLOCK_DEM_EXTENDED_LENGTH));
    NvMTest_CorruptByte(static_cast<uint16>(NVM_BLOCK_DEM_EXTENDED_MIRROR_EEPROM_ADDR + NVM_BLOCK_DEM_EXTENDED_LENGTH));

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    NvMTest_ReinitAndReport();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(LastStatusOfEvent_Dem_SetEventStatus[DemConf_DemEventParameter_NVM_INTEGRITY_FAILED], DEM_EVENT_STATUS_FAILED);
    EXPECT_EQ(LastStatusOfEvent_Dem_SetEventStatus[DemConf_DemEventParameter_NVM_LOSS_OF_REDUNDANCY], 0xFFU);   // 報告されない
    EXPECT_EQ(CallCount_Dem_SetEventStatus, 1U);
}

}  // namespace
