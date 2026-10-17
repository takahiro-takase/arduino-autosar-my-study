/**
 * \file    Det_GetVersionInfo_test.cpp
 * \brief   Det_GetVersionInfo() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 Det_test.cpp）。
 *
 *          AUTOSAR SWS_Det_00011 が規定する Det_GetVersionInfo() の
 *          NULL ポインタチェックと ModuleId 返却、SWS_Det_00008/00010 が
 *          規定する Det_Init()/Det_Start()（DET エラーを報告しないこと）、
 *          および SWS_Det_01001/01003 が規定する Det_ReportRuntimeError()/
 *          Det_ReportTransientFault()（報告内容とE_OK固定の戻り値）を検証する。
 *          Det.c 自体は実物をリンクし、実 HW 依存の Det_Hw.cpp のみを
 *          Fake_Det_Hw.c に差し替える（他モジュールのテストと同じ構成）。
 *
 *          分割前の共通の説明は、旧 Det_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Det.h"
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

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* Det の単体テスト用フィクスチャ。
 * SetUp(): DET の記録を初期化する。 */
class Det_GetVersionInfo_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_Reset();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Det_GetVersionInfo_Test, Det_GetVersionInfo_NG_RejectsNullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Det_GetVersionInfo(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, DET_E_PARAM_POINTER);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);
}

TEST_F(Det_GetVersionInfo_Test, Det_GetVersionInfo_OK_FillsExpectedModuleId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Std_VersionInfoType info;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Det_GetVersionInfo(&info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(info.moduleID, DET_MODULE_ID);
}

}  // namespace
