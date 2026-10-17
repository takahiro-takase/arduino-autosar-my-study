/**
 * \file    WdgM_CheckpointReached_test.cpp
 * \brief   WdgM_CheckpointReached() の Logical / Deadline 監視の単体テスト
 * \details 許可されていない遷移（Logical Supervision）は即座に EXPIRED にする。Deadline Supervision は、前回のチェックポイントからの経過時間が
 *          許容範囲（下限未満・上限超過のどちらも）を外れたときに EXPIRED にし、範囲内なら OK のままにする。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "WdgM.h"
#include "WdgM_TestUtil.h"
#include "Wdg.h"
#include "Wdg_PBCfg.h"
#include "Fake_Wdg_Hw.h"
#include "Wrap_Dem.h"
#include "Fake_Det_Hw.h"
#include "Fake_Millis.h"
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

/* WdgM_CheckpointReached() の単体テスト用フィクスチャ。
 * SetUp(): 時刻・模擬 Wdg_Hw・DET・Dem の wrap を初期化し、Dem_Init(NULL)・Wdg_Init()・WdgM_Init() を EcuM_Init() と同じ順序で呼ぶ（Init 自体の記録は消す）。
 *          補助関数: ViolateEngineDeadline()（ENGINE の Deadline 違反を作る）、DriveToStoppedByEngineDeadline()（その違反が原因で STOPPED まで進める）。 */
class WdgM_CheckpointReached_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        FakeDetHw_Reset();
        FakeWdgHw_Reset();
        WrapDem_Reset();
        Dem_Init(NULL);  // Demの内部状態を毎テスト決定的にリセットする（Fake_NvM.cにより常に「初回起動」）
        Wdg_Init(&Wdg_Config);
        WdgM_Init(&WdgM_Config);
        FakeWdgHw_RefreshCount = 0U;
        FakeDetHw_Reset();
    }

    /* WARNING の Local Status を読む。 */
    static WdgM_LocalStatusType WarningStatus()
    {
        WdgM_LocalStatusType status = WDGM_LOCAL_STATUS_OK;
        (void)WdgM_GetLocalStatus(WDGM_ENTITY_WARNING, &status);
        return status;
    }

    /* ENGINE の START→END を、Deadline の上限（500ms）を超える間隔で報告して、Deadline 違反（EXPIRED）にする。 */
    static void ViolateEngineDeadline()
    {
        WdgM_CheckpointReached(WDGM_ENTITY_ENGINE, WDGM_CP_ENGINE_START);
        FakeMillis_Value += WDGM_ENGINE_DEADLINE_START_TO_END_MAX_MS + 1UL;
        WdgM_CheckpointReached(WDGM_ENTITY_ENGINE, WDGM_CP_ENGINE_END);
    }

    /* 判定サイクル 1 回分。ENGINE は毎サイクル 1 回（START/END を交互に）報告して Alive を満たす。WARNING は何も報告しない。 */
    uint8 engineNextIsStart = 0U;   // 直前の ViolateEngineDeadline() の続き（END の次は START）
    void RunCycleKeepingEngineAlive()
    {
        FakeMillis_Value += 3000UL;
        WdgM_CheckpointReached(WDGM_ENTITY_ENGINE, (engineNextIsStart != 0U) ? WDGM_CP_ENGINE_START : WDGM_CP_ENGINE_END);
        engineNextIsStart = (uint8)(engineNextIsStart == 0U ? 1U : 0U);
        WdgM_RunSupervisionCycle();
    }

    /* Deadline 違反のエンティティ（ENGINE）が原因で、Global Supervision Status を STOPPED まで進める。 */
    void DriveToStoppedByEngineDeadline()
    {
        ViolateEngineDeadline();
        engineNextIsStart = 1U;
        for (uint8 cycle = 0U; cycle < 8U; cycle++)
        {
            RunCycleKeepingEngineAlive();
            WdgM_GlobalStatusType global = WDGM_GLOBAL_STATUS_OK;
            (void)WdgM_GetGlobalStatus(&global);
            if (global == WDGM_GLOBAL_STATUS_STOPPED)
            {
                return;
            }
        }
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(WdgM_CheckpointReached_Test, CheckpointReached_NG_UnexpectedTransitionExpiresLogicalStatus)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    WdgM_CheckpointReached(WDGM_ENTITY_WARNING, WDGM_CP_WARNING_START);   // INITIAL -> START（許可）
    WdgM_CheckpointReached(WDGM_ENTITY_WARNING, WDGM_CP_WARNING_START);   // START -> START（許可されていない）

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(WarningStatus(), WDGM_LOCAL_STATUS_EXPIRED);
}

