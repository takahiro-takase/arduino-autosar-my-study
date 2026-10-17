/**
 * \file    ComM_CommunicationAllowed_test.cpp
 * \brief   ComM_CommunicationAllowed() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 ComM_test.cpp）。
 *
 *          分割前の共通の説明は、旧 ComM_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "ComM.h"
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

/**
 * \details AUTOSAR SWS_ComM_00871 準拠のシグネチャで新設。ここでは
 *          Allowed=FALSE のまま保留される経路（CanSM を一切呼ばない）と、
 *          異常系・初期値のみ検証する。「Allowed=TRUE で保留中の要求が
 *          実際に CanSM まで届く」正常系は、他の ComM API と同じ static
 *          共有状態のハザードを避けるため、CanSM/CanIf/Can が安全に
 *          初期化済みの NmStack_SleepCoordination_test.cpp 側で検証する
 *          （下記 ComM_GetRequestedComMode セクション末尾のコメントと同じ理由）。
 */

/* ComM_CommunicationAllowed() の単体テスト用フィクスチャ。
 * SetUp(): ComM_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): ComM_DeInit() で未初期化へ戻す。 */
class ComM_CommunicationAllowedX_Test : public ::testing::Test
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

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(ComM_CommunicationAllowedX_Test, ComM_CommunicationAllowed_NG_UninitializedReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ComM_DeInit();
    FakeDetHw_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ComM_CommunicationAllowed(COMM_CHANNEL_0, TRUE);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_UNINIT);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);
}

TEST_F(ComM_CommunicationAllowedX_Test, ComM_CommunicationAllowed_NG_InvalidChannelReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ComM_CommunicationAllowed(COMM_CHANNEL_COUNT, TRUE);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_WRONG_PARAMETERS);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);
}

}  // namespace
