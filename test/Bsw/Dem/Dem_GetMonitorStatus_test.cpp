/**
 * \file    Dem_GetMonitorStatus_test.cpp
 * \brief   Dem_GetMonitorStatus() の単体テスト
 *
 * \details 監視ステータス（[SWS_Dem_91005]）の TestFailed（bit0）と TestNotCompletedThisOperationCycle（bit1）を、
 *          テストの結果（未テスト、失敗、成功）に応じて取得できることと、不正な呼び出しを拒否することを確認する。
 *          bit1 は、UDS ステータスの bit6 に当たり、Dem_GetEventUdsStatus() では隠れるが、本関数では見える。
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
class Dem_GetMonitorStatus_Test : public ::testing::Test
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

// NG: Dem_Init() の前に呼ぶと、DEM_E_UNINIT を報告して E_NOT_OK を返す。
TEST_F(Dem_GetMonitorStatus_Test, Dem_GetMonitorStatus_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dem_Test_ResetInitState();
    Dem_MonitorStatusType monitor = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetMonitorStatus(0U, &monitor);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_UNINIT);
    EXPECT_EQ(FakeDetHw_LastApiId, DEM_API_ID_GET_MONITOR_STATUS);
}

// NG: 範囲外のイベント ID は、E_NOT_OK を返す（[SWS_Dem_01288]）。
TEST_F(Dem_GetMonitorStatus_Test, Dem_GetMonitorStatus_NG_InvalidEventId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dem_MonitorStatusType monitor = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetMonitorStatus(kInvalidEventId, &monitor);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_WRONG_CONFIGURATION);
}

// NG: 出力先が NULL なら、E_NOT_OK を返す。
TEST_F(Dem_GetMonitorStatus_Test, Dem_GetMonitorStatus_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetMonitorStatus(0U, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_PARAM_POINTER);
}

// OK: 初期化直後の（まだテストされていない）イベントは、TestNotCompletedThisOperationCycle だけが立つ。
// UDS ステータスの bit6 は、Dem_GetEventUdsStatus() では隠れるが、本関数では見える。
TEST_F(Dem_GetMonitorStatus_Test, Dem_GetMonitorStatus_OK_NotTestedYetHasTnctocOnly)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dem_MonitorStatusType monitor = 0xFFU;
    Dem_UdsStatusByteType uds = 0xFFU;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetMonitorStatus(DemConf_DemEventParameter_CAN_BUSOFF, &monitor);
    (void)Dem_GetEventUdsStatus(DemConf_DemEventParameter_CAN_BUSOFF, &uds);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(monitor, DEM_MONITOR_STATUS_TNCTOC);
    EXPECT_EQ(uds & DEM_STATUS_NOT_COMPLETED_THIS_CYCLE, 0U);   // UDS ステータスでは、bit6 は隠れている
}

// OK: 失敗を報告すると、TestFailed だけが立つ（テスト済みになるので、TNCTOC は落ちる）。
TEST_F(Dem_GetMonitorStatus_Test, Dem_GetMonitorStatus_OK_FailedHasTfOnly)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    (void)Dem_SetEventStatus(DemConf_DemEventParameter_CAN_BUSOFF, DEM_EVENT_STATUS_FAILED);
    Dem_MonitorStatusType monitor = 0xFFU;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetMonitorStatus(DemConf_DemEventParameter_CAN_BUSOFF, &monitor);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(monitor, DEM_MONITOR_STATUS_TF);
}

// OK: 成功を報告すると、どちらも立たない（テスト済みで、失敗していない）。
TEST_F(Dem_GetMonitorStatus_Test, Dem_GetMonitorStatus_OK_PassedHasNoFlags)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    (void)Dem_SetEventStatus(DemConf_DemEventParameter_CAN_BUSOFF, DEM_EVENT_STATUS_FAILED);
    (void)Dem_SetEventStatus(DemConf_DemEventParameter_CAN_BUSOFF, DEM_EVENT_STATUS_PASSED);
    Dem_MonitorStatusType monitor = 0xFFU;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetMonitorStatus(DemConf_DemEventParameter_CAN_BUSOFF, &monitor);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(monitor, 0U);
}

}  // namespace
