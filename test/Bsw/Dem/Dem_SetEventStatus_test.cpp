/**
 * \file    Dem_SetEventStatus_test.cpp
 * \brief   Dem_SetEventStatus() の単体テスト
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
class Dem_SetEventStatus_Test : public ::testing::Test
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

TEST_F(Dem_SetEventStatus_Test, Dem_SetEventStatus_NG_WrongConfiguration)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Dem_SetEventStatus(kInvalidEventId, DEM_EVENT_STATUS_FAILED);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_WRONG_CONFIGURATION);
}

TEST_F(Dem_SetEventStatus_Test, Dem_SetEventStatus_NG_ParamData)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    // PASSED / FAILED / PREPASSED / PREFAILED 以外の値は受け付けない
    Std_ReturnType ret = Dem_SetEventStatus(0U, static_cast<Dem_EventStatusType>(4));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_PARAM_DATA);
}

TEST_F(Dem_SetEventStatus_Test, Dem_SetEventStatus_OK_PreFailedConfirmsOnlyAtLimit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み。しきい値は 2）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret1 = Dem_SetEventStatus(0U, DEM_EVENT_STATUS_PREFAILED);
    Dem_MonitorStatusType afterFirst = 0U;
    (void)Dem_GetMonitorStatus(0U, &afterFirst);
    Std_ReturnType ret2 = Dem_SetEventStatus(0U, DEM_EVENT_STATUS_PREFAILED);
    Dem_MonitorStatusType afterSecond = 0U;
    (void)Dem_GetMonitorStatus(0U, &afterSecond);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret1, E_OK);
    EXPECT_EQ(ret2, E_OK);
    EXPECT_EQ(afterFirst & DEM_MONITOR_STATUS_TF, 0U);    // 1 回目: まだ段階的な進行（確定しない）
    EXPECT_NE(afterSecond & DEM_MONITOR_STATUS_TF, 0U);   // 2 回目: しきい値に達して確定
}

TEST_F(Dem_SetEventStatus_Test, Dem_SetEventStatus_OK_FailedConfirmsImmediately)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み。しきい値は 2）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Dem_SetEventStatus(0U, DEM_EVENT_STATUS_FAILED);
    Dem_MonitorStatusType status = 0U;
    (void)Dem_GetMonitorStatus(0U, &status);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_NE(status & DEM_MONITOR_STATUS_TF, 0U);   // 1 回で確定（カウンタが、しきい値へジャンプする）
}

TEST_F(Dem_SetEventStatus_Test, Dem_SetEventStatus_OK_PassedJumpsCounterToPassedThreshold)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SetEventStatus(0U, DEM_EVENT_STATUS_PREFAILED), E_OK);   // 失敗側へ 1 つ進んだ状態
    sint8 fdc = 0;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Dem_SetEventStatus(0U, DEM_EVENT_STATUS_PASSED);
    (void)Dem_GetFaultDetectionCounter(0U, &fdc);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(fdc, -128);   // 成功側のしきい値（-limit）へ、直接ジャンプ
}

TEST_F(Dem_SetEventStatus_Test, Dem_SetEventStatus_OK_PrePassedAfterPreFailedRestartsFromZero)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SetEventStatus(0U, DEM_EVENT_STATUS_PREFAILED), E_OK);   // 失敗側 +1
    sint8 fdc = 0;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Dem_SetEventStatus(0U, DEM_EVENT_STATUS_PREPASSED);
    (void)Dem_GetFaultDetectionCounter(0U, &fdc);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(fdc, -64);   // 0 へ戻してから 1 つ成功側へ（-1。しきい値 2 の半分）
}

TEST_F(Dem_SetEventStatus_Test, Dem_SetEventStatus_OK_PreFailedAfterPrePassedRestartsFromZero)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SetEventStatus(0U, DEM_EVENT_STATUS_PREPASSED), E_OK);   // 成功側 -1
    sint8 fdc = 0;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Dem_SetEventStatus(0U, DEM_EVENT_STATUS_PREFAILED);
    (void)Dem_GetFaultDetectionCounter(0U, &fdc);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(fdc, 63);   // 0 へ戻してから 1 つ失敗側へ（+1。しきい値 2 で線形写像: 1*127/2 = 63）
}

}  // namespace
