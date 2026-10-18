/**
 * \file    IoHwAb_Adc_GetValue_mV_test.cpp
 * \brief   IoHwAb_Adc_GetValue_mV() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 IoHwAb_test.cpp）。
 *
 *          IoHwAb は初期化状態を持たない（`IoHwAb_Init()` は出力初期化のみの
 *          薄い実装、IoHwAb.c 参照）ため、未初期化チェックの検証対象は無い。
 *          `IoHwAb_Button_GetLevel()`/`IoHwAb_Adc_GetValue_mV()` は
 *          `IoHwAb_MainFunction()` が確定した static 変数を返すだけのため、
 *          `IoHwAb_Init()` すら呼ばずに直接検証できる。
 *
 *          分割前の共通の説明は、旧 IoHwAb_test.cpp の冒頭コメント（git の履歴）を参照。
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
class IoHwAb_Adc_GetValue_mV_Test : public ::testing::Test
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

TEST_F(IoHwAb_Adc_GetValue_mV_Test, IoHwAb_Adc_GetValue_mV_NG_NullPointer)
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
    EXPECT_EQ(FakeDetHw_LastErrorId, 0x01U);  // IoHwAb 独自の IOHWAB_E_PARAM_POINTER（IoHwAb_Cfg.h 参照）
}

}  // namespace
