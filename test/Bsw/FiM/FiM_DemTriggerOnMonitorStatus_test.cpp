/**
 * \file    FiM_DemTriggerOnMonitorStatus_test.cpp
 * \brief   FiM_DemTriggerOnMonitorStatus() と、Dem からの通知（Dem_SetEventStatus）の単体テスト
 *
 * \details Dem がイベントのステータスの変化を FiM へ知らせると（[SWS_Fim_00021]）、FiM が、周期（FiM_MainFunction）を
 *          待たずに、そのイベントを使う機能の許可状態を更新することを確認する（[SWS_Fim_00020]）。
 *          FiM が Dem へ問い合わせた回数は、Dem の wrap（CallCount_Dem_GetEventUdsStatus）で数える。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "FiM.h"
#include "Dem.h"
#include "Dem_Cfg.h"
#include "Fake_Millis.h"
#include "Fake_Det_Hw.h"
#include "NvM_TestUtil.h"
#include "Wrap_Dem.h"
}

/* ======================================================================
 * Global Variables
 * ====================================================================== */

namespace
{

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* FiM の単体テスト用フィクスチャ。
 * SetUp(): 時刻と wrap を初期化し、NvM を空の EEPROM から初期化して、Dem_Init(NULL) を呼ぶ。FiM_Init() は各テストが呼ぶ。
 * TearDown(): FiM を未初期化の状態へ戻し、NvM を片付ける。 */
class FiM_DemTriggerOnMonitorStatus_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        WrapDem_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Dem_Init() のログはノイズになるため抑制

        NvMTest_ColdBoot();
        Dem_Init(NULL);

        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        FiM_Test_ResetInitState();
        WrapDem_Reset();
        NvMTest_Shutdown();
    }

    static boolean GetPermission(FiM_FunctionIdType fid)
    {
        boolean permission = FALSE;
        const Std_ReturnType ret = FiM_GetFunctionPermission(fid, &permission);
        EXPECT_EQ(ret, E_OK);
        return permission;
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

// OK: FiM_Init() の前に呼んでも、何もしない（DET を報告せず、Dem へも問い合わせない）。Dem は、FiM の初期化より前にも報告するため。
TEST_F(FiM_DemTriggerOnMonitorStatus_Test, FiM_DemTriggerOnMonitorStatus_OK_UninitializedDoesNothing)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FiM_Test_ResetInitState();
    FakeDetHw_Reset();
    WrapDem_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    FiM_DemTriggerOnMonitorStatus(DemConf_DemEventParameter_CAN_BUSOFF);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
    EXPECT_EQ(CallCount_Dem_GetEventUdsStatus, 0U);
}

// OK: 抑止の条件に使っているイベントの通知で、機能の許可状態を、すぐに再評価する（Dem へ問い合わせる）。
TEST_F(FiM_DemTriggerOnMonitorStatus_Test, FiM_DemTriggerOnMonitorStatus_OK_ReevaluatesWhenEventIsUsedByInhibition)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FiM_Init(&FiM_Config);
    WrapDem_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    FiM_DemTriggerOnMonitorStatus(DemConf_DemEventParameter_CAN_BUSOFF);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_GE(CallCount_Dem_GetEventUdsStatus, 1U);
}

// OK: 抑止の条件に使っていないイベントの通知では、再評価しない（Dem へ問い合わせない）。
TEST_F(FiM_DemTriggerOnMonitorStatus_Test, FiM_DemTriggerOnMonitorStatus_OK_IgnoresEventNotUsedByAnyInhibition)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FiM_Init(&FiM_Config);
    WrapDem_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    FiM_DemTriggerOnMonitorStatus(DemConf_DemEventParameter_ADC_VOLT_LOW);   // FiM_Config は、CAN_BUSOFF と BUTTON_STUCK だけを使う

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Dem_GetEventUdsStatus, 0U);
}

// OK: Dem_SetEventStatus() で、イベントが確定（FAILED）すると、FiM_MainFunction() を待たずに、機能が抑止される。
TEST_F(FiM_DemTriggerOnMonitorStatus_Test, Dem_SetEventStatus_OK_InhibitsFidWithoutWaitingForMainFunction)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FiM_Init(&FiM_Config);
    ASSERT_EQ(GetPermission(FIM_FID_RUNNING_LED), TRUE);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    (void)Dem_SetEventStatus(DemConf_DemEventParameter_CAN_BUSOFF, DEM_EVENT_STATUS_FAILED);   // FiM_MainFunction() は呼ばない

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(GetPermission(FIM_FID_RUNNING_LED), FALSE);
    EXPECT_EQ(GetPermission(FIM_FID_BUTTON_ACK), TRUE);   // 無関係の機能は、影響を受けない
}

}  // namespace
