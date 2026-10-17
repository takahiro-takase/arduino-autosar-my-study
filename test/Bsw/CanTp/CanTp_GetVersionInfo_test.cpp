/**
 * \file    CanTp_GetVersionInfo_test.cpp
 * \brief   CanTp_GetVersionInfo() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 CanTp_test.cpp）。
 *
 *          分割前の共通の説明は、旧 CanTp_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "CanTp.h"
#include "Fake_Det_Hw.h"
#include "Fake_Millis.h"
#include "Wrap_CanTp.h"
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

/* CanTp の単体テスト用フィクスチャ。
 * SetUp(): 時刻と CanTp の wrap（呼び出し回数・故障注入。他のテストが設定したまま残っていても影響を受けないよう）を初期化し、CanTp_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。 */
class CanTp_GetVersionInfo_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        WrapCanTp_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        CanTp_Init(NULL);              // CfgPtr は常に NULL（CanTp.h 参照）
        FakeDetHw_Reset();              // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    // CanTp に DeInit() は無いため TearDown は書かない。未初期化状態が必要な
    // テストは各自 CanTp_Test_ResetInitState() を明示的に呼ぶ（ファイル冒頭
    // コメント参照）。
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(CanTp_GetVersionInfo_Test, CanTp_GetVersionInfo_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTp_GetVersionInfo(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, CANTP_E_PARAM_POINTER);
}

}  // namespace
