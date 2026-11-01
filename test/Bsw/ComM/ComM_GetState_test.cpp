/**
 * \file    ComM_GetState_test.cpp
 * \brief   ComM_GetState() の単体テスト
 * \details [SWS_ComM_00872]/[SWS_ComM_00674]。チャネルの状態機械の 5 つの状態（サブステートを含む）を返す。
 *          NO_COM の 2 つのサブステートは、ユーザ要求の集約結果と CommunicationAllowed から導く。
 *          CanSM と Nm は wrap で実体を呼ばずに成功させ、ComM 単体の振る舞いだけを確認する
 *          （ComM_BusSM_ModeIndication_NmStartup_test.cpp と同じ構成）。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "ComM.h"
#include "ComM_Cfg.h"
#include "Fake_Det_Hw.h"
#include "Wrap_CanSM.h"
#include "Wrap_Nm.h"
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

/* ComM_GetState() の単体テスト用フィクスチャ。
 * SetUp(): ComM_Init(NULL) を呼び、CanSM と Nm の wrap をリセットして、CanSM と Nm への要求は実体を呼ばずに成功させる（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。範囲外のチャネルを表す kInvalidChannel も用意する。
 * TearDown(): ComM_DeInit() で未初期化へ戻し、wrap をリセットする。 */
class ComM_GetState_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        ComM_Init(NULL);
        WrapCanSM_Reset();
        WrapNm_Reset();
        FailFromCallCount_CanSM_RequestComMode = 1U;
        ForcedReturn_CanSM_RequestComMode      = E_OK;
        FailFromCallCount_Nm_NetworkRequest    = 1U;
        ForcedReturn_Nm_NetworkRequest         = E_OK;
        FailFromCallCount_Nm_PassiveStartUp    = 1U;
        ForcedReturn_Nm_PassiveStartUp         = E_OK;
        FailFromCallCount_Nm_NetworkRelease    = 1U;
        ForcedReturn_Nm_NetworkRelease         = E_OK;
        FakeDetHw_Reset();              // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        ComM_DeInit();
        WrapCanSM_Reset();
        WrapNm_Reset();
    }

    static ComM_StateType StateOf(NetworkHandleType channel)
    {
        ComM_StateType state = 0xFFU;
        EXPECT_EQ(ComM_GetState(channel, &state), E_OK);
        return state;
    }

    static const NetworkHandleType kInvalidChannel = COMM_CHANNEL_COUNT;
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(ComM_GetState_Test, ComM_GetState_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ComM_DeInit();
    ComM_StateType state = 0xFFU;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = ComM_GetState(0U, &state);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_UNINIT);
}

TEST_F(ComM_GetState_Test, ComM_GetState_NG_InvalidChannel)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ComM_StateType state = 0xFFU;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = ComM_GetState(kInvalidChannel, &state);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_WRONG_PARAMETERS);
}

TEST_F(ComM_GetState_Test, ComM_GetState_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = ComM_GetState(0U, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_PARAM_POINTER);
}

// [SWS_ComM_00485]: 初期の状態は NO_COM（サブステートは、保留なし）
TEST_F(ComM_GetState_Test, ComM_GetState_OK_InitialStateIsNoComNoPendingRequest)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const ComM_StateType state = StateOf(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(state, static_cast<ComM_StateType>(COMM_NO_COM_NO_PENDING_REQUEST));
}

// [SWS_ComM_00875]/[SWS_ComM_00895]: FULL_COM が要求され、通信がまだ許可されていなければ、要求が保留中
TEST_F(ComM_GetState_Test, ComM_GetState_OK_NoComRequestPendingWhileCommunicationNotAllowed)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(ComM_RequestComMode(COMM_USER_0, COMM_FULL_COMMUNICATION), E_OK);   // CommunicationAllowed が FALSE なので保留

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const ComM_StateType state = StateOf(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(state, static_cast<ComM_StateType>(COMM_NO_COM_REQUEST_PENDING));
}

