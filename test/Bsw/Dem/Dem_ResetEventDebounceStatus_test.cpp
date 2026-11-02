/**
 * \file    Dem_ResetEventDebounceStatus_test.cpp
 * \brief   Dem_ResetEventDebounceStatus() の単体テスト
 * \details RESET でデバウンスカウンタを 0 へ戻す（FDC も 0）。FREEZE は、止める対象のタイマが無いので何もしない。どちらも UDS ステータスは変えない。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Dem.h"
#include "Dem_Cfg.h"
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

/* Dem_ResetEventDebounceStatus() の単体テスト用フィクスチャ。
 * SetUp(): wrap を初期化し、Dem_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 *          kMulti は、確定に複数回の報告が要るイベント（DemConf_DemEventParameter_CAN_BUSOFF）、kSingle は、1 回で確定するイベント。
 * TearDown(): 未初期化の状態へ戻す。 */
class Dem_ResetEventDebounceStatus_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        WrapDem_Reset();
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

    static const Dem_EventIdType kMulti  = DemConf_DemEventParameter_CAN_BUSOFF;
    static const Dem_EventIdType kSingle = DemConf_DemEventParameter_BUTTON_STUCK;
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Dem_ResetEventDebounceStatus_Test, Dem_ResetEventDebounceStatus_NG_InvalidEventId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_ResetEventDebounceStatus(DEM_EVENT_COUNT, DEM_DEBOUNCE_STATUS_RESET);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_WRONG_CONFIGURATION);
}

TEST_F(Dem_ResetEventDebounceStatus_Test, Dem_ResetEventDebounceStatus_NG_InvalidResetStatus)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_ResetEventDebounceStatus(kMulti, 0x02U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_PARAM_DATA);
}

// RESET: カウンタを 0 へ戻す（FDC も 0）。UDS ステータスは、変えない
TEST_F(Dem_ResetEventDebounceStatus_Test, Dem_ResetEventDebounceStatus_OK_ResetReturnsCounterToZeroWithoutChangingStatus)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SetEventStatus(kMulti, DEM_EVENT_STATUS_PREFAILED), E_OK);
    Dem_UdsStatusByteType before = 0U;
    ASSERT_EQ(Dem_GetEventUdsStatus(kMulti, &before), E_OK);
    sint8 fdcBefore = 0;
    ASSERT_EQ(Dem_GetFaultDetectionCounter(kMulti, &fdcBefore), E_OK);
    ASSERT_GT(fdcBefore, 0);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_ResetEventDebounceStatus(kMulti, DEM_DEBOUNCE_STATUS_RESET);
    sint8 fdcAfter = 99;
    ASSERT_EQ(Dem_GetFaultDetectionCounter(kMulti, &fdcAfter), E_OK);
    Dem_UdsStatusByteType after = 0xFFU;
    ASSERT_EQ(Dem_GetEventUdsStatus(kMulti, &after), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(fdcAfter, 0);
    EXPECT_EQ(after, before);
}

// 確定済みの故障（TestFailed）は、デバウンスの RESET では消えない（イベントの確定には影響しない）
TEST_F(Dem_ResetEventDebounceStatus_Test, Dem_ResetEventDebounceStatus_OK_ResetDoesNotUndoConfirmedFailure)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SetEventStatus(kSingle, DEM_EVENT_STATUS_FAILED), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_ResetEventDebounceStatus(kSingle, DEM_DEBOUNCE_STATUS_RESET);
    Dem_UdsStatusByteType status = 0U;
    ASSERT_EQ(Dem_GetEventUdsStatus(kSingle, &status), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_NE(status & DEM_STATUS_TEST_FAILED, 0U);
}

// FREEZE: カウンタ式のデバウンスには止めるタイマが無いので、何も変わらない
TEST_F(Dem_ResetEventDebounceStatus_Test, Dem_ResetEventDebounceStatus_OK_FreezeLeavesCounterUnchanged)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SetEventStatus(kMulti, DEM_EVENT_STATUS_PREFAILED), E_OK);
    sint8 fdcBefore = 0;
    ASSERT_EQ(Dem_GetFaultDetectionCounter(kMulti, &fdcBefore), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_ResetEventDebounceStatus(kMulti, DEM_DEBOUNCE_STATUS_FREEZE);
    sint8 fdcAfter = 99;
    ASSERT_EQ(Dem_GetFaultDetectionCounter(kMulti, &fdcAfter), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(fdcAfter, fdcBefore);
}

// Dem_PreInit のあと、すぐに使える API のため、未初期化でも拒否しない（Dem_SetEventStatus() と同じ）
TEST_F(Dem_ResetEventDebounceStatus_Test, Dem_ResetEventDebounceStatus_OK_AvailableBeforeInit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dem_Test_ResetInitState();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_ResetEventDebounceStatus(kMulti, DEM_DEBOUNCE_STATUS_RESET);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
}

}  // namespace
