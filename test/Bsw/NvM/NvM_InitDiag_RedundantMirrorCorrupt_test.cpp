/**
 * \file    NvM_InitDiag_RedundantMirrorCorrupt_test.cpp
 * \brief   冗長ブロックのミラー面の破損を、NVM_LOSS_OF_REDUNDANCY（FAILED）として Dem へ報告することの単体テスト
 * \details 冗長ブロック（DEM_EXTENDED）のミラー面の CRC だけを壊すと、プライマリ面のデータを採用し、ミラーを
 *          プライマリの内容で自己修復する。最初の NvM_MainFunction() が NVM_LOSS_OF_REDUNDANCY を FAILED で報告する
 *          （仕様の最小読み込みアルゴリズムでは検出しえない、本プロジェクト独自の対称的な拡張）。
 *          有効なデータは得られたため、NVM_INTEGRITY_FAILED は PASSED。
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

/* NvM_InitDiag_RedundantMirrorCorrupt_Test のフィクスチャ。
 * SetUp(): DET と Dem の wrap を初期化し、全ブロックが有効な EEPROM を用意する。
 * TearDown(): NvM を未初期化へ戻す。 */
class NvM_InitDiag_RedundantMirrorCorrupt_Test : public ::testing::Test
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

TEST_F(NvM_InitDiag_RedundantMirrorCorrupt_Test, NvM_MainFunction_OK_MirrorCorruptReportsLossOfRedundancy)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    NvMTest_CorruptByte(static_cast<uint16>(NVM_BLOCK_DEM_EXTENDED_MIRROR_EEPROM_ADDR + NVM_BLOCK_DEM_EXTENDED_LENGTH));

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
