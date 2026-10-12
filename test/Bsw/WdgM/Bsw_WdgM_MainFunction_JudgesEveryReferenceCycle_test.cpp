/**
 * \file    Bsw_WdgM_MainFunction_JudgesEveryReferenceCycle_test.cpp
 * \brief   WdgM_MainFunction() が基準周期（WDGM_SUPERVISION_REFERENCE_CYCLE 回）ごとにだけ判定サイクルを評価することの単体テスト
 * \details WdgM_MainFunction() は WDGM_MAIN_FUNCTION_CYCLE_MS ごとに呼ばれ、毎回 HW ウォッチドッグを
 *          リフレッシュし、WDGM_SUPERVISION_REFERENCE_CYCLE 回に 1 回だけ判定サイクルを評価する
 *          （独自 API の WdgM_TriggerHwWatchdog() を 2026-10 に統合した）。
 *          本番の WdgM_Config (WdgM_PBCfg.c) をそのまま使う。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>
#include "WdgM.h"
#include "WdgM_Cfg.h"
#include "Wdg.h"
#include "Wdg_PBCfg.h"
#include "Fake_Wdg_Hw.h"
#include "Wrap_Dem.h"
#include "Fake_Det_Hw.h"
#include "Fake_Millis.h"

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* ======================================================================
 * Definitions
 * ====================================================================== */

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * Global Variables
 * ====================================================================== */

/* Bsw_WdgM_MainFunction_JudgesEveryReferenceCycle_Test のフィクスチャ。
 * SetUp(): 時刻・模擬 Wdg_Hw・DET・Dem の wrap を初期化し、Dem_Init(NULL)・Wdg_Init()・WdgM_Init() を EcuM_Init() と同じ順序で呼ぶ（Init 自体の記録は消す）。 */
class Bsw_WdgM_MainFunction_JudgesEveryReferenceCycle_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        FakeDetHw_Reset();
        FakeWdgHw_Reset();
        WrapDem_Reset();
        Dem_Init(NULL);
        Wdg_Init(&Wdg_Config);
        WdgM_Init(&WdgM_Config);
        FakeWdgHw_RefreshCount = 0U;
        FakeDetHw_Reset();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Bsw_WdgM_MainFunction_JudgesEveryReferenceCycle_Test, MainFunction_OK_JudgesOnlyOnReferenceCycleCall)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // CheckpointReached を一切呼ばないため、判定サイクルが評価されると Alive Supervision が FAILED になる。
    WdgM_LocalStatusType status = WDGM_LOCAL_STATUS_FAILED;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    for (uint8 i = 0U; i < (uint8)(WDGM_SUPERVISION_REFERENCE_CYCLE - 1U); i++)
    {
        WdgM_MainFunction();
    }
    ASSERT_EQ(WdgM_GetLocalStatus(WDGM_ENTITY_WARNING, &status), E_OK);
    const WdgM_LocalStatusType beforeReferenceCycle = status;

    WdgM_MainFunction();   // 基準周期に達する呼び出し
    ASSERT_EQ(WdgM_GetLocalStatus(WDGM_ENTITY_WARNING, &status), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(beforeReferenceCycle, WDGM_LOCAL_STATUS_OK);   // 基準周期の手前までは評価しない
    EXPECT_EQ(status, WDGM_LOCAL_STATUS_FAILED);             // 基準周期に達した呼び出しで評価する
}