TEST_F(WdgM_CheckpointReached_Test, CheckpointReached_NG_DeadlineAboveMaxExpiresLocalStatus)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    WdgM_CheckpointReached(WDGM_ENTITY_WARNING, WDGM_CP_WARNING_START);   // INITIAL -> START（Deadline の対象外）
    FakeMillis_Value += WDGM_WARNING_DEADLINE_START_TO_END_MAX_MS + 1UL;       // START -> END の上限（200ms）を超える

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    WdgM_CheckpointReached(WDGM_ENTITY_WARNING, WDGM_CP_WARNING_END);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(WarningStatus(), WDGM_LOCAL_STATUS_EXPIRED);   // Logical は許可された遷移のため、Deadline が原因
}

TEST_F(WdgM_CheckpointReached_Test, CheckpointReached_NG_DeadlineBelowMinExpiresLocalStatus)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    WdgM_CheckpointReached(WDGM_ENTITY_WARNING, WDGM_CP_WARNING_START);
    FakeMillis_Value += 50UL;
    WdgM_CheckpointReached(WDGM_ENTITY_WARNING, WDGM_CP_WARNING_END);         // START -> END: 範囲内
    ASSERT_EQ(WarningStatus(), WDGM_LOCAL_STATUS_OK);
    FakeMillis_Value += WDGM_WARNING_DEADLINE_END_TO_START_MIN_MS - 1UL;       // END -> START の下限（300ms）に満たない

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    WdgM_CheckpointReached(WDGM_ENTITY_WARNING, WDGM_CP_WARNING_START);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(WarningStatus(), WDGM_LOCAL_STATUS_EXPIRED);   // Logical は許可された遷移のため、Deadline が原因
}

TEST_F(WdgM_CheckpointReached_Test, CheckpointReached_NG_UninitializedReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    WdgM_DeInit();   // 未初期化にする
    FakeDetHw_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = WdgM_CheckpointReached(WDGM_ENTITY_WARNING, WDGM_CP_WARNING_START);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, WDGM_E_NO_INIT);
}

TEST_F(WdgM_CheckpointReached_Test, CheckpointReached_NG_InvalidSupervisedEntityIdReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FakeDetHw_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = WdgM_CheckpointReached(0xFFU, WDGM_CP_WARNING_START);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, WDGM_E_PARAM_SEID);
}

TEST_F(WdgM_CheckpointReached_Test, CheckpointReached_OK_DeadlineWithinRangeKeepsStatusOk)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    WdgM_CheckpointReached(WDGM_ENTITY_WARNING, WDGM_CP_WARNING_START);
    FakeMillis_Value += WDGM_WARNING_DEADLINE_START_TO_END_MAX_MS;             // 上限ちょうど（範囲内）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    WdgM_CheckpointReached(WDGM_ENTITY_WARNING, WDGM_CP_WARNING_END);
    FakeMillis_Value += WDGM_WARNING_DEADLINE_END_TO_START_MIN_MS;             // 下限ちょうど（範囲内）
    WdgM_CheckpointReached(WDGM_ENTITY_WARNING, WDGM_CP_WARNING_START);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(WarningStatus(), WDGM_LOCAL_STATUS_OK);
}

}  // namespace
