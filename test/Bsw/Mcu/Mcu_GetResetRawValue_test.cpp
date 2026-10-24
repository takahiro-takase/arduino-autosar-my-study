/**
 * \file    Mcu_GetResetRawValue_test.cpp
 * \brief   Mcu_GetResetRawValue() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 Mcu_test.cpp）。
 *
 *          AUTOSAR SWS_Mcu が規定する Mcu_GetResetReason()/Mcu_GetResetRawValue()/
 *          Mcu_PerformReset() の未初期化時エラー（[SWS_Mcu_00125]、
 *          MCU_E_UNINIT）と、Mcu_GetVersionInfo() の NULL ポインタチェック
 *          （MCU_E_PARAM_POINTER）を検証する。Mcu.c 自体は実物をリンクし、
 *          実 HW 依存の Mcu_Hw.c のみを Fake_Mcu_Hw.c に差し替える
 *          （他モジュールのテストと同じ構成）。
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
class Mcu_GetResetRawValue_Test : public ::testing::Test
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

TEST_F(Mcu_GetResetRawValue_Test, Mcu_GetResetRawValue_NG_ReportsUninitBeforeInit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Mcu_RawResetType raw = Mcu_GetResetRawValue();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(raw, 0xFFU);  // [SWS_Mcu_00135] 実装依存の非ゼロ値
    EXPECT_EQ(CallCount_Det_ReportError, 1U);
    EXPECT_EQ(LastModuleId_Det_ReportError, MCU_MODULE_ID);
    EXPECT_EQ(LastApiId_Det_ReportError, MCU_API_ID_GET_RESET_RAW_VALUE);
    EXPECT_EQ(LastErrorId_Det_ReportError, MCU_E_UNINIT);
}

TEST_F(Mcu_GetResetRawValue_Test, Mcu_GetResetRawValue_OK_ReturnsBitPackedFlags)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FakeMcuHw_ResetReason.Watchdog = 1U;
    FakeMcuHw_ResetReason.PowerOn  = 1U;
    Mcu_Init(&Mcu_Config);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Mcu_RawResetType ret = Mcu_GetResetRawValue();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret,
              static_cast<Mcu_RawResetType>(MCU_RAW_RESET_WATCHDOG_BIT | MCU_RAW_RESET_POWERON_BIT));
    EXPECT_EQ(CallCount_Det_ReportError, 0U);
}

TEST_F(Mcu_GetResetRawValue_Test, Mcu_GetResetRawValue_OK_SoftwareFlagSetsBit4)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FakeMcuHw_ResetReason.Software = 1U;
    Mcu_Init(&Mcu_Config);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Mcu_RawResetType ret = Mcu_GetResetRawValue();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, static_cast<Mcu_RawResetType>(MCU_RAW_RESET_SOFTWARE_BIT));
    EXPECT_EQ(ret, 0x10U);   // bit4
}

}  // namespace
