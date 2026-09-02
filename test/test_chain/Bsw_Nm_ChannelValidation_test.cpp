/**
 * \file    Bsw_Nm_ChannelValidation_test.cpp
 * \brief   Nm の NetworkHandle 引数検証 (NM_E_INVALID_CHANNEL) の単体テスト
 *          （GoogleTest / PlatformIO `[env:native_chain]`）。
 *
 * \details 2026-08-30、IF シグネチャは仕様準拠という方針のもと
 *          Nm_NetworkRequest/NetworkRelease/RepeatMessageRequest/GetState に
 *          NetworkHandleType Channel 引数を追加し、CanSM の
 *          CANSM_E_INVALID_NETWORK_HANDLE と平仄を合わせて
 *          NM_E_INVALID_CHANNEL（[SWS_CanNm_00192]）による範囲チェックを
 *          追加した際に新設。/code-review で「新設した検証パスに対する
 *          テストが無い」と指摘され追加した。
 *          Nm.c 単体（Can/CanIf/CanSM/ComM は不要）で検証できるため、
 *          Bsw_SleepCoordination_test.cpp より軽量なフィクスチャで足りる。
 */
#include <gtest/gtest.h>

extern "C" {
#include "Nm.h"
#include "Hal_Millis_fake.h"
#include "Hal_Det_Hw_fake.h"
}

class Bsw_Nm_ChannelValidation_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        Nm_Init(NULL);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Nm_DeInit();
    }

    static const NetworkHandleType kInvalidChannel = NM_MAIN_NETWORK_HANDLE + 1U;
};

TEST_F(Bsw_Nm_ChannelValidation_Test, NetworkRequest_NG_InvalidChannelReturnsErrorAndReportsDet)
{
    Std_ReturnType ret = Nm_NetworkRequest(kInvalidChannel);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, NM_E_INVALID_CHANNEL);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);
}

/* NetworkRequest の「有効な Channel」正常系は、Nm_EnterRepeatMessage() 経由で
 * ComM_Nm_NetworkMode() へカスケードする（本ファイルは Nm.c 単体の検証が
 * 目的のため ComM_Init() を呼ばない軽量フィクスチャであり、意図的に
 * ComM 側は未初期化のまま。カスケード後の挙動検証は
 * Bsw_SleepCoordination_test.cpp の責務）。そのため本ファイルでは
 * NG（Channel 不正時に即座に拒否される）側のみを検証する。 */

TEST_F(Bsw_Nm_ChannelValidation_Test, NetworkRelease_NG_InvalidChannelReturnsErrorAndReportsDet)
{
    Std_ReturnType ret = Nm_NetworkRelease(kInvalidChannel);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, NM_E_INVALID_CHANNEL);
}

TEST_F(Bsw_Nm_ChannelValidation_Test, RepeatMessageRequest_NG_InvalidChannelReturnsErrorAndReportsDet)
{
    Std_ReturnType ret = Nm_RepeatMessageRequest(kInvalidChannel);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, NM_E_INVALID_CHANNEL);
}

TEST_F(Bsw_Nm_ChannelValidation_Test, GetState_NG_InvalidChannelReturnsErrorAndReportsDet)
{
    Nm_StateType state;
    Nm_ModeType  mode;

    Std_ReturnType ret = Nm_GetState(kInvalidChannel, &state, &mode);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, NM_E_INVALID_CHANNEL);
}

TEST_F(Bsw_Nm_ChannelValidation_Test, GetState_OK_ValidChannelIsAccepted)
{
    Nm_StateType state;
    Nm_ModeType  mode;

    Std_ReturnType ret = Nm_GetState(NM_MAIN_NETWORK_HANDLE, &state, &mode);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(Bsw_Nm_ChannelValidation_Test, DisableCommunication_NG_InvalidChannelReturnsErrorAndReportsDet)
{
    Std_ReturnType ret = Nm_DisableCommunication(kInvalidChannel);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, NM_E_INVALID_CHANNEL);
}

TEST_F(Bsw_Nm_ChannelValidation_Test, EnableCommunication_NG_InvalidChannelReturnsErrorAndReportsDet)
{
    Std_ReturnType ret = Nm_EnableCommunication(kInvalidChannel);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, NM_E_INVALID_CHANNEL);
}

TEST_F(Bsw_Nm_ChannelValidation_Test, DisableCommunication_OK_ValidChannelIsAccepted)
{
    Std_ReturnType ret = Nm_DisableCommunication(NM_MAIN_NETWORK_HANDLE);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(Bsw_Nm_ChannelValidation_Test, EnableCommunication_OK_ValidChannelIsAccepted)
{
    Std_ReturnType ret = Nm_EnableCommunication(NM_MAIN_NETWORK_HANDLE);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}
