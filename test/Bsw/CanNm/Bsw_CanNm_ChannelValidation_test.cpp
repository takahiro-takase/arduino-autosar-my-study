/**
 * \file    Bsw_CanNm_ChannelValidation_test.cpp
 * \brief   CanNm の NetworkHandle 引数検証 (CANNM_E_INVALID_CHANNEL) の単体テスト
 *          （GoogleTest / PlatformIO `[env:native_chain]`）。
 *
 * \details 2026-08-30、IF シグネチャは仕様準拠という方針のもと
 *          CanNm_NetworkRequest/NetworkRelease/RepeatMessageRequest/GetState に
 *          NetworkHandleType Channel 引数を追加し、CanSM の
 *          CANSM_E_INVALID_NETWORK_HANDLE と平仄を合わせて
 *          CANNM_E_INVALID_CHANNEL（[SWS_CanNm_00192]）による範囲チェックを
 *          追加した際に新設。/code-review で「新設した検証パスに対する
 *          テストが無い」と指摘され追加した。
 *          CanNm.c 単体（Can/CanIf/CanSM/ComM は不要）で検証できるため、
 *          Bsw_CanNmStack_SleepCoordination_test.cpp より軽量なフィクスチャで足りる。
 */
#include <gtest/gtest.h>

extern "C" {
#include "CanNm.h"
#include "ComM.h"
#include "Nm.h"
#include "Fake_Millis.h"
#include "Fake_Det_Hw.h"
}

class Bsw_CanNm_ChannelValidation_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        /* 2026-09 追加の CanNm_RxIndication() -> ComM_Nm_NetworkStartIndication()
         * 呼び出し（[SWS_CanNm_00127]）は ComM が初期化済みだとカスケードする。
         * 本フィクスチャは意図的に ComM_Init() を呼ばない軽量構成のため、他の
         * テストファイルが ComM を初期化したまま残す可能性
         * （feedback_native_chain_shared_static_hang 参照）を排除するべく、
         * 明示的に ComM_DeInit() で未初期化状態を保証する
         * （未初期化なら DET 報告のみで無害に即 return）。 */
        ComM_DeInit();
        CanNm_Init(NULL);
        Nm_Init(NULL);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        CanNm_DeInit();
    }

    static const NetworkHandleType kInvalidChannel = CANNM_MAIN_NETWORK_HANDLE + 1U;
};

TEST_F(Bsw_CanNm_ChannelValidation_Test, NetworkRequest_NG_InvalidChannelReturnsErrorAndReportsDet)
{
    Std_ReturnType ret = CanNm_NetworkRequest(kInvalidChannel);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_INVALID_CHANNEL);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);
}

/* NetworkRequest の「有効な Channel」正常系は、CanNm_EnterRepeatMessage() 経由で
 * ComM_Nm_NetworkMode() へカスケードする（本ファイルは CanNm.c 単体の検証が
 * 目的のため ComM_Init() を呼ばない軽量フィクスチャであり、意図的に
 * ComM 側は未初期化のまま。カスケード後の挙動検証は
 * Bsw_CanNmStack_SleepCoordination_test.cpp の責務）。そのため本ファイルでは
 * NG（Channel 不正時に即座に拒否される）側のみを検証する。 */

TEST_F(Bsw_CanNm_ChannelValidation_Test, NetworkRelease_NG_InvalidChannelReturnsErrorAndReportsDet)
{
    Std_ReturnType ret = CanNm_NetworkRelease(kInvalidChannel);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_INVALID_CHANNEL);
}

TEST_F(Bsw_CanNm_ChannelValidation_Test, RepeatMessageRequest_NG_InvalidChannelReturnsErrorAndReportsDet)
{
    Std_ReturnType ret = CanNm_RepeatMessageRequest(kInvalidChannel);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_INVALID_CHANNEL);
}

TEST_F(Bsw_CanNm_ChannelValidation_Test, GetState_NG_InvalidChannelReturnsErrorAndReportsDet)
{
    CanNm_StateType state;
    CanNm_ModeType  mode;

    Std_ReturnType ret = CanNm_GetState(kInvalidChannel, &state, &mode);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_INVALID_CHANNEL);
}

TEST_F(Bsw_CanNm_ChannelValidation_Test, GetState_OK_ValidChannelIsAccepted)
{
    CanNm_StateType state;
    CanNm_ModeType  mode;

    Std_ReturnType ret = CanNm_GetState(CANNM_MAIN_NETWORK_HANDLE, &state, &mode);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(Bsw_CanNm_ChannelValidation_Test, DisableCommunication_NG_InvalidChannelReturnsErrorAndReportsDet)
{
    Std_ReturnType ret = CanNm_DisableCommunication(kInvalidChannel);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_INVALID_CHANNEL);
}

