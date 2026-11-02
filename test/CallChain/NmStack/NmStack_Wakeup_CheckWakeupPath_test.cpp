/**
 * \file    NmStack_Wakeup_CheckWakeupPath_test.cpp
 * \brief   起床の検出から、検証待ち（PENDING）になるまでの、仕様の経路の結線確認（Can → EcuM → CanIf → Can → EcuM）
 * \details 2026-11、起床の確認と検証待ちへの登録が、EcuM_CheckWakeup() の一つにまとまっていたのを、仕様の経路へ分けた:
 *            Can_MainFunction_Wakeup() → EcuM_CheckWakeup() → CanIf_CheckWakeup() → Can_CheckWakeup() → EcuM_SetWakeupEvent()
 *          EcuM は実体（模擬 EcuM を素通しにする）、Can と CanIf も実体のまま結ぶ。各段の分岐は、各モジュールの単体テストで見る。
 *          ここでは、4 つの関数が実際に呼び合って、起床が検証待ちとして EcuM に残るところまでを確かめる。
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
#include "EcuM.h"
#include "EcuM_Cfg.h"
#include "Fake_Bsw_EcuM.h"
#include "Fake_Can_Hw.h"
#include "Fake_Det_Hw.h"
#include "Fake_Millis.h"
#include "Fake_Gpt_Hw.h"
#include "Fake_Wdg_Hw.h"
#include "Gpt.h"
#include "Gpt_PBCfg.h"
#include "Gpt_Hw.h"
#include "Os.h"
#include "Wdg.h"
#include "Wdg_PBCfg.h"
#include "WdgM.h"
#include "WdgM_PBCfg.h"
#include "Dem.h"
#include "Wrap_Can.h"
#include "Wrap_CanIf.h"
#include "Wrap_Dem.h"
#include "Wrap_Os.h"
#include "Wrap_BswM.h"
#include "Wrap_CanSM.h"
#include "Wrap_WdgM.h"
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

constexpr uint8 TEST_OS_GPT_CHANNEL = GPT_CHANNEL_1;

const CanIf_ConfigType kWakeupPathCanIfConfig = {
    /* TxPduConfig */ NULL,
    /* TxPduCount */  0U,
    /* RxPduConfig */ NULL,
    /* RxPduCount */  0U
};

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* 起床の経路の結線確認用フィクスチャ。
 * SetUp(): 模擬 EcuM をパススルーにして実体の EcuM を呼び、時刻の源（Gpt と Os、タスク 0 件）・Dem・Wdg・WdgM を EcuM_Init() と同じ順序で初期化する
 *          （EcuM_SetWakeupEvent() が検証のタイムアウトの起算に時刻を使うため）。Can と CanIf は実体を初期化し、コントローラをスリープさせて、
 *          INT ピンが LOW の状態（起床要因あり）を用意する。
 * TearDown(): Gpt・Os・EcuM・CanIf と模擬 EcuM を元に戻す。 */
class NmStack_Wakeup_CheckWakeupPath_Test : public ::testing::Test
{
protected:
    Gpt_ChannelConfigType channels[2];
    Gpt_ConfigType        gptConfig;
    Os_ConfigType         osConfig;
    Can_ConfigType        canConfig;

