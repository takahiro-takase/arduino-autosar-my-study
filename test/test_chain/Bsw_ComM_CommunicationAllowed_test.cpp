/**
 * \file    Bsw_ComM_CommunicationAllowed_test.cpp
 * \brief   ComM_CommunicationAllowed() の単体テスト（GoogleTest / PlatformIO
 *          `[env:native_chain]`）。
 * \details AUTOSAR SWS_ComM_00871 準拠のシグネチャで新設。ここでは
 *          Allowed=FALSE のまま保留される経路（CanSM を一切呼ばない）と、
 *          異常系・初期値のみ検証する。「Allowed=TRUE で保留中の要求が
 *          実際に CanSM まで届く」正常系は、ComM_RequestComMode.md 等と同じ
 *          static 共有状態のハザードを避けるため、CanSM/CanIf/Can が
 *          安全に初期化済みの Bsw_SleepCoordination_test.cpp 側で検証する
 *          （Bsw_ComM_GetRequestedComMode_test.cpp 末尾のコメントと同じ理由）。
 */
#include <gtest/gtest.h>

extern "C" {
#include "ComM.h"
#include "Hal_Det_Hw_fake.h"
}

class Bsw_ComM_CommunicationAllowed_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        ComM_Init(NULL);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        ComM_DeInit();
    }
};

TEST_F(Bsw_ComM_CommunicationAllowed_Test, NG_InvalidChannelReportsDet)
{
    ComM_CommunicationAllowed(COMM_CHANNEL_COUNT, TRUE);

    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_WRONG_PARAMETERS);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);
}

TEST_F(Bsw_ComM_CommunicationAllowed_Test, NG_UninitializedReportsDet)
{
    ComM_DeInit();
    FakeDetHw_Reset();

    ComM_CommunicationAllowed(COMM_CHANNEL_0, TRUE);

    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_UNINIT);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);
}

/**
 * \brief   [SWS_ComM_00884]: ComM_Init() 直後の既定値は FALSE。この状態で
 *          ユーザが FULL_COM を要求しても E_OK を返す（保留として受理する）が、
 *          CanSM へは一切転送しないためチャネルは NO_COM のまま変化しない。
 *          CanSM/CanIf/Can はこのフィクスチャでは未初期化のままだが、
 *          保留経路はそれらを一切呼ばないため安全に検証できる。
 */
TEST_F(Bsw_ComM_CommunicationAllowed_Test, OK_DefaultIsFalseAfterInitAndHoldsFullComRequest)
{
    Std_ReturnType ret = ComM_RequestComMode(COMM_USER_0, COMM_FULL_COMMUNICATION);

    EXPECT_EQ(ret, E_OK);
    ComM_ModeType mode = COMM_FULL_COMMUNICATION;
    ASSERT_EQ(ComM_GetCurrentComMode(COMM_USER_0, &mode), E_OK);
    EXPECT_EQ(mode, static_cast<ComM_ModeType>(COMM_NO_COMMUNICATION));
}

/**
 * \brief   Allowed=FALSE のまま（保留中の要求もない）場合は DET も CanSM 転送も
 *          起きず、単にフラグが記録されるだけであること。
 */
TEST_F(Bsw_ComM_CommunicationAllowed_Test, OK_ExplicitFalseReportsNoDet)
{
    ComM_CommunicationAllowed(COMM_CHANNEL_0, FALSE);

    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

/**
 * \brief   /code-review で発見した回帰の防止: Allowed=FALSE 中に FULL_COM を
 *          要求した直後、同じユーザが NO_COM へ要求し直して撤回した場合、
 *          後から Allowed=TRUE が通知されても、もう誰も望んでいない FULL_COM
 *          へ勝手に「復活」してはならない。CanSM/CanIf/Can はこのフィクスチャ
 *          では未初期化のままだが、正しく修正されていれば CanSM は一切
 *          呼ばれないため安全に検証できる（誤って呼ばれれば未初期化アクセスで
 *          落ちるはずなので、その意味でも回帰検出になる）。
 */
TEST_F(Bsw_ComM_CommunicationAllowed_Test, OK_AbandonedPendingRequestDoesNotResurrectOnLaterAllow)
{
    ComM_CommunicationAllowed(COMM_CHANNEL_0, FALSE);
    ASSERT_EQ(ComM_RequestComMode(COMM_USER_0, COMM_FULL_COMMUNICATION), E_OK);
    ASSERT_EQ(ComM_RequestComMode(COMM_USER_0, COMM_NO_COMMUNICATION), E_OK);

    /* 実行 (Act) */
    ComM_CommunicationAllowed(COMM_CHANNEL_0, TRUE);

    /* 評価 (Assert) */
    ComM_ModeType mode = COMM_FULL_COMMUNICATION;
    ASSERT_EQ(ComM_GetCurrentComMode(COMM_USER_0, &mode), E_OK);
    EXPECT_EQ(mode, static_cast<ComM_ModeType>(COMM_NO_COMMUNICATION));
}
