/**
 * \file    CanNm_GetNodeIdentifier_test.cpp
 * \brief   CanNm_GetNodeIdentifier() の単体テスト
 * \details 2026-10-16、モジュールの単体テストは関数ごとにファイルを分ける方針に改め、旧 CanNm_test.cpp（1 ファイルに 3 つのフィクスチャを統合していた）を関数単位へ分割した。
 *          [SWS_CanNm_00219]。直近に受信した NM フレームの送信元ノード ID を返す（未受信なら 0）。範囲外のチャネル・NULL ポインタ・未初期化は E_NOT_OK。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "ComM.h"
#include "CanNm.h"
#include "CanNm_Cfg.h"
#include "Nm.h"
#include "Fake_Millis.h"
#include "Fake_Det_Hw.h"
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

/* CanNm_GetNodeIdentifier() の単体テスト用フィクスチャ。
 * SetUp(): ComM_DeInit()（RxIndication のカスケードを防ぐ）の後、CanNm_Init(NULL) と Nm_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): CanNm_DeInit() で未初期化へ戻す。 */
class CanNm_GetNodeIdentifier_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        /* CanNm_RxIndication() -> ComM_Nm_NetworkStartIndication() の呼び出し
         * （[SWS_CanNm_00127]）は ComM が初期化済みだとカスケードする。
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

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(CanNm_GetNodeIdentifier_Test, GetNodeIdentifier_OK_ReturnsZeroBeforeAnyReception)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 nodeId = 0xFFU;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CanNm_GetNodeIdentifier(CANNM_MAIN_NETWORK_HANDLE, &nodeId);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(nodeId, 0U);
}

TEST_F(CanNm_GetNodeIdentifier_Test, GetNodeIdentifier_OK_ReflectsMostRecentlyReceivedFrame)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    /* 2026-09 追加の ComM_Nm_NetworkStartIndication() 呼び出し
     * （[SWS_CanNm_00127]）により、Bus-Sleep Mode 中の受信は ComM が
     * 初期化済みだとカスケードしうる（CanSM_RequestComMode()/
     * CanNm_NetworkRequest() 経由）が、SetUp() の ComM_DeInit() により本テスト
     * では ComM は必ず未初期化（＝カスケードせず COMM_E_UNINIT の DET 報告
     * のみで即 return）。カスケード時の挙動検証自体は
     * NmStack_SleepCoordination_test.cpp の責務。 */
    uint8 pdu[2] = { 0x00U, 0x2AU };  // CBV=0, sourceNodeId=0x2A
    PduInfoType pduInfo = { pdu, 2U };
    CanNm_RxIndication(0U, &pduInfo);

    uint8 nodeId = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CanNm_GetNodeIdentifier(CANNM_MAIN_NETWORK_HANDLE, &nodeId);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(nodeId, 0x2AU);
}

TEST_F(CanNm_GetNodeIdentifier_Test, GetNodeIdentifier_NG_InvalidChannelReturnsErrorAndReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 nodeId = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CanNm_GetNodeIdentifier(kInvalidChannel, &nodeId);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_INVALID_CHANNEL);
}

TEST_F(CanNm_GetNodeIdentifier_Test, GetNodeIdentifier_NG_NullPointerReturnsErrorAndReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CanNm_GetNodeIdentifier(CANNM_MAIN_NETWORK_HANDLE, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_PARAM_POINTER);
}

TEST_F(CanNm_GetNodeIdentifier_Test, CanNm_GetNodeIdentifier_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanNm_DeInit();
    uint8 nodeId = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CanNm_GetNodeIdentifier(CANNM_MAIN_NETWORK_HANDLE, &nodeId);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_UNINIT);
}

}  // namespace
