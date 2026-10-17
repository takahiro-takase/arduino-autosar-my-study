/**
 * \file    Port_SetPinMode_test.cpp
 * \brief   Port_SetPinMode() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 Port_test.cpp）。
 *
 *          AUTOSAR SWS_Port_00060/00061 が規定する「全ピンの方向を設定方向へ
 *          再適用する」挙動、および SWS_Port_00223 が規定する
 *          Port_SetPinMode() の PORT_E_MODE_UNCHANGEABLE 報告を検証する。
 *          Port.c 自体は実物をリンクし、実 HW 依存の Port_Hw.cpp のみを
 *          Fake_Port_Hw.c（ピンごとに直近の設定方向を記録する簡易メモリ
 *          モデル）に差し替える。
 *
 *          分割前の共通の説明は、旧 Port_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Port.h"
#include "Fake_Port_Hw.h"
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

/* Port の単体テスト用フィクスチャ。
 * SetUp(): 模擬 Port_Hw と DET の記録を初期化する。 */
class Port_SetPinMode_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakePortHw_Reset();
        FakeDetHw_Reset();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Port_SetPinMode_Test, Port_SetPinMode_NG_AlwaysReportsModeUnchangeableAndHasNoEffect)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Port_Init(NULL);
    FakePortHw_Reset();  /* Port_Init 自身の適用呼び出しを後続の検証対象から除く */

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Port_SetPinMode(PORT_PIN_LED_RUNNING, 0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, PORT_E_MODE_UNCHANGEABLE);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);
    /* [SWS_Port_00223]: エラー報告以外は何もしない（ピン方向は変化しない）。 */
    EXPECT_EQ(FakePortHw_SetPinDirectionCount, 0U);
}

}  // namespace
