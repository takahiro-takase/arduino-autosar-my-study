/**
 * \file    WdgM_SetGetMode_test.cpp
 * \brief   WdgM_SetMode()/WdgM_GetMode() の単体テスト
 * \details AUTOSAR SWS_WdgM_00154/SWS_WdgM_00168 準拠のシグネチャで新設。
 *          本プロジェクトは単一の静的コンフィグのみ保持し、有効なモードは
 *          WDGM_MODE_DEFAULT（通常）と WDGM_MODE_POST_RUN（POST_RUN、監視の判定を抑制）
 *          の 2 つだけであることを検証する（範囲外は E_NOT_OK）。
 *          本番の WdgM_Config (WdgM_PBCfg.c) をそのまま使う。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>
#include "WdgM.h"
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

/* WdgM_SetMode() / WdgM_GetMode() の単体テスト用フィクスチャ。
 * SetUp(): 時刻・模擬 Wdg_Hw・DET・Dem の wrap を初期化し、Dem_Init(NULL)・Wdg_Init()・WdgM_Init() を EcuM_Init() と同じ順序で呼ぶ（Init 自体の DET 記録は消す）。 */
class WdgM_SetGetMode_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        FakeDetHw_Reset();
        FakeWdgHw_Reset();
        WrapDem_Reset();
        Dem_Init(NULL);  // Demの内部状態を毎テスト決定的にリセットする（Fake_NvM.cにより常に「初回起動」）
        Wdg_Init(&Wdg_Config);  // WdgIf/Wdg を実体リンクへ切り替えた際に追加（EcuM_Init() と同じ順序）
        WdgM_Init(&WdgM_Config);
        FakeDetHw_Reset();  /* Init 自体が出す DET ログ・記録を後続の検証対象から除く */
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(WdgM_SetGetMode_Test, SetMode_OK_DefaultModeReturnsOk)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = WdgM_SetMode(WDGM_MODE_DEFAULT);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(WdgM_SetGetMode_Test, SetMode_NG_OutOfRangeModeReturnsErrorAndReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = WdgM_SetMode((WdgM_ModeType)WDGM_MODE_COUNT);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, WDGM_E_PARAM_MODE);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);
}

TEST_F(WdgM_SetGetMode_Test, SetMode_NG_UninitializedReturnsErrorAndReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    WdgM_DeInit();
    FakeDetHw_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = WdgM_SetMode(WDGM_MODE_DEFAULT);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, WDGM_E_NO_INIT);
}

TEST_F(WdgM_SetGetMode_Test, GetMode_OK_ReturnsDefaultModeRightAfterInit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    WdgM_ModeType mode = (WdgM_ModeType)0xFFU;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = WdgM_GetMode(&mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(mode, WDGM_MODE_DEFAULT);
}

TEST_F(WdgM_SetGetMode_Test, GetMode_OK_ReflectsPreviousSetMode)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(WdgM_SetMode(WDGM_MODE_DEFAULT), E_OK);

    WdgM_ModeType mode = (WdgM_ModeType)0xFFU;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = WdgM_GetMode(&mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_EQ(ret, E_OK);
    EXPECT_EQ(mode, WDGM_MODE_DEFAULT);
}

TEST_F(WdgM_SetGetMode_Test, SetMode_OK_PostRunModeIsAcceptedAndReflectedByGetMode)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    WdgM_ModeType mode = (WdgM_ModeType)0xFFU;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType setRet = WdgM_SetMode(WDGM_MODE_POST_RUN);
    Std_ReturnType getRet = WdgM_GetMode(&mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(setRet, E_OK);   // WdgIf_SetMode(OFF) が拒否されても、E_OK（HW を実際に無効化できなくてよい）
    ASSERT_EQ(getRet, E_OK);
    EXPECT_EQ(mode, WDGM_MODE_POST_RUN);
}

TEST_F(WdgM_SetGetMode_Test, GetMode_NG_NullPointerReturnsErrorAndReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = WdgM_GetMode(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, WDGM_E_INV_POINTER);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);
}

TEST_F(WdgM_SetGetMode_Test, GetMode_NG_UninitializedReturnsDefaultAndReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    WdgM_DeInit();
    FakeDetHw_Reset();

    WdgM_ModeType mode = (WdgM_ModeType)0xFFU;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = WdgM_GetMode(&mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(mode, WDGM_MODE_DEFAULT);
    EXPECT_EQ(FakeDetHw_LastErrorId, WDGM_E_NO_INIT);
}
