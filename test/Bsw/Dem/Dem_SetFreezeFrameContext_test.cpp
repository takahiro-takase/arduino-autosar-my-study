/**
 * \file    Dem_SetFreezeFrameContext_test.cpp
 * \brief   Dem_SetFreezeFrameContext() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 Dem_test.cpp）。
 *
 *          分割前の共通の説明は、旧 Dem_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Dem.h"
#include "Fake_Det_Hw.h"
#include "Wrap_Dem.h"
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

/* Dem の単体テスト用フィクスチャ。
 * SetUp(): wrap の状態を戻し、Dem_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。範囲外のイベント ID を表す kInvalidEventId も用意する。
 * TearDown(): 未初期化の状態へ戻す。 */
class Dem_SetFreezeFrameContext_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        WrapDem_Reset();  // 他ファイルの故障注入が漏れ伝わらないよう防御的にリセット
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        Dem_Init(NULL);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Dem_Test_ResetInitState();
    }

    static const Dem_EventIdType kInvalidEventId = DEM_EVENT_COUNT;
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Dem_SetFreezeFrameContext_Test, Dem_SetFreezeFrameContext_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dem_Test_ResetInitState();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dem_SetFreezeFrameContext(0U, 0U, 0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_UNINIT);
}

TEST_F(Dem_SetFreezeFrameContext_Test, Dem_SetFreezeFrameContext_OK_ContextIsCapturedWhenEventFails)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dem_SetFreezeFrameContext(1500U, 90U, 2U);
    ASSERT_EQ(Dem_SetEventStatus(0U, DEM_EVENT_STATUS_FAILED), E_OK);
    ASSERT_EQ(Dem_SetEventStatus(0U, DEM_EVENT_STATUS_FAILED), E_OK);   // デバウンス上限 2 で FAILED が確定する
    Dem_FreezeFrameType frame = { 0U, 0U, 0U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Dem_GetFreezeFrameOfEvent(0U, &frame);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_EQ(ret, E_OK);
    EXPECT_EQ(frame.EngineSpeed, 1500U);
    EXPECT_EQ(frame.CoolantTemp, 90U);
    EXPECT_EQ(frame.EngineState, 2U);
}

}  // namespace
