/**
 * \file    Bsw_WdgM_SetGetMode_test.cpp
 * \brief   WdgM_SetMode()/WdgM_GetMode() の単体テスト
 * \details AUTOSAR SWS_WdgM_00154/SWS_WdgM_00168 準拠のシグネチャで新設。
 *          本プロジェクトは単一の静的コンフィグのみ保持するため、
 *          WDGM_MODE_DEFAULT (0) のみを有効なモードとして受理する簡略実装
 *          であることを検証する。
 *          本番の WdgM_Config (WdgM_PBCfg.c) をそのまま使う。
 */
#include <gtest/gtest.h>
#include "WdgM.h"
#include "WdgIf_fake.h"
#include "Hal_Det_Hw_fake.h"
#include "Hal_Millis_fake.h"

class Bsw_WdgM_SetGetMode_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        FakeDetHw_Reset();
        FakeWdgIf_Reset();
        WdgM_Init(&WdgM_Config);
        FakeDetHw_Reset();  /* Init 自体が出す DET ログ・記録を後続の検証対象から除く */
    }
};

TEST_F(Bsw_WdgM_SetGetMode_Test, SetMode_OK_DefaultModeReturnsOk)
{
    Std_ReturnType ret = WdgM_SetMode(WDGM_MODE_DEFAULT);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(Bsw_WdgM_SetGetMode_Test, SetMode_NG_OutOfRangeModeReturnsErrorAndReportsDet)
{
    Std_ReturnType ret = WdgM_SetMode((WdgM_ModeType)(WDGM_MODE_DEFAULT + 1U));

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, WDGM_E_PARAM_MODE);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);
}

TEST_F(Bsw_WdgM_SetGetMode_Test, SetMode_NG_UninitializedReturnsErrorAndReportsDet)
{
    WdgM_DeInit();
    FakeDetHw_Reset();

    Std_ReturnType ret = WdgM_SetMode(WDGM_MODE_DEFAULT);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, WDGM_E_NO_INIT);
}

TEST_F(Bsw_WdgM_SetGetMode_Test, GetMode_OK_ReturnsDefaultModeRightAfterInit)
{
    WdgM_ModeType mode = (WdgM_ModeType)0xFFU;
    Std_ReturnType ret = WdgM_GetMode(&mode);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(mode, WDGM_MODE_DEFAULT);
}

TEST_F(Bsw_WdgM_SetGetMode_Test, GetMode_OK_ReflectsPreviousSetMode)
{
    ASSERT_EQ(WdgM_SetMode(WDGM_MODE_DEFAULT), E_OK);

    WdgM_ModeType mode = (WdgM_ModeType)0xFFU;
    ASSERT_EQ(WdgM_GetMode(&mode), E_OK);
    EXPECT_EQ(mode, WDGM_MODE_DEFAULT);
}

TEST_F(Bsw_WdgM_SetGetMode_Test, GetMode_NG_NullPointerReturnsErrorAndReportsDet)
{
    Std_ReturnType ret = WdgM_GetMode(NULL);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, WDGM_E_INV_POINTER);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);
}

TEST_F(Bsw_WdgM_SetGetMode_Test, GetMode_NG_UninitializedReturnsDefaultAndReportsDet)
{
    WdgM_DeInit();
    FakeDetHw_Reset();

    WdgM_ModeType mode = (WdgM_ModeType)0xFFU;
    Std_ReturnType ret = WdgM_GetMode(&mode);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(mode, WDGM_MODE_DEFAULT);
    EXPECT_EQ(FakeDetHw_LastErrorId, WDGM_E_NO_INIT);
}