// [SWS_ComM_00897]: 要求が取り下げられたら、保留なしへ戻る
TEST_F(ComM_GetState_Test, ComM_GetState_OK_NoComRequestPendingGoesBackWhenRequestIsWithdrawn)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(ComM_RequestComMode(COMM_USER_0, COMM_FULL_COMMUNICATION), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ASSERT_EQ(ComM_RequestComMode(COMM_USER_0, COMM_NO_COMMUNICATION), E_OK);
    const ComM_StateType state = StateOf(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(state, static_cast<ComM_StateType>(COMM_NO_COM_NO_PENDING_REQUEST));
}

// [SWS_ComM_00899]/[SWS_ComM_00869]: ユーザが要求している FULL_COM は NETWORK_REQUESTED
TEST_F(ComM_GetState_Test, ComM_GetState_OK_FullComNetworkRequestedWhenUserRequests)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(ComM_RequestComMode(COMM_USER_0, COMM_FULL_COMMUNICATION), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ComM_BusSM_ModeIndication(0U, COMM_FULL_COMMUNICATION);
    const ComM_StateType state = StateOf(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(state, static_cast<ComM_StateType>(COMM_FULL_COM_NETWORK_REQUESTED));
}

// [SWS_ComM_00665]/[SWS_ComM_00296]: 誰も要求していない起床の FULL_COM は READY_SLEEP
TEST_F(ComM_GetState_Test, ComM_GetState_OK_FullComReadySleepWhenNobodyRequests)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（誰も通信を要求していない）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ComM_BusSM_ModeIndication(0U, COMM_FULL_COMMUNICATION);
    const ComM_StateType state = StateOf(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(state, static_cast<ComM_StateType>(COMM_FULL_COM_READY_SLEEP));
}

// READY_SLEEP のあとで、ユーザが FULL_COM を要求したら、NETWORK_REQUESTED へ
TEST_F(ComM_GetState_Test, ComM_GetState_OK_ReadySleepBecomesNetworkRequestedWhenUserRequests)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ComM_BusSM_ModeIndication(0U, COMM_FULL_COMMUNICATION);   // READY_SLEEP

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ASSERT_EQ(ComM_RequestComMode(COMM_USER_0, COMM_FULL_COMMUNICATION), E_OK);
    const ComM_StateType state = StateOf(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(state, static_cast<ComM_StateType>(COMM_FULL_COM_NETWORK_REQUESTED));
}

// [SWS_ComM_00882]/[SWS_ComM_00883]: ユーザが解放したら、FULL_COM のまま READY_SLEEP（協調スリープ待ち）
TEST_F(ComM_GetState_Test, ComM_GetState_OK_FullComReadySleepAfterUserReleases)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(ComM_RequestComMode(COMM_USER_0, COMM_FULL_COMMUNICATION), E_OK);
    ComM_BusSM_ModeIndication(0U, COMM_FULL_COMMUNICATION);   // NETWORK_REQUESTED

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ASSERT_EQ(ComM_RequestComMode(COMM_USER_0, COMM_NO_COMMUNICATION), E_OK);
    const ComM_StateType state = StateOf(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(state, static_cast<ComM_StateType>(COMM_FULL_COM_READY_SLEEP));
}

// SILENT_COM（Prepare Bus-Sleep、Bus-Off の回復中）
TEST_F(ComM_GetState_Test, ComM_GetState_OK_SilentCom)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ComM_BusSM_ModeIndication(0U, COMM_FULL_COMMUNICATION);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ComM_BusSM_ModeIndication(0U, COMM_SILENT_COMMUNICATION);
    const ComM_StateType state = StateOf(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(state, static_cast<ComM_StateType>(COMM_SILENT_COM));
}

// [SWS_ComM_00295]/[SWS_ComM_00637]: Bus-Sleep のあとは、NO_COM（保留なし）
TEST_F(ComM_GetState_Test, ComM_GetState_OK_BackToNoComNoPendingRequestAfterBusSleep)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ComM_BusSM_ModeIndication(0U, COMM_FULL_COMMUNICATION);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ComM_BusSM_ModeIndication(0U, COMM_NO_COMMUNICATION);
    const ComM_StateType state = StateOf(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(state, static_cast<ComM_StateType>(COMM_NO_COM_NO_PENDING_REQUEST));
}

// CommunicationAllowed が TRUE でも、CanSM が FULL_COM の要求を断った間は、NO_COM のまま要求が保留中
TEST_F(ComM_GetState_Test, ComM_GetState_OK_NoComRequestPendingWhileCanSmRefusesAllowedRequest)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ComM_CommunicationAllowed(0U, TRUE);
    FailFromCallCount_CanSM_RequestComMode = 1U;   // CanSM が、要求を断る
    ForcedReturn_CanSM_RequestComMode      = E_NOT_OK;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ASSERT_EQ(ComM_RequestComMode(COMM_USER_0, COMM_FULL_COMMUNICATION), E_NOT_OK);
    const ComM_StateType state = StateOf(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(state, static_cast<ComM_StateType>(COMM_NO_COM_REQUEST_PENDING));
}

// [SWS_ComM_00876]: Dcm の診断が始まると、仮想のユーザの要求で、保留中になる
TEST_F(ComM_GetState_Test, ComM_GetState_OK_NoComRequestPendingByDcmActiveDiagnostic)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（CommunicationAllowed は FALSE のまま）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ComM_DCM_ActiveDiagnostic(0U);
    const ComM_StateType state = StateOf(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(state, static_cast<ComM_StateType>(COMM_NO_COM_REQUEST_PENDING));
}

// Bus-Off（SILENT_COM）のあとの回復で、ユーザが FULL_COM を要求していれば、NETWORK_REQUESTED へ戻る
TEST_F(ComM_GetState_Test, ComM_GetState_OK_BusOffRecoveryReturnsToNetworkRequestedWhenUserHolds)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(ComM_RequestComMode(COMM_USER_0, COMM_FULL_COMMUNICATION), E_OK);
    ComM_BusSM_ModeIndication(0U, COMM_FULL_COMMUNICATION);
    ComM_BusSM_ModeIndication(0U, COMM_SILENT_COMMUNICATION);   // Bus-Off。Nm へ解放を送る
    ASSERT_EQ(StateOf(0U), static_cast<ComM_StateType>(COMM_SILENT_COM));

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ComM_BusSM_ModeIndication(0U, COMM_FULL_COMMUNICATION);   // 回復
    const ComM_StateType state = StateOf(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(state, static_cast<ComM_StateType>(COMM_FULL_COM_NETWORK_REQUESTED));
}

// Bus-Off（SILENT_COM）の最中に、ユーザが FULL_COM を要求したら、回復のあとで NETWORK_REQUESTED になる
TEST_F(ComM_GetState_Test, ComM_GetState_OK_UserRequestDuringBusOffIsReflectedAfterRecovery)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ComM_BusSM_ModeIndication(0U, COMM_FULL_COMMUNICATION);   // 誰も要求していない起床（READY_SLEEP）
    ComM_BusSM_ModeIndication(0U, COMM_SILENT_COMMUNICATION);
    ASSERT_EQ(ComM_RequestComMode(COMM_USER_0, COMM_FULL_COMMUNICATION), E_OK);   // SILENT_COM の最中

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ComM_BusSM_ModeIndication(0U, COMM_FULL_COMMUNICATION);   // 回復
    const ComM_StateType state = StateOf(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(state, static_cast<ComM_StateType>(COMM_FULL_COM_NETWORK_REQUESTED));
}

// 協調スリープ待ち（解放済み）のまま、Bus-Off から回復しても、READY_SLEEP のまま（通信は要求しない）
TEST_F(ComM_GetState_Test, ComM_GetState_OK_BusOffRecoveryWithReleasePendingStaysReadySleep)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(ComM_RequestComMode(COMM_USER_0, COMM_FULL_COMMUNICATION), E_OK);
    ComM_BusSM_ModeIndication(0U, COMM_FULL_COMMUNICATION);          // NETWORK_REQUESTED
    ASSERT_EQ(ComM_RequestComMode(COMM_USER_0, COMM_NO_COMMUNICATION), E_OK);   // 解放を送って、待つ
    ComM_BusSM_ModeIndication(0U, COMM_SILENT_COMMUNICATION);        // Bus-Off

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ComM_BusSM_ModeIndication(0U, COMM_FULL_COMMUNICATION);          // 回復
    const ComM_StateType state = StateOf(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(state, static_cast<ComM_StateType>(COMM_FULL_COM_READY_SLEEP));
    EXPECT_EQ(CallCount_Nm_NetworkRequest, 1U);   // 最初の要求の 1 回だけ（回復で、重ねて要求しない）
}

}  // namespace
