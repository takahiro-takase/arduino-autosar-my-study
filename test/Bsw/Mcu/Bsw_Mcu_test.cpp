/**
 * \file    Bsw_Mcu_test.cpp
 * \brief   Mcu.c（src/Bsw/Mcu/Mcu.c）の単体テスト
 * \details AUTOSAR SWS_Mcu が規定する Mcu_GetResetReason()/Mcu_GetResetRawValue()/
 *          Mcu_PerformReset() の未初期化時エラー（[SWS_Mcu_00125]、
 *          MCU_E_UNINIT）と、Mcu_GetVersionInfo() の NULL ポインタチェック
 *          （MCU_E_PARAM_POINTER）を検証する。Mcu.c 自体は実物をリンクし、
 *          実 HW 依存の Mcu_Hw.c のみを Fake_Mcu_Hw.c に差し替える
 *          （他モジュールのテストと同じ構成）。
 *
 *          Det_ReportError() へ報告される ModuleId/ApiId/ErrorId が
 *          エラー種別に応じて正しいかは、Wrap_Det.c が記録する
 *          LastModuleId_Det_ReportError/LastApiId_Det_ReportError/
 *          LastErrorId_Det_ReportError で検証する（Wrap_Det.h 参照）。
 *
 *          Mcu.h には実 AUTOSAR SWS_Mcu 相当の Mcu_DeInit() が無いため、
 *          「未初期化状態からの呼び出し」を確実に再現するには
 *          MCU_UNIT_TEST ビルドのみに存在する Mcu_Test_ResetInitState()
 *          （テスト専用）が必要（他のテストファイルが SetUp() で
 *          Mcu_Init() を呼んでいるため、native_chain_tests 全体の実行順に
 *          関わらず本ファイル内で確実に未初期化へ戻す）。
 *
 *          GoogleTest の main() は test_main.cpp に集約しているため、
 *          本ファイルでは定義しない。
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
class McuTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMcuHw_Reset();
        WrapDet_Reset();
        Mcu_Test_ResetInitState();
    }
};

// ------------------------------------------------------------
// Mcu_GetResetReason()
// ------------------------------------------------------------

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(McuTest, Mcu_GetResetReason_OK_ReturnsWatchdogReset)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FakeMcuHw_ResetReason.Watchdog = 1U;
    Mcu_Init(&Mcu_Config);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Mcu_ResetType ret = Mcu_GetResetReason();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, MCU_WATCHDOG_RESET);
    EXPECT_EQ(CallCount_Det_ReportError, 0U);
}

TEST_F(McuTest, Mcu_GetResetReason_OK_ReturnsPowerOnReset)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FakeMcuHw_ResetReason.PowerOn = 1U;
    Mcu_Init(&Mcu_Config);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Mcu_ResetType ret = Mcu_GetResetReason();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, MCU_POWER_ON_RESET);
    EXPECT_EQ(CallCount_Det_ReportError, 0U);
}

TEST_F(McuTest, Mcu_GetResetReason_OK_ReturnsUndefinedWhenNoRecognizedFlag)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // BrownOut/External が単独で立っている場合、Mcu_ResetType には対応する
    // 値が無いため MCU_RESET_UNDEFINED になる（Mcu.h 冒頭コメント参照）。
    FakeMcuHw_ResetReason.BrownOut = 1U;
    Mcu_Init(&Mcu_Config);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Mcu_ResetType ret = Mcu_GetResetReason();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, MCU_RESET_UNDEFINED);
    EXPECT_EQ(CallCount_Det_ReportError, 0U);  // 初期化済みのためエラー報告なし
}

TEST_F(McuTest, Mcu_GetResetReason_NG_ReportsUninitBeforeInit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Mcu_ResetType reason = Mcu_GetResetReason();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(reason, MCU_RESET_UNDEFINED);
    EXPECT_EQ(CallCount_Det_ReportError, 1U);
    EXPECT_EQ(LastModuleId_Det_ReportError, MCU_MODULE_ID);
    EXPECT_EQ(LastApiId_Det_ReportError, MCU_API_ID_GET_RESET_REASON);
    EXPECT_EQ(LastErrorId_Det_ReportError, MCU_E_UNINIT);
}

// ------------------------------------------------------------
// Mcu_GetResetRawValue()
// ------------------------------------------------------------

TEST_F(McuTest, Mcu_GetResetRawValue_OK_ReturnsBitPackedFlags)
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

TEST_F(McuTest, Mcu_GetResetRawValue_NG_ReportsUninitBeforeInit)
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

// ------------------------------------------------------------
// Mcu_PerformReset()
// ------------------------------------------------------------

TEST_F(McuTest, Mcu_PerformReset_NG_ReportsUninitAndDoesNotCallHw)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Mcu_PerformReset();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeMcuHw_PerformResetCount, 0U);
    EXPECT_EQ(CallCount_Det_ReportError, 1U);
    EXPECT_EQ(LastModuleId_Det_ReportError, MCU_MODULE_ID);
    EXPECT_EQ(LastApiId_Det_ReportError, MCU_API_ID_PERFORM_RESET);
    EXPECT_EQ(LastErrorId_Det_ReportError, MCU_E_UNINIT);
}

// ------------------------------------------------------------
// Mcu_GetVersionInfo()
// ------------------------------------------------------------

TEST_F(McuTest, Mcu_GetVersionInfo_OK_FillsExpectedModuleId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Std_VersionInfoType info;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Mcu_GetVersionInfo(&info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(info.moduleID, MCU_MODULE_ID);
    EXPECT_EQ(CallCount_Det_ReportError, 0U);
}

TEST_F(McuTest, Mcu_GetVersionInfo_NG_ReportsParamPointerForNull)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Mcu_GetVersionInfo(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Det_ReportError, 1U);
    EXPECT_EQ(LastModuleId_Det_ReportError, MCU_MODULE_ID);
    EXPECT_EQ(LastApiId_Det_ReportError, MCU_API_ID_GET_VERSION_INFO);
    EXPECT_EQ(LastErrorId_Det_ReportError, MCU_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// Mcu_Init()
// ------------------------------------------------------------

TEST_F(McuTest, Mcu_Init_OK_MarksInitializedAndCachesResetReason)
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

TEST_F(McuTest, Mcu_Init_NG_NullConfigLeavesModuleUninitialized)
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

}  // namespace
