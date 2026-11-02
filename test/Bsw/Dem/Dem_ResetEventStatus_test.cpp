/**
 * \file    Dem_ResetEventStatus_test.cpp
 * \brief   Dem_ResetEventStatus() の単体テスト
 * \details PASSED を報告せずに、失敗ステータス（TestFailed）と、デバウンスを戻す。この操作サイクルでテスト済みのイベントは拒否する。DTC の設定が無効の間は、何もしない。
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

/* Dem_ResetEventStatus() の単体テスト用フィクスチャ。
 * SetUp(): wrap を初期化し、Dem_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 *          kMulti は、確定に複数回の報告が要るイベント（DemConf_DemEventParameter_CAN_BUSOFF）、kSingle は、1 回で確定するイベント。
 * TearDown(): 未初期化の状態へ戻す。 */
class Dem_ResetEventStatus_Test : public ::testing::Test
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

// [SWS_Dem_00124]: 未初期化チェック（DEM_E_UNINIT）の対象外。未初期化でも、DET へ報告しない
TEST_F(Dem_ResetEventStatus_Test, Dem_ResetEventStatus_OK_NoDetReportBeforeInit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dem_Test_ResetInitState();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    (void)Dem_ResetEventStatus(kMulti);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(Dem_ResetEventStatus_Test, Dem_ResetEventStatus_NG_InvalidEventId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_ResetEventStatus(DEM_EVENT_COUNT);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_WRONG_CONFIGURATION);
}

// この操作サイクルでテスト済み（確定した）のイベントは、E_NOT_OK で拒否し、ステータスを変えない（[SWS_Dem_00638]）
TEST_F(Dem_ResetEventStatus_Test, Dem_ResetEventStatus_NG_AlreadyTestedInThisCycleIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SetEventStatus(kSingle, DEM_EVENT_STATUS_FAILED), E_OK);
    Dem_UdsStatusByteType before = 0U;
    ASSERT_EQ(Dem_GetEventUdsStatus(kSingle, &before), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_ResetEventStatus(kSingle);
    Dem_UdsStatusByteType after = 0xFFU;
    ASSERT_EQ(Dem_GetEventUdsStatus(kSingle, &after), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(after, before);
}

// 未テスト（確定前）のイベント: デバウンスを 0 へ戻す。監視ステータス（TNCTOC）は変えない（[SWS_Dem_00187]）
TEST_F(Dem_ResetEventStatus_Test, Dem_ResetEventStatus_OK_NotYetTestedResetsDebounceAndKeepsMonitorStatus)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    static_assert(DEM_DEBOUNCE_LIMIT_CAN_BUSOFF > 1, "確定に複数回の報告が要るイベントが前提");
    ASSERT_EQ(Dem_SetEventStatus(kMulti, DEM_EVENT_STATUS_PREFAILED), E_OK);
    sint8 fdcBefore = 0;
    ASSERT_EQ(Dem_GetFaultDetectionCounter(kMulti, &fdcBefore), E_OK);
    ASSERT_GT(fdcBefore, 0);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_ResetEventStatus(kMulti);
    sint8 fdcAfter = 99;
    ASSERT_EQ(Dem_GetFaultDetectionCounter(kMulti, &fdcAfter), E_OK);
    Dem_MonitorStatusType monitor = 0U;
    ASSERT_EQ(Dem_GetMonitorStatus(kMulti, &monitor), E_OK);
    Dem_UdsStatusByteType status = 0xFFU;
    ASSERT_EQ(Dem_GetEventUdsStatus(kMulti, &status), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(fdcAfter, 0);
    EXPECT_NE(monitor & DEM_MONITOR_STATUS_TNCTOC, 0U);   // 未テストのまま
    EXPECT_EQ(status & DEM_STATUS_TEST_FAILED, 0U);
}

// DTC の設定が無効の間は、テスト済みのイベントでも、拒否せず、何もしないで E_OK を返す（[SWS_Dem_00079]）
TEST_F(Dem_ResetEventStatus_Test, Dem_ResetEventStatus_OK_DtcSettingDisabledIsOkEvenForTestedEvent)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SetEventStatus(kSingle, DEM_EVENT_STATUS_FAILED), E_OK);   // テスト済み（確定）
    ASSERT_EQ(Dem_DisableDTCSetting(0U), E_OK);
    Dem_UdsStatusByteType before = 0U;
    ASSERT_EQ(Dem_GetEventUdsStatus(kSingle, &before), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_ResetEventStatus(kSingle);
    Dem_UdsStatusByteType after = 0xFFU;
    ASSERT_EQ(Dem_GetEventUdsStatus(kSingle, &after), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(after, before);
}

// DTC の設定が無効の間は、UDS ステータスを更新しないので、デバウンスも戻さず、E_OK を返す（[SWS_Dem_00079]）
TEST_F(Dem_ResetEventStatus_Test, Dem_ResetEventStatus_OK_IgnoredWhileDtcSettingDisabled)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SetEventStatus(kMulti, DEM_EVENT_STATUS_PREFAILED), E_OK);
    ASSERT_EQ(Dem_DisableDTCSetting(0U), E_OK);
    sint8 fdcBefore = 0;
    ASSERT_EQ(Dem_GetFaultDetectionCounter(kMulti, &fdcBefore), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_ResetEventStatus(kMulti);
    sint8 fdcAfter = 99;
    ASSERT_EQ(Dem_GetFaultDetectionCounter(kMulti, &fdcAfter), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(fdcAfter, fdcBefore);
}

}  // namespace
