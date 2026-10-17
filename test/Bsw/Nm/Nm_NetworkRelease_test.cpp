/**
 * \file    Nm_NetworkRelease_test.cpp
 * \brief   Nm_NetworkRelease() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 Nm_test.cpp）。
 *
 *          分割前の共通の説明は、旧 Nm_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Nm.h"
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

/* Nm の単体テスト用フィクスチャ。
 * SetUp(): Nm_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。範囲外のチャネルを表す kInvalidChannel も用意する。
 * TearDown(): 未初期化の状態へ戻す。 */
class Nm_NetworkRelease_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        Nm_Init(NULL);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Nm_Test_ResetInitState();
    }

    static const NetworkHandleType kInvalidChannel = NM_MAIN_NETWORK_HANDLE + 1U;
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Nm_NetworkRelease_Test, Nm_NetworkRelease_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Nm_Test_ResetInitState();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Nm_NetworkRelease(NM_MAIN_NETWORK_HANDLE);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, NM_E_UNINIT);
}

TEST_F(Nm_NetworkRelease_Test, Nm_NetworkRelease_NG_InvalidChannel)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Nm_NetworkRelease(kInvalidChannel);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, NM_E_INVALID_CHANNEL);
}

}  // namespace
