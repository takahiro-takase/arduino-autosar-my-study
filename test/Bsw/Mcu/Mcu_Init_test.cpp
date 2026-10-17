/**
 * \file    Mcu_Init_test.cpp
 * \brief   Mcu_Init() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 Mcu_test.cpp）。
 *
 *          Mcu.h には実 AUTOSAR SWS_Mcu 相当の Mcu_DeInit() が無いため、
 *          「未初期化状態からの呼び出し」を確実に再現するには
 *          MCU_UNIT_TEST ビルドのみに存在する Mcu_Test_ResetInitState()
 *          （テスト専用）が必要（他のテストファイルが SetUp() で
 *          Mcu_Init() を呼んでいるため、native_chain_tests 全体の実行順に
 *          関わらず本ファイル内で確実に未初期化へ戻す）。
 *
 *          分割前の共通の説明は、旧 Mcu_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Mcu.h"
#include "Fake_Mcu_Hw.h"
#include "Wrap_Det.h"
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

/* Mcu の単体テスト用フィクスチャ。
 * SetUp(): 模擬 Mcu_Hw と Det の wrap を初期化し、Mcu を未初期化の状態へ戻す。Mcu_Init() は各テストが呼ぶ。 */
class Mcu_Init_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMcuHw_Reset();
        WrapDet_Reset();
        Mcu_Test_ResetInitState();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Mcu_Init_Test, Mcu_Init_NG_NullConfigLeavesModuleUninitialized)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    // ConfigPtr が NULL の場合、リセット原因キャッシュの更新は行うが
    // 初期化済みとはみなさない（Mcu.c 冒頭コメント参照。現状唯一の
    // 呼び出し元 main.cpp は必ず &Mcu_Config を渡すため到達しないが、
    // 防御的に確認する）。以降の呼び出しが MCU_E_UNINIT を報告し続ける
    // ことで間接的に検証する。
    Mcu_Init(NULL);

    Mcu_ResetType reason = Mcu_GetResetReason();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(reason, MCU_RESET_UNDEFINED);
    EXPECT_EQ(CallCount_Det_ReportError, 1U);
    EXPECT_EQ(LastApiId_Det_ReportError, MCU_API_ID_GET_RESET_REASON);
    EXPECT_EQ(LastErrorId_Det_ReportError, MCU_E_UNINIT);
}

TEST_F(Mcu_Init_Test, Mcu_Init_OK_MarksInitializedAndCachesResetReason)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FakeMcuHw_ResetReason.Watchdog = 1U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Mcu_Init(&Mcu_Config);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeMcuHw_ReadAndClearResetReasonCount, 1U);
    EXPECT_EQ(Mcu_GetResetReason(), MCU_WATCHDOG_RESET);
    EXPECT_EQ(CallCount_Det_ReportError, 0U);
}

}  // namespace
