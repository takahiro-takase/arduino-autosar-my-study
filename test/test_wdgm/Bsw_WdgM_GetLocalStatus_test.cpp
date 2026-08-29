/**
 * \file    Bsw_WdgM_GetLocalStatus_test.cpp
 * \brief   WdgM_GetLocalStatus() の単体テスト
 * \details AUTOSAR SWS_WdgM_00169 準拠のシグネチャ
 *          (`Std_ReturnType WdgM_GetLocalStatus(SEID, WdgM_LocalStatusType* Status)`)
 *          へ変更した際に新設。異常系 (NULL ポインタ・未初期化・SEID 不正) と、
 *          Alive/Logical Supervision の結果反映を検証する。
 *          本番の WdgM_Config (WdgM_PBCfg.c、Entity 0=ENGINE/Entity 1=WARNING の
 *          2エンティティ構成) をそのまま使う。
 */
#include <gtest/gtest.h>
#include "WdgM.h"
#include "WdgIf_fake.h"
#include "Hal_Det_Hw_fake.h"
#include "Hal_Millis_fake.h"

class Bsw_WdgM_GetLocalStatus_Test : public ::testing::Test
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

TEST_F(Bsw_WdgM_GetLocalStatus_Test, GetLocalStatus_NG_NullPointerReturnsErrorAndReportsDet)
{
    Std_ReturnType ret = WdgM_GetLocalStatus(WDGM_ENTITY_ENGINE, NULL);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, WDGM_E_INV_POINTER);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);
}

TEST_F(Bsw_WdgM_GetLocalStatus_Test, GetLocalStatus_NG_UninitializedReturnsDeactivatedAndReportsDet)
{
    WdgM_DeInit();
    FakeDetHw_Reset();

    WdgM_LocalStatusType status = WDGM_LOCAL_STATUS_OK;
    Std_ReturnType ret = WdgM_GetLocalStatus(WDGM_ENTITY_ENGINE, &status);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(status, WDGM_LOCAL_STATUS_DEACTIVATED);
    EXPECT_EQ(FakeDetHw_LastErrorId, WDGM_E_NO_INIT);
}

TEST_F(Bsw_WdgM_GetLocalStatus_Test, GetLocalStatus_NG_InvalidSeidReturnsDeactivatedAndReportsDet)
{
    WdgM_LocalStatusType status = WDGM_LOCAL_STATUS_OK;
    Std_ReturnType ret = WdgM_GetLocalStatus(WdgM_Config.EntityCount, &status);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(status, WDGM_LOCAL_STATUS_DEACTIVATED);
    EXPECT_EQ(FakeDetHw_LastErrorId, WDGM_E_PARAM_SEID);
}

TEST_F(Bsw_WdgM_GetLocalStatus_Test, GetLocalStatus_OK_ReturnsOkRightAfterInit)
{
    WdgM_LocalStatusType status = WDGM_LOCAL_STATUS_FAILED;
    Std_ReturnType ret = WdgM_GetLocalStatus(WDGM_ENTITY_ENGINE, &status);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(status, WDGM_LOCAL_STATUS_OK);
}

TEST_F(Bsw_WdgM_GetLocalStatus_Test, GetLocalStatus_OK_ReturnsFailedAfterAliveShortfall)
{
    /* CheckpointReached を一切呼ばずに WdgM_MainFunction() を実行すると、
     * 両エンティティとも Alive Supervision が期待回数を満たせず FAILED になる。 */
    WdgM_MainFunction();

    WdgM_LocalStatusType status = WDGM_LOCAL_STATUS_OK;
    ASSERT_EQ(WdgM_GetLocalStatus(WDGM_ENTITY_WARNING, &status), E_OK);
    EXPECT_EQ(status, WDGM_LOCAL_STATUS_FAILED);
}

TEST_F(Bsw_WdgM_GetLocalStatus_Test, GetLocalStatus_OK_ReturnsFailedImmediatelyAfterLogicalViolation)
{
    /* ENGINE の許可遷移は INITIAL->START/START->END/END->START のみ。
     * INITIAL から直接 END へ遷移させ、Logical Supervision 違反を即座に起こす。
     * WdgM_MainFunction() を待たず、この時点で既に FAILED になる。 */
    Std_ReturnType cpRet = WdgM_CheckpointReached(WDGM_ENTITY_ENGINE, WDGM_CP_ENGINE_END);
    ASSERT_EQ(cpRet, E_OK);

    WdgM_LocalStatusType status = WDGM_LOCAL_STATUS_OK;
    ASSERT_EQ(WdgM_GetLocalStatus(WDGM_ENTITY_ENGINE, &status), E_OK);
    EXPECT_EQ(status, WDGM_LOCAL_STATUS_FAILED);
}
