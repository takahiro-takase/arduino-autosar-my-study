/**
 * \file    ComM_BusSM_ModeIndication_NmStartup_test.cpp
 * \brief   ComM_BusSM_ModeIndication() の、FULL_COM へ入ったときの Nm の起動方法の単体テスト
 * \details [SWS_ComM_00665]/[SWS_ComM_00903]/[SWS_ComM_00869]。CanSM がユーザの要求とは独立にチャネルを
 *          FULL_COM へ変えたとき（起床など）は、Nm_PassiveStartUp()（READY_SLEEP）、ユーザが FULL_COM を
 *          要求しているときは、Nm_NetworkRequest()（NETWORK_REQUESTED）を呼ぶ。READY_SLEEP のあとで
 *          ユーザが FULL_COM を要求したら、ComM_RequestComMode() が Nm_NetworkRequest() を呼ぶ。
 *          CanSM と Nm は wrap で実体を呼ばずに成功させ、ComM 単体の振る舞いだけを確認する
 *          （ComM_Nm_NetworkStartIndication_test.cpp と同じ構成）。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "ComM.h"
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

/* Nm の起動方法の単体テスト用フィクスチャ。
 * SetUp(): ComM_Init(NULL) を呼び、CanSM と Nm の wrap をリセットして、Nm への要求は実体を呼ばずに成功させる（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): ComM_DeInit() で未初期化へ戻し、wrap をリセットする。 */
class ComM_BusSM_ModeIndication_NmStartup_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        ComM_Init(NULL);
        WrapCanSM_Reset();
        WrapNm_Reset();
        FailFromCallCount_Nm_NetworkRequest = 1U;   // Nm への要求は、実体を呼ばずに成功する（CanNm へカスケードしない）
        ForcedReturn_Nm_NetworkRequest      = E_OK;
        FailFromCallCount_Nm_PassiveStartUp = 1U;
        ForcedReturn_Nm_PassiveStartUp      = E_OK;
        FailFromCallCount_Nm_NetworkRelease = 1U;
        ForcedReturn_Nm_NetworkRelease      = E_OK;
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
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

// 起床など、ユーザの要求とは独立に FULL_COM へ入ったときは、通信を要求せず、パッシブ起動する（READY_SLEEP）
TEST_F(ComM_BusSM_ModeIndication_NmStartup_Test, ComM_BusSM_ModeIndication_OK_FullComWithoutUserRequestStartsNmPassively)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み。誰も通信を要求していない）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ComM_BusSM_ModeIndication(0U, COMM_FULL_COMMUNICATION);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Nm_PassiveStartUp, 1U);
    EXPECT_EQ(CallCount_Nm_NetworkRequest, 0U);
}

// FULL_COM へ入っても、User0 の要求は作らない（誰も要求していないため）
TEST_F(ComM_BusSM_ModeIndication_NmStartup_Test, ComM_BusSM_ModeIndication_OK_FullComDoesNotCreateUserRequest)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ComM_ModeType requested = COMM_FULL_COMMUNICATION;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ComM_BusSM_ModeIndication(0U, COMM_FULL_COMMUNICATION);
    ASSERT_EQ(ComM_GetRequestedComMode(COMM_USER_0, &requested), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(requested, static_cast<ComM_ModeType>(COMM_NO_COMMUNICATION));
}

// ユーザが FULL_COM を要求しているときは、通信を要求する（NETWORK_REQUESTED）
TEST_F(ComM_BusSM_ModeIndication_NmStartup_Test, ComM_BusSM_ModeIndication_OK_FullComWithUserRequestRequestsNetwork)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(ComM_RequestComMode(COMM_USER_0, COMM_FULL_COMMUNICATION), E_OK);   // CommunicationAllowed が FALSE なので、CanSM へは伝えず保留する

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ComM_BusSM_ModeIndication(0U, COMM_FULL_COMMUNICATION);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Nm_NetworkRequest, 1U);
    EXPECT_EQ(CallCount_Nm_PassiveStartUp, 0U);
}

// READY_SLEEP（パッシブ起動のあと）で、ユーザが FULL_COM を要求したら、通信が必要なことを Nm へ伝える
TEST_F(ComM_BusSM_ModeIndication_NmStartup_Test, ComM_RequestComMode_OK_FullComRequestedWhileReadySleepRequestsNetwork)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ComM_BusSM_ModeIndication(0U, COMM_FULL_COMMUNICATION);   // パッシブ起動（READY_SLEEP）
    ASSERT_EQ(CallCount_Nm_NetworkRequest, 0U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = ComM_RequestComMode(COMM_USER_0, COMM_FULL_COMMUNICATION);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(CallCount_Nm_NetworkRequest, 1U);
    EXPECT_EQ(CallCount_CanSM_RequestComMode, 0U);   // チャネルは既に FULL_COM なので、CanSM へは伝えない
}

// 通信を要求済みの状態で、同じ FULL_COM の要求を繰り返しても、Nm へ重ねて伝えない
TEST_F(ComM_BusSM_ModeIndication_NmStartup_Test, ComM_RequestComMode_OK_RepeatedFullComRequestAfterNetworkRequestIsIdempotent)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ComM_BusSM_ModeIndication(0U, COMM_FULL_COMMUNICATION);
    ASSERT_EQ(ComM_RequestComMode(COMM_USER_0, COMM_FULL_COMMUNICATION), E_OK);   // READY_SLEEP → NETWORK_REQUESTED

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = ComM_RequestComMode(COMM_USER_0, COMM_FULL_COMMUNICATION);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(CallCount_Nm_NetworkRequest, 1U);
}

// パッシブ起動のあとで、ユーザが NO_COM を要求しても、協調スリープへの解放（Nm_NetworkRelease）は問題なく伝わる
TEST_F(ComM_BusSM_ModeIndication_NmStartup_Test, ComM_RequestComMode_OK_NoComRequestedAfterPassiveStartReleasesNetwork)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ComM_BusSM_ModeIndication(0U, COMM_FULL_COMMUNICATION);   // パッシブ起動（READY_SLEEP）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = ComM_RequestComMode(COMM_USER_0, COMM_NO_COMMUNICATION);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(CallCount_Nm_NetworkRelease, 1U);
    EXPECT_EQ(CallCount_Nm_NetworkRequest, 0U);
}

}  // namespace
