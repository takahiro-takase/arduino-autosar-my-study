/**
 * \file    NmStack_Wakeup_PassiveStartup_test.cpp
 * \brief   ウェイクアップの検証に成功したあと、誰も通信を要求しなければ、CanNm が Ready Sleep を経て、
 *          スリープへ向かうこと（パッシブ起動、[SWS_ComM_00665]/[SWS_CanNm_00128]）のコールチェーンのテスト。
 *
 * \details Can_MainFunction_Wakeup() → EcuM_CheckWakeup() → CanSM_ControllerModeIndication() → WAKEUP_VALIDATING
 *            → Can_MainFunction_Read() / CanIf_CheckValidation() → ComM_EcuM_WakeUpIndication()
 *              → CanSM_RequestComMode(FULL_COM) → ComM_BusSM_ModeIndication(FULL_COM)
 *                → Nm_PassiveStartUp() → CanNm_PassiveStartUp()（Repeat Message State。ネットワークの要求は立てない）
 *            → CanNm_MainFunction()（Repeat Message Time の満了 → Ready Sleep State → NM-Timeout → Prepare Bus-Sleep
 *              → Wait Bus-Sleep → Bus-Sleep Mode）→ ComM_Nm_BusSleepMode() → CanSM_RequestComMode(NO_COM)
 *          以前は、起床のたびに ComM が Nm_NetworkRequest() を呼び、CanNm が Normal Operation State へ進んでいた。
 *          フィクスチャは NmStack_Wakeup_test.cpp と同じ構成（クラス名だけ別）。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "CanSM.h"
#include "CanIf.h"
#include "Can.h"
#include "Can_Hw.h"
#include "ComM.h"
#include "CanNm.h"
#include "Nm.h"
#include "Fake_Can_Hw.h"
#include "Fake_Millis.h"
#include "Fake_Bsw_EcuM.h"
#include "Wrap_BswM.h"
#include "Wrap_Dem.h"
#include "Fake_Det_Hw.h"
#include "Wrap_CanIf.h"
#include "Wrap_Can.h"
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

const CanIf_ConfigType kWakeup_PassiveStartupCanIfConfig = {
    /* TxPduConfig */ NULL,
    /* TxPduCount */  0U,
    /* RxPduConfig */ NULL,
    /* RxPduCount */  0U
};

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* ウェイクアップ検出と検証（Can_MainFunction_Wakeup → EcuM_CheckWakeup → CanSM → … → FULL_COM）のコールチェーンのテストで共通のフィクスチャ（シナリオごとの派生クラスの基底）。
 * SetUp(): 模擬 Can_Hw・EcuM と wrap をリセットし、Dem_Init(NULL)・Can_Init()（直後は STOPPED）・CanIf_Init(&kWakeup_PassiveStartupCanIfConfig)・CanSM_Init(NULL)・ComM_Init(NULL)・CanNm_Init(NULL)・Nm_Init(NULL) の順に初期化したうえで、コントローラを SLEEP にする（スリープ済みの状態から始める）。CurrentComMode() は ComM が把握しているチャネルモードを返す補助関数、ArrangeValidating() は検証開始（WAKEUP_VALIDATING）の状態を用意する補助関数。
 * TearDown(): CanNm・ComM・CanSM・CanIf を未初期化へ戻す。 */
class Bsw_NmStack_Wakeup_PassiveStartup_Base : public ::testing::Test
{
protected:
    static CanNm_StateType CurrentNmState()
    {
        CanNm_StateType state = CANNM_STATE_BUS_SLEEP;
        CanNm_ModeType  mode;
        EXPECT_EQ(CanNm_GetState(CANNM_MAIN_NETWORK_HANDLE, &state, &mode), E_OK);
        return state;
    }

