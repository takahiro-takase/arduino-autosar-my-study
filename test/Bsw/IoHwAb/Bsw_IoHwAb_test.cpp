/**
 * \file    Bsw_IoHwAb_test.cpp
 * \brief   IoHwAb.c（src/Bsw/IoHwAb/IoHwAb.c）の単体テスト（NG系）
 * \details IoHwAb.c 全公開APIの `Det_ReportError()` 呼び出し箇所（NG ケースの
 *          み）を1ファイルにまとめる。各ケースで、報告される ErrorId が
 *          仕様どおり正しい値になっていることを `Fake_Det_Hw.h`
 *          （`FakeDetHw_LastErrorId`）で検証する。
 *
 *          IoHwAb は初期化状態を持たない（`IoHwAb_Init()` は出力初期化のみの
 *          薄い実装、IoHwAb.c 参照）ため、未初期化チェックの検証対象は無い。
 *          `IoHwAb_Button_GetLevel()`/`IoHwAb_Adc_GetValue_mV()` は
 *          `IoHwAb_MainFunction()` が確定した static 変数を返すだけのため、
 *          `IoHwAb_Init()` すら呼ばずに直接検証できる。
 *
 *          GoogleTest の main() は test_main.cpp に集約しているため、
 *          本ファイルでは定義しない。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "IoHwAb.h"
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

/* IoHwAb の単体テスト用フィクスチャ。
 * SetUp(): DET の記録を初期化する。 */
class Bsw_IoHwAb_Test : public ::testing::Test
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

// ------------------------------------------------------------
// IoHwAb_Button_GetLevel()
// ------------------------------------------------------------

TEST_F(Bsw_IoHwAb_Test, IoHwAb_Button_GetLevel_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = IoHwAb_Button_GetLevel(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, 0x01U);  // IoHwAb 独自の IOHWAB_E_PARAM_POINTER（非公開定数、IoHwAb.c 参照）
}

// ------------------------------------------------------------
// IoHwAb_Adc_GetValue_mV()
// ------------------------------------------------------------

TEST_F(Bsw_IoHwAb_Test, IoHwAb_Adc_GetValue_mV_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = IoHwAb_Adc_GetValue_mV(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, 0x01U);  // IoHwAb 独自の IOHWAB_E_PARAM_POINTER（非公開定数、IoHwAb.c 参照）
}

}  // namespace
