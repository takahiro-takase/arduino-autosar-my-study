/**
 * \file    Bsw_Adc_GetVersionInfo_test.cpp
 * \brief   Adc_GetVersionInfo() の単体テスト
 * \details バージョン情報を、Adc_Cfg.h の定数どおりに書き込む。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Adc.h"
#include "Adc_Cfg.h"
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

/* Adc_GetVersionInfo() の単体テスト用フィクスチャ。
 * SetUp(): DET の記録を初期化する。 */
class Bsw_Adc_GetVersionInfo_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_Reset();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Bsw_Adc_GetVersionInfo_Test, Adc_GetVersionInfo_OK_ReturnsConfiguredVersion)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Std_VersionInfoType info = { 0xFFFFU, 0xFFFFU, 0xFFU, 0xFFU, 0xFFU };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Adc_GetVersionInfo(&info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(info.vendorID, ADC_VENDOR_ID);
    EXPECT_EQ(info.moduleID, ADC_MODULE_ID);
    EXPECT_EQ(info.sw_major_version, ADC_SW_MAJOR_VERSION);
    EXPECT_EQ(info.sw_minor_version, ADC_SW_MINOR_VERSION);
    EXPECT_EQ(info.sw_patch_version, ADC_SW_PATCH_VERSION);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

}  // namespace