    void SetUp() override
    {
        FakeCanHw_Reset();
        WrapCanIf_Reset();  // 他ファイルの故障注入が漏れ伝わらないよう防御的にリセット
        WrapCan_Reset();  // 同上（Can.c 側）
        FakeEcuM_Reset();
        WrapBswM_Reset();
        WrapDem_Reset();
        FakeMillis_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        Dem_Init(NULL);  // Demの内部状態を毎テスト決定的にリセットする（Fake_NvM.cにより常に「初回起動」）

        canConfig.filter.filterId = 0x0220U;
        canConfig.filter.mask     = 0x1FFFU;
        canConfig.csPin           = 10U;
        canConfig.intPin          = 2U;
        canConfig.baudrate        = 500000U;
        canConfig.crystalFreq     = CAN_CRYSTAL_16MHZ;

        Can_Init(&canConfig);
        ASSERT_EQ(Can_Test_GetControllerState(), CAN_CS_STOPPED);  // Can_Init() 直後は STOPPED（Listen-Only）
        CanIf_Init(&kWakeup_PassiveStartupCanIfConfig);
        CanSM_Init(NULL);
        ComM_Init(NULL);
        CanNm_Init(NULL);
        Nm_Init(NULL);

        // ボランタリスリープ済みの状態を Arrange する（docs/can_stack.md のとおり、
        // ウェイクアップ検証は CANSM_STATE_NO_COM からの起床のみを受け付ける）。
        // ComM_Init() 直後の既定値は NO_COM のため、ComM 側は別途要求しなくても
        // この前提と一致する。
        ASSERT_EQ(CanIf_SetControllerMode(0U, CAN_CS_SLEEP), E_OK);
        ASSERT_EQ(Can_Test_GetControllerState(), CAN_CS_SLEEP);
        FakeCanHw_SetModeCount = 0U;

        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;  // DeInit() のログを抑制
        CanNm_DeInit();
        ComM_DeInit();
        CanSM_DeInit();
        CanIf_DeInit();
    }

    /* 現在 ComM が把握しているチャネルモードを返すヘルパー
     * （FakeComM_BusSM_ModeIndicationCount 相当の「まだ通知されていないか」判定に使う）。 */
    static ComM_ModeType CurrentComMode()
    {
        ComM_ModeType mode = 0xFFU;
        EXPECT_EQ(ComM_GetCurrentComMode(COMM_USER_0, &mode), E_OK);
        return mode;
    }

    /* 検証開始状態（CANSM_STATE_WAKEUP_VALIDATING）を Arrange するヘルパー。 */
    void ArrangeValidating()
    {
        FakeCanHw_IsWakeupPendingReturn = CAN_HW_OK;
        Can_MainFunction_Wakeup();
        ASSERT_EQ(Can_Test_GetControllerState(), CAN_CS_STOPPED);
        ASSERT_EQ(CurrentComMode(), static_cast<ComM_ModeType>(COMM_NO_COMMUNICATION));
        ASSERT_EQ(FakeEcuM_RequestRUNCount, 0U);
        FakeCanHw_SetModeCount = 0U;
        FakeEcuM_Reset();
        WrapBswM_Reset();
        WrapDem_Reset();
    }

    Can_ConfigType canConfig;
};

// シナリオごとのフィクスチャ（共通の準備は Bsw_NmStack_Wakeup_PassiveStartup_Base）
/* ウェイクアップの検証成功のあと、誰も通信を要求しない場合。 */
class NmStack_Wakeup_PassiveStartup_Test : public Bsw_NmStack_Wakeup_PassiveStartup_Base {};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(NmStack_Wakeup_PassiveStartup_Test, OK)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FakeCanHw_IsWakeupPendingReturn = CAN_HW_OK;
    FakeCanHw_RxPendingCount = 1U;
    FakeCanHw_RxId  = 0x100U;
    FakeCanHw_RxDlc = 2U;
    FakeCanHw_RxData[0] = 0x12U;
    FakeCanHw_RxData[1] = 0x34U;
    Can_MainFunction_Wakeup();     // SLEEP 中の起床を検出（WAKEUP_VALIDATING）
    Can_MainFunction_Read();       // 起床後の最初の受信
    ASSERT_EQ(CanIf_CheckValidation(ECUM_WKSOURCE_CAN), E_OK);   // 検証成功 → FULL_COM
    Can_MainFunction_Mode();
    ASSERT_EQ(CurrentComMode(), static_cast<ComM_ModeType>(COMM_FULL_COMMUNICATION));
    ASSERT_EQ(CurrentNmState(), CANNM_STATE_REPEAT_MESSAGE);   // パッシブ起動で、Repeat Message State

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    bool sawNormalOperation = false;
    bool sawReadySleep      = false;
    for (int tick = 0; tick < 12; tick++)
    {
        FakeMillis_Value += CANNM_CYCLE_MS;
        CanNm_MainFunction();
        Can_MainFunction_Mode();
        sawNormalOperation = sawNormalOperation || (CurrentNmState() == CANNM_STATE_NORMAL_OPERATION);
        sawReadySleep      = sawReadySleep      || (CurrentNmState() == CANNM_STATE_READY_SLEEP);
    }

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_FALSE(sawNormalOperation);   // ネットワークを要求していないので、Normal Operation State へは進まない
    EXPECT_TRUE(sawReadySleep);
    EXPECT_EQ(CurrentNmState(), CANNM_STATE_BUS_SLEEP);
    EXPECT_EQ(CurrentComMode(), static_cast<ComM_ModeType>(COMM_NO_COMMUNICATION));   // 誰も要求しなければ、スリープへ収束する
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_SLEEP);
}

}  // namespace