TEST_F(Bsw_CanNm_ChannelValidation_Test, EnableCommunication_NG_InvalidChannelReturnsErrorAndReportsDet)
{
    Std_ReturnType ret = CanNm_EnableCommunication(kInvalidChannel);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_INVALID_CHANNEL);
}

TEST_F(Bsw_CanNm_ChannelValidation_Test, DisableCommunication_OK_ValidChannelIsAccepted)
{
    Std_ReturnType ret = CanNm_DisableCommunication(CANNM_MAIN_NETWORK_HANDLE);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(Bsw_CanNm_ChannelValidation_Test, EnableCommunication_OK_ValidChannelIsAccepted)
{
    Std_ReturnType ret = CanNm_EnableCommunication(CANNM_MAIN_NETWORK_HANDLE);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

// ------------------------------------------------------------
// CanNm_GetLocalNodeIdentifier()/CanNm_GetNodeIdentifier()
// （[SWS_CanNm_00220]/[SWS_CanNm_00219] 準拠で新設）
// ------------------------------------------------------------

TEST_F(Bsw_CanNm_ChannelValidation_Test, GetLocalNodeIdentifier_NG_InvalidChannelReturnsErrorAndReportsDet)
{
    uint8 nodeId = 0U;

    Std_ReturnType ret = CanNm_GetLocalNodeIdentifier(kInvalidChannel, &nodeId);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_INVALID_CHANNEL);
}

TEST_F(Bsw_CanNm_ChannelValidation_Test, GetLocalNodeIdentifier_NG_NullPointerReturnsErrorAndReportsDet)
{
    Std_ReturnType ret = CanNm_GetLocalNodeIdentifier(CANNM_MAIN_NETWORK_HANDLE, NULL);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_PARAM_POINTER);
}

TEST_F(Bsw_CanNm_ChannelValidation_Test, GetLocalNodeIdentifier_OK_ReturnsConfiguredSourceNodeId)
{
    uint8 nodeId = 0U;

    Std_ReturnType ret = CanNm_GetLocalNodeIdentifier(CANNM_MAIN_NETWORK_HANDLE, &nodeId);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(nodeId, CANNM_SOURCE_NODE_ID);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(Bsw_CanNm_ChannelValidation_Test, GetNodeIdentifier_NG_InvalidChannelReturnsErrorAndReportsDet)
{
    uint8 nodeId = 0U;

    Std_ReturnType ret = CanNm_GetNodeIdentifier(kInvalidChannel, &nodeId);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_INVALID_CHANNEL);
}

TEST_F(Bsw_CanNm_ChannelValidation_Test, GetNodeIdentifier_NG_NullPointerReturnsErrorAndReportsDet)
{
    Std_ReturnType ret = CanNm_GetNodeIdentifier(CANNM_MAIN_NETWORK_HANDLE, NULL);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_PARAM_POINTER);
}

TEST_F(Bsw_CanNm_ChannelValidation_Test, GetNodeIdentifier_OK_ReturnsZeroBeforeAnyReception)
{
    uint8 nodeId = 0xFFU;

    Std_ReturnType ret = CanNm_GetNodeIdentifier(CANNM_MAIN_NETWORK_HANDLE, &nodeId);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(nodeId, 0U);
}

TEST_F(Bsw_CanNm_ChannelValidation_Test, GetNodeIdentifier_OK_ReflectsMostRecentlyReceivedFrame)
{
    /* 2026-09 追加の ComM_Nm_NetworkStartIndication() 呼び出し
     * （[SWS_CanNm_00127]）により、Bus-Sleep Mode 中の受信は ComM が
     * 初期化済みだとカスケードしうる（CanSM_RequestComMode()/
     * CanNm_NetworkRequest() 経由）が、SetUp() の ComM_DeInit() により本テスト
     * では ComM は必ず未初期化（＝カスケードせず COMM_E_UNINIT の DET 報告
     * のみで即 return）。カスケード時の挙動検証自体は
     * Bsw_CanNmStack_SleepCoordination_test.cpp の責務。 */
    uint8 pdu[2] = { 0x00U, 0x2AU };  // CBV=0, sourceNodeId=0x2A
    PduInfoType pduInfo = { pdu, 2U };
    CanNm_RxIndication(0U, &pduInfo);

    uint8 nodeId = 0U;
    Std_ReturnType ret = CanNm_GetNodeIdentifier(CANNM_MAIN_NETWORK_HANDLE, &nodeId);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(nodeId, 0x2AU);
}
