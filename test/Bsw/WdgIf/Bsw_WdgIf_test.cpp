/**
 * \file    Bsw_WdgIf_test.cpp
 * \brief   WdgIf.c（src/Bsw/WdgIf/WdgIf.c）の単体テスト（NG系）
 * \details WdgIf.c は `WdgIf_CheckDevice()`（1箇所の `Det_ReportError()`
 *          呼び出し）を SetMode/SetTriggerCondition の2エントリポイントから
 *          呼ぶ薄い層のため、ソース上の `Det_ReportError()` 呼び出し箇所は
 *          2箇所（本関数＋GetVersionInfo）のみだが、各エントリポイントで
 *          報告される ApiId が正しい値になっていることを個別に検証する
 *          （`Fake_Det_Hw.h`の`FakeDetHw_LastErrorId`/`FakeDetHw_LastApiId`）。
 *
 *          WdgIf 自体は初期化状態を持たない（実 AUTOSAR の WdgIf に Init が
 *          存在しないのと同じ、WdgIf.c 冒頭コメント参照）ため、未初期化
 *          チェックの検証対象は無い。下位層 Wdg は未初期化のままで構わない
 *          （`WdgIf_SetMode`/`SetTriggerCondition` の Device 検証は
 *          `Wdg_SetMode`/`Wdg_SetTriggerCondition` 呼び出し前に reject する
 *          ため到達しない）。
 *
 *          GoogleTest の main() は test_main.cpp に集約しているため、
 *          本ファイルでは定義しない。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "WdgIf.h"
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

const WdgIf_DeviceType kInvalidDevice = WDGIF_DEVICE_0 + 1U;

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* WdgIf の単体テスト用フィクスチャ。
 * SetUp(): DET の記録を初期化する。 */
class Bsw_WdgIf_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_Reset();
    }
};

// ------------------------------------------------------------
// WdgIf_SetMode()
// ------------------------------------------------------------

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Bsw_WdgIf_Test, WdgIf_SetMode_NG_InvalidDevice)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = WdgIf_SetMode(kInvalidDevice, WDGIF_FAST_MODE);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, WDGIF_E_PARAM_DEVICE);
    EXPECT_EQ(FakeDetHw_LastApiId, WDGIF_API_ID_SET_MODE);
}

// ------------------------------------------------------------
// WdgIf_SetTriggerCondition()
// ------------------------------------------------------------

TEST_F(Bsw_WdgIf_Test, WdgIf_SetTriggerCondition_NG_InvalidDevice)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    WdgIf_SetTriggerCondition(kInvalidDevice, 100U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, WDGIF_E_PARAM_DEVICE);
    EXPECT_EQ(FakeDetHw_LastApiId, WDGIF_API_ID_SET_TRIGGER_CONDITION);
}

// ------------------------------------------------------------
// WdgIf_GetVersionInfo()
// ------------------------------------------------------------

TEST_F(Bsw_WdgIf_Test, WdgIf_GetVersionInfo_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    WdgIf_GetVersionInfo(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, WDGIF_E_PARAM_POINTER);
    EXPECT_EQ(FakeDetHw_LastApiId, WDGIF_API_ID_GET_VERSION_INFO);
}

}  // namespace
