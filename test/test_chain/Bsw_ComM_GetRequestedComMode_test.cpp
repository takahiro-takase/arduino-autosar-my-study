/**
 * \file    Bsw_ComM_GetRequestedComMode_test.cpp
 * \brief   ComM_GetRequestedComMode() の単体テスト（GoogleTest / PlatformIO
 *          `[env:native_chain]`）。
 * \details AUTOSAR SWS_ComM_00079 準拠のシグネチャで新設。集約前のユーザ単位
 *          の要求値（`ComM_UserRequest[User]`）をそのまま返す薄いgetterで
 *          あることを検証する。異常系と初期値のみ検証し、
 *          「ComM_RequestComMode() で設定した値を反映する」正常系は
 *          ComM_RequestComMode() 自体のカスケード（CanSM/CanIf/Can）が絡む
 *          static 共有状態のハザードを避けるためあえて省略する（詳細は末尾の
 *          コメント参照）。
 */
#include <gtest/gtest.h>

extern "C" {
#include "ComM.h"
#include "Hal_Det_Hw_fake.h"
}

class Bsw_ComM_GetRequestedComMode_Test : public ::testing::Test
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

    static const ComM_UserHandleType kInvalidUser = COMM_USER_COUNT;
};

TEST_F(Bsw_ComM_GetRequestedComMode_Test, NG_InvalidUserReturnsErrorAndReportsDet)
{
    ComM_ModeType mode = COMM_FULL_COMMUNICATION;

    Std_ReturnType ret = ComM_GetRequestedComMode(kInvalidUser, &mode);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_WRONG_PARAMETERS);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);
}

TEST_F(Bsw_ComM_GetRequestedComMode_Test, NG_NullPointerReturnsErrorAndReportsDet)
{
    Std_ReturnType ret = ComM_GetRequestedComMode(COMM_USER_0, NULL);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_PARAM_POINTER);
}

TEST_F(Bsw_ComM_GetRequestedComMode_Test, NG_UninitializedReturnsErrorAndReportsDet)
{
    ComM_DeInit();
    FakeDetHw_Reset();

    ComM_ModeType mode = COMM_FULL_COMMUNICATION;
    Std_ReturnType ret = ComM_GetRequestedComMode(COMM_USER_0, &mode);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_UNINIT);
}

TEST_F(Bsw_ComM_GetRequestedComMode_Test, OK_ReturnsNoComRightAfterInit)
{
    ComM_ModeType mode = COMM_FULL_COMMUNICATION;

    Std_ReturnType ret = ComM_GetRequestedComMode(COMM_USER_0, &mode);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(mode, COMM_NO_COMMUNICATION);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

/* 「ComM_RequestComMode() で設定した値をそのまま返す」という正常系は、
 * ここでは意図的に検証しない: ComM_RequestComMode(FULL_COM) は
 * ComM_ApplyAggregatedRequest() 経由で実体の CanSM_RequestComMode() →
 * CanIf_SetControllerMode() までカスケードする。CanSM/CanIf/Can は
 * native_chain バイナリ内で他のテストファイル（Bsw_SleepCoordination_
 * test.cpp 等）と static な初期化状態を共有しており、実行順序次第で
 * CanSM が既に初期化済み（かつ実 HW 前提の内部状態）のままこの呼び出しに
 * 到達し、ハングする実害を確認した（実測: 177秒でタイムアウト）。
 * PduR_ConfigPtr 等と同じ「バイナリ全体で共有される static、かつ
 * DeInit で完全には戻らない」既知の危険パターン（Bsw_PduR_
 * SecOCTxConfirmation_test.cpp 冒頭コメント参照）のため、この正常系は
 * 意図的に省略する。 */