    void SetUp() override
    {
        FakeEcuM_Reset();
        FakeEcuM_PassThrough = 1U;
        FakeGptHw_Reset();
        FakeDetHw_LogSuppressed = 1U;
        FakeDetHw_Reset();
        FakeMillis_Reset();
        FakeWdgHw_Reset();
        FakeCanHw_Reset();
        WrapOs_Reset();
        WrapBswM_Reset();
        WrapCanSM_Reset();
        WrapWdgM_Reset();
        WrapDem_Reset();
        WrapCan_Reset();
        WrapCanIf_Reset();
        EcuM_Test_ResetInitState();
        Os_Test_ResetInitState();

        channels[0].ChannelId       = GPT_CHANNEL_0;
        channels[0].Mode            = GPT_CH_MODE_CONTINUOUS;
        channels[0].TickFrequencyHz = 1000U;
        channels[0].TickValueMax    = 0xFFFFFFFFU;
        channels[0].Notification    = nullptr;
        channels[1].ChannelId       = TEST_OS_GPT_CHANNEL;
        channels[1].Mode            = GPT_CH_MODE_CONTINUOUS;
        channels[1].TickFrequencyHz = 1000U;
        channels[1].TickValueMax    = 0xFFFFFFFFU;
        channels[1].Notification    = nullptr;
        gptConfig.Channels     = channels;
        gptConfig.ChannelCount = 2U;
        osConfig.Tasks     = nullptr;
        osConfig.TaskCount = 0U;

        Gpt_Init(&gptConfig);
        Os_Init(&osConfig);
        Dem_Init(NULL);
        Wdg_Init(&Wdg_Config);
        WdgM_Init(&WdgM_Config);

        canConfig.filter.filterId = 0x0220U;
        canConfig.filter.mask     = 0x1FFFU;
        canConfig.csPin           = 10U;
        canConfig.intPin          = 2U;
        canConfig.baudrate        = 500000U;
        canConfig.crystalFreq     = CAN_CRYSTAL_16MHZ;
        Can_Test_SetConfigPtr(NULL);
        Can_Init(&canConfig);
        CanIf_Init(&kWakeupPathCanIfConfig);
        (void)Can_SetControllerMode(0U, CAN_CS_SLEEP);
        FakeCanHw_IsWakeupPendingReturn = CAN_HW_OK;   // INT ピンが LOW（起床要因あり）

        WrapOs_Reset();
        WrapBswM_Reset();
        WrapWdgM_Reset();
        WrapCan_Reset();
        WrapCanIf_Reset();
        FakeDetHw_Reset();
        FakeDetHw_LogSuppressed = 0U;
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        CanIf_DeInit();
        Gpt_StopTimer(TEST_OS_GPT_CHANNEL);
        Gpt_DeInit();
        Os_Test_ResetInitState();
        EcuM_Test_ResetInitState();
        FakeEcuM_Reset();  // PassThrough も FALSE へ戻る
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(NmStack_Wakeup_CheckWakeupPath_Test, OK_DetectedWakeupBecomesPendingThroughCanIfAndCan)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EcuM_Test_SetState(ECUM_STATE_RUN);
    ASSERT_EQ(EcuM_GetPendingWakeupEvents(), 0U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_MainFunction_Wakeup();   // 起床を検出し、EcuM_CheckWakeup() を呼ぶ（以降の 4 段は、実体で呼び合う）

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_CanIf_CheckWakeup, 1U);                           // EcuM_CheckWakeup() → CanIf_CheckWakeup()
    EXPECT_EQ(LastWakeupSource_CanIf_CheckWakeup, ECUM_WKSOURCE_CAN);
    EXPECT_EQ(EcuM_GetPendingWakeupEvents(), ECUM_WKSOURCE_CAN);          // → Can_CheckWakeup() → EcuM_SetWakeupEvent()
    EXPECT_EQ(EcuM_GetValidatedWakeupEvents(), 0U);
    EXPECT_EQ(CallCount_BswM_EcuM_CurrentWakeup, 1U);                     // 検証待ちになったことを BswM へ通知した
    EXPECT_EQ(Last_state_BswM_EcuM_CurrentWakeup, ECUM_WKSTATUS_PENDING);
    EXPECT_EQ(Last_source_BswM_EcuM_CurrentWakeup, ECUM_WKSOURCE_CAN);
}

TEST_F(NmStack_Wakeup_CheckWakeupPath_Test, OK_NoWakeupDetectedLeavesNothingPending)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EcuM_Test_SetState(ECUM_STATE_RUN);
    FakeCanHw_IsWakeupPendingReturn = CAN_HW_FAIL;   // INT ピンが HIGH（起床要因なし）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_MainFunction_Wakeup();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_CanIf_CheckWakeup, 0U);   // 検出しなければ、EcuM_CheckWakeup() も呼ばない
    EXPECT_EQ(EcuM_GetPendingWakeupEvents(), 0U);
    EXPECT_EQ(CallCount_BswM_EcuM_CurrentWakeup, 0U);
}

}  // namespace
