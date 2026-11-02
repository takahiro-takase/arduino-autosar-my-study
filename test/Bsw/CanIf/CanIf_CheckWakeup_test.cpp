/**
 * \file    CanIf_CheckWakeup_test.cpp
 * \brief   CanIf_CheckWakeup() の単体テスト
 * \details CanIf_CheckWakeup() は、CAN の起床要因について Can_CheckWakeup() を呼び、その戻り値を返す
 *          （[SWS_CANIF_00395]/[SWS_CANIF_00720]/[SWS_CANIF_00678]）。Can を実体のまま使い、スリープ中の起床の検出は
 *          Can_MainFunction_Wakeup()（INT ピンの LOW）で作る。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "CanIf.h"
#include "CanSM.h"
#include "Can.h"
#include "Can_Hw.h"
#include "Fake_Can_Hw.h"
#include "Fake_Det_Hw.h"
#include "Fake_Bsw_EcuM.h"
#include "Wrap_CanIf.h"
#include "Wrap_Can.h"
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

const CanIf_ConfigType kCheckWakeupCanIfConfig = {
    /* TxPduConfig */ NULL,
    /* TxPduCount */  0U,
    /* RxPduConfig */ NULL,
    /* RxPduCount */  0U
};

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* CanIf_CheckWakeup() の単体テスト用フィクスチャ。
 * SetUp(): 模擬 Can_Hw・wrap・模擬 EcuM をリセットし、Can_Init() と CanIf_Init() を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消す）。
 * SleepAndDetectWakeup(): Can をスリープさせ、INT ピンが LOW の状態で Can_MainFunction_Wakeup() を呼んで、起床を検出させる。
 * TearDown(): CanIf_DeInit() で未初期化へ戻す。 */
class CanIf_CheckWakeup_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeCanHw_Reset();
        WrapCan_Reset();
        WrapCanIf_Reset();
        FakeEcuM_Reset();
        FakeDetHw_LogSuppressed = 1U;

        canConfig.filter.filterId = 0x0220U;
        canConfig.filter.mask     = 0x1FFFU;
        canConfig.csPin           = 10U;
        canConfig.intPin          = 2U;
        canConfig.baudrate        = 500000U;
        canConfig.crystalFreq     = CAN_CRYSTAL_16MHZ;

        Can_Test_SetConfigPtr(NULL);
        Can_Init(&canConfig);
        CanIf_Init(&kCheckWakeupCanIfConfig);
        FakeDetHw_Reset();
        FakeDetHw_LogSuppressed = 0U;
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        WrapCan_Reset();
        CanIf_DeInit();
    }

    void SleepAndDetectWakeup()
    {
        ASSERT_EQ(Can_SetControllerMode(0U, CAN_CS_SLEEP), E_OK);
        FakeCanHw_IsWakeupPendingReturn = CAN_HW_OK;
        Can_MainFunction_Wakeup();
        FakeCanHw_IsWakeupPendingReturn = CAN_HW_FAIL;
        FakeEcuM_Reset();
        FakeDetHw_Reset();
    }

    Can_ConfigType canConfig;
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(CanIf_CheckWakeup_Test, CheckWakeup_NG_UninitializedReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanIf_DeInit();   // 未初期化にする

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanIf_CheckWakeup(ECUM_WKSOURCE_CAN);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_UNINIT);
    EXPECT_EQ(FakeDetHw_LastApiId, CANIF_API_ID_CHECK_WAKEUP);
}

TEST_F(CanIf_CheckWakeup_Test, CheckWakeup_NG_InvalidWakeupSourceReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    SleepAndDetectWakeup();   // 起床を検出していても、要因が不正なら、Can へ聞かない

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanIf_CheckWakeup(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_WAKEUPSOURCE);   // [SWS_CANIF_00398]
    EXPECT_EQ(FakeDetHw_LastApiId, CANIF_API_ID_CHECK_WAKEUP);
    EXPECT_EQ(FakeEcuM_SetWakeupEventCount, 0U);
}

TEST_F(CanIf_CheckWakeup_Test, CheckWakeup_NG_NoWakeupDetectedReturnsNotOk)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Can_SetControllerMode(0U, CAN_CS_SLEEP), E_OK);   // 眠っているだけ
    FakeDetHw_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanIf_CheckWakeup(ECUM_WKSOURCE_CAN);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);   // Can_CheckWakeup() の戻り値をそのまま返す（[SWS_CANIF_00678]）
    EXPECT_EQ(FakeEcuM_SetWakeupEventCount, 0U);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(CanIf_CheckWakeup_Test, CheckWakeup_OK_WakeupDetectedNotifiesEcuMThroughCan)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    SleepAndDetectWakeup();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanIf_CheckWakeup(ECUM_WKSOURCE_CAN);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);   // [SWS_CANIF_00720]
    EXPECT_EQ(FakeEcuM_SetWakeupEventCount, 1U);   // Can_CheckWakeup() が EcuM_SetWakeupEvent() を呼んだ
    EXPECT_EQ(FakeEcuM_LastSetWakeupEventSources, ECUM_WKSOURCE_CAN);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

}  // namespace
