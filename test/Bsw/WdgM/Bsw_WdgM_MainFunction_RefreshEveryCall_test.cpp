/**
 * \file    Bsw_WdgM_MainFunction_RefreshEveryCall_test.cpp
 * \brief   WdgM_MainFunction() が毎回の呼び出しで HW ウォッチドッグをリフレッシュすることの単体テスト
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

/* Bsw_WdgM_MainFunction_RefreshEveryCall_Test のフィクスチャ。
 * SetUp(): 時刻・模擬 Wdg_Hw・DET・Dem の wrap を初期化し、Dem_Init(NULL)・Wdg_Init()・WdgM_Init() を EcuM_Init() と同じ順序で呼ぶ（Init 自体の記録は消す）。 */
class Bsw_WdgM_MainFunction_RefreshEveryCall_Test : public ::testing::Test
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

TEST_F(Bsw_WdgM_MainFunction_RefreshEveryCall_Test, MainFunction_OK_RefreshesHwWatchdogOnEveryCall)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み、リフレッシュ回数は 0）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    WdgM_MainFunction();
    const uint32 afterFirst = FakeWdgHw_RefreshCount;
    WdgM_MainFunction();
    WdgM_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(afterFirst, 1U);                 // 判定サイクルに達していない 1 回目でもリフレッシュする
    EXPECT_EQ(FakeWdgHw_RefreshCount, 3U);     // 呼び出しごとに 1 回
}
