/**
 * \file    CanNm_NetworkRequest_test.cpp
 * \brief   CanNm_NetworkRequest() の単体テスト
 * \details 2026-10-16、モジュールの単体テストは関数ごとにファイルを分ける方針に改め、旧 CanNm_test.cpp（1 ファイルに 3 つのフィクスチャを統合していた）を関数単位へ分割した。
 *          チャネルが範囲外（CANNM_E_INVALID_CHANNEL、[SWS_CanNm_00192]）、または未初期化（CANNM_E_UNINIT）のときは E_NOT_OK を返す。
 *          有効なチャネルの正常系は、CanNm_EnterRepeatMessage() 経由で ComM_Nm_NetworkMode() へカスケードする。本ファイルは CanNm.c 単体の検証が
 *          目的で ComM_Init() を呼ばない軽量フィクスチャのため、カスケード後の挙動は NmStack_SleepCoordination_test.cpp の責務とする。
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

/* CanNm_NetworkRequest() の単体テスト用フィクスチャ。
 * SetUp(): ComM_DeInit()（RxIndication のカスケードを防ぐ）の後、CanNm_Init(NULL) と Nm_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): CanNm_DeInit() で未初期化へ戻す。 */
class CanNm_NetworkRequest_Test : public ::testing::Test
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

TEST_F(CanNm_NetworkRequest_Test, NetworkRequest_NG_InvalidChannelReturnsErrorAndReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CanNm_NetworkRequest(kInvalidChannel);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_INVALID_CHANNEL);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);
}

TEST_F(CanNm_NetworkRequest_Test, CanNm_NetworkRequest_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanNm_DeInit();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CanNm_NetworkRequest(CANNM_MAIN_NETWORK_HANDLE);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_UNINIT);
}

}  // namespace
