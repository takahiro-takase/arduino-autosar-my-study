/**
 * \file    Can_MainFunction_Mode_test.cpp
 * \brief   Can_MainFunction_Mode() の単体テスト
 * \details [SWS_Can_00017]/[SWS_Can_00369]。Can_SetControllerMode() が成功した遷移ごとに、CanIf_ControllerModeIndication() を
 *          1 回呼ぶ（CanIf は CanSM_ControllerModeIndication() へ渡す）。通知は、要求の呼び出しとは別に、本関数が行う。
 *          CanIf と CanSM は、実体を初期化して使い、CanSM_ControllerModeIndication() の呼び出し回数を wrap で数える。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Can.h"
#include "Can_Cfg.h"
#include "Can_Hw.h"
#include "CanIf.h"
#include "CanSM.h"
#include "Fake_Can_Hw.h"
#include "Fake_Det_Hw.h"
#include "Wrap_CanIf.h"
#include "Wrap_Can.h"
#include "Wrap_CanSM.h"
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

const CanIf_ConfigType kEmptyCanIfConfig = { NULL, 0U, NULL, 0U };

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* Can_MainFunction_Mode() の単体テスト用フィクスチャ。
 * SetUp(): 模擬 Can_Hw・wrap・DET の記録をリセットし、Can を未初期化の状態へ戻したうえで、CanSM_Init(NULL) と CanIf_Init()（PDU なし）を呼び、Can_Init() に渡す設定（config）を用意する。Can_Init() は各テストが呼ぶ。
 * TearDown(): CanIf と CanSM を未初期化へ戻す。 */
class Can_MainFunction_Mode_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeCanHw_Reset();
        WrapCanIf_Reset();
        WrapCan_Reset();
        WrapCanSM_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        Can_Test_SetConfigPtr(NULL);   // 初期化前状態に戻す
        Can_Test_SetControllerState(CAN_CS_UNINIT);
        Can_Test_ResetTxErrCount();
        CanSM_Init(NULL);
        CanIf_Init(&kEmptyCanIfConfig);
        FakeDetHw_Reset();
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間

        config.filter.filterId = 0x0220U;
        config.filter.mask     = 0x1FFFU;
        config.csPin           = 10U;
        config.intPin          = 2U;
        config.baudrate        = 500000U;
        config.crystalFreq     = CAN_CRYSTAL_16MHZ;
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        CanIf_DeInit();
        CanSM_DeInit();
    }

    Can_ConfigType config;
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Can_MainFunction_Mode_Test, Can_MainFunction_Mode_NG_NullConfig)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // Can_Init(&config);  // 初期化せずに呼ぶ

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_MainFunction_Mode();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, CAN_E_UNINIT);
    EXPECT_EQ(FakeDetHw_LastApiId, CAN_API_ID_MAIN_FUNCTION_MODE);
}

// モード変更の要求が無ければ、何も通知しない
TEST_F(Can_MainFunction_Mode_Test, Can_MainFunction_Mode_OK_NothingToNotifyWithoutTransition)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_MainFunction_Mode();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_CanSM_ControllerModeIndication, 0U);
}

// 成功した遷移ごとに 1 回通知する。通知済みのものは、重ねて通知しない
TEST_F(Can_MainFunction_Mode_Test, Can_MainFunction_Mode_OK_NotifiesEachSuccessfulTransitionOnce)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    ASSERT_EQ(Can_SetControllerMode(0U, CAN_T_START), CAN_OK);
    ASSERT_EQ(Can_SetControllerMode(0U, CAN_T_STOP), CAN_OK);
    ASSERT_EQ(Can_SetControllerMode(0U, CAN_T_SLEEP), CAN_OK);
    ASSERT_EQ(CallCount_CanSM_ControllerModeIndication, 0U);   // 要求の中では、通知しない

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_MainFunction_Mode();
    const uint32 afterFirst = CallCount_CanSM_ControllerModeIndication;
    Can_MainFunction_Mode();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(afterFirst, 3U);
    EXPECT_EQ(CallCount_CanSM_ControllerModeIndication, 3U);
}

// 失敗した遷移（SLEEP から START）は、通知しない
TEST_F(Can_MainFunction_Mode_Test, Can_MainFunction_Mode_OK_FailedTransitionIsNotNotified)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    ASSERT_EQ(Can_SetControllerMode(0U, CAN_T_SLEEP), CAN_OK);
    Can_MainFunction_Mode();   // SLEEP の通知を出し切る
    const uint32 before = CallCount_CanSM_ControllerModeIndication;
    FakeDetHw_LogSuppressed = 1U;   // 不正な遷移の DET ログは抑制する
    ASSERT_EQ(Can_SetControllerMode(0U, CAN_T_START), CAN_NOT_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_MainFunction_Mode();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_CanSM_ControllerModeIndication, before);
}

// 通知がたまりすぎたら（キューは 4 件）、新しいものを破棄する
TEST_F(Can_MainFunction_Mode_Test, Can_MainFunction_Mode_OK_DropsNotificationsBeyondQueueSize)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Init(&config);
    for (uint8 i = 0U; i < 6U; i++)
    {
        ASSERT_EQ(Can_SetControllerMode(0U, ((i % 2U) == 0U) ? CAN_T_START : CAN_T_STOP), CAN_OK);
    }

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_MainFunction_Mode();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_CanSM_ControllerModeIndication, 4U);
}

}  // namespace
