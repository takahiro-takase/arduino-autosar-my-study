/**
 * \file    Can_CheckWakeup_test.cpp
 * \brief   Can_CheckWakeup() の単体テスト
 * \details コントローラが起床を検出したかを調べ、検出していれば EcuM_SetWakeupEvent() で通知する（[SWS_Can_00361]）。
 *          検出は Can_MainFunction_Wakeup() が記録する（INT ピンの LOW を、スリープ中に検出したとき）。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Can.h"
#include "Can_Cfg.h"
#include "Can_Hw.h"
#include "Fake_Can_Hw.h"
#include "Fake_Det_Hw.h"
#include "Wrap_CanIf.h"
#include "Wrap_Can.h"
#include "Fake_Bsw_EcuM.h"
#include "EcuM.h"
}

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

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* Can_CheckWakeup() の単体テスト用フィクスチャ。
 * SetUp(): 模擬 Can_Hw・wrap・模擬 EcuM・DET の記録をリセットし、Can を未初期化の状態へ戻したうえで、Can_Init() に渡す設定（config）を用意する。Can_Init() は各テストが呼ぶ。
 * SleepAndDetectWakeup(): スリープへ入り、INT ピンが LOW になった状態で Can_MainFunction_Wakeup() を呼んで、起床を検出させる。 */
class Can_CheckWakeup_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeCanHw_Reset();
        WrapCanIf_Reset();
        WrapCan_Reset();
        FakeEcuM_Reset();
        FakeDetHw_Reset();
        Can_Test_SetConfigPtr(NULL);
        Can_Test_SetControllerState(CAN_CS_UNINIT);
        Can_Test_ResetTxErrCount();

        config.filter.filterId = 0x0220U;
        config.filter.mask     = 0x1FFFU;
        config.csPin           = 10U;
        config.intPin          = 2U;
        config.baudrate        = 500000U;
        config.crystalFreq     = CAN_CRYSTAL_16MHZ;
    }

    void SleepAndDetectWakeup()
    {
        ASSERT_EQ(Can_SetControllerMode(0U, CAN_CS_SLEEP), E_OK);
        FakeCanHw_IsWakeupPendingReturn = CAN_HW_OK;   // INT ピンが LOW（起床要因がアサート中）
        Can_MainFunction_Wakeup();
        FakeCanHw_IsWakeupPendingReturn = CAN_HW_FAIL;
        FakeDetHw_Reset();
        FakeEcuM_Reset();
    }

    Can_ConfigType config;
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Can_CheckWakeup_Test, Can_CheckWakeup_NG_NullConfig)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // Can_Init(&config);  // 初期化せずに呼ぶ

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Can_CheckWakeup(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CAN_E_UNINIT);
    EXPECT_EQ(FakeDetHw_LastApiId, CAN_API_ID_CHECK_WAKEUP);
    EXPECT_EQ(FakeEcuM_SetWakeupEventCount, 0U);
}

TEST_F(Can_CheckWakeup_Test, Can_CheckWakeup_NG_InvalidController)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    SleepAndDetectWakeup();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Can_CheckWakeup(1U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CAN_E_PARAM_CONTROLLER);
    EXPECT_EQ(FakeDetHw_LastApiId, CAN_API_ID_CHECK_WAKEUP);
    EXPECT_EQ(FakeEcuM_SetWakeupEventCount, 0U);   // 起床を検出していても、不正なコントローラでは通知しない
}

TEST_F(Can_CheckWakeup_Test, Can_CheckWakeup_NG_NoWakeupDetected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    ASSERT_EQ(Can_SetControllerMode(0U, CAN_CS_SLEEP), E_OK);   // 眠っているだけで、起床は検出していない
    FakeDetHw_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Can_CheckWakeup(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeEcuM_SetWakeupEventCount, 0U);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);   // 検出していないのは、エラーではない
}

TEST_F(Can_CheckWakeup_Test, Can_CheckWakeup_OK_WakeupDetectedNotifiesEcuM)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    SleepAndDetectWakeup();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Can_CheckWakeup(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(FakeEcuM_SetWakeupEventCount, 1U);
    EXPECT_EQ(FakeEcuM_LastSetWakeupEventSources, ECUM_WKSOURCE_CAN);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(Can_CheckWakeup_Test, Can_CheckWakeup_OK_DetectionIsKeptAcrossCalls)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    SleepAndDetectWakeup();
    ASSERT_EQ(Can_CheckWakeup(0U), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Can_CheckWakeup(0U);   // スリープから出るか、次にスリープへ入るまで、検出の記録は残る

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(FakeEcuM_SetWakeupEventCount, 2U);
}

TEST_F(Can_CheckWakeup_Test, Can_CheckWakeup_NG_LeavingSleepConsumesDetection)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    SleepAndDetectWakeup();
    ASSERT_EQ(Can_SetControllerMode(0U, CAN_CS_STOPPED), E_OK);   // 起床して、スリープから出た
    FakeEcuM_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Can_CheckWakeup(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);   // 検出した起床は消費済み。EcuM_SetWakeupEvent() を再び呼ばない
    EXPECT_EQ(FakeEcuM_SetWakeupEventCount, 0U);
}

TEST_F(Can_CheckWakeup_Test, Can_CheckWakeup_NG_SleepAgainClearsDetection)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    SleepAndDetectWakeup();
    ASSERT_EQ(Can_SetControllerMode(0U, CAN_CS_STOPPED), E_OK);   // 起床して、
    ASSERT_EQ(Can_SetControllerMode(0U, CAN_CS_SLEEP), E_OK);     // 検証に失敗して、また眠る
    FakeEcuM_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Can_CheckWakeup(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);   // 新しいスリープでは、前回の起床の記録を捨てる
    EXPECT_EQ(FakeEcuM_SetWakeupEventCount, 0U);
}

TEST_F(Can_CheckWakeup_Test, Can_CheckWakeup_NG_DeInitClearsDetection)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    SleepAndDetectWakeup();
    ASSERT_EQ(Can_SetControllerMode(0U, CAN_CS_STOPPED), E_OK);
    Can_DeInit();
    Can_Init(&config);   // 再び使えるようにする（スリープには入らない）
    FakeEcuM_Reset();
    FakeDetHw_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Can_CheckWakeup(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeEcuM_SetWakeupEventCount, 0U);
}

}  // namespace
