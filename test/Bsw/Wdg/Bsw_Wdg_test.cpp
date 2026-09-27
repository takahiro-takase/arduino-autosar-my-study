/**
 * \file    Bsw_Wdg_test.cpp
 * \brief   Wdg.c（src/Bsw/Wdg/Wdg.c）の単体テスト（NG系）
 * \details Wdg.c 全公開APIの `Det_ReportError()` 呼び出し箇所（NG ケースのみ）を
 *          1ファイルにまとめる。各ケースで、報告される ErrorId が仕様どおり
 *          正しい値になっていることを `Fake_Det_Hw.h`（`FakeDetHw_LastErrorId`）
 *          で検証する。
 *
 *          `Wdg_SetMode(WDGIF_OFF_MODE)`（本プロジェクトの HW 制約により常に
 *          E_NOT_OK を返すが、報告は Det ではなく Dem 経由）は本ファイルの
 *          スコープ外（Det_ReportError() を呼ばないため）。
 *
 *          Wdg には `Wdg_DeInit()` に相当する API が無く、`Wdg_Initialized` は
 *          native_chain_tests バイナリ全体で共有される static のため、
 *          `WDG_E_DRIVER_STATE` の検証には `Wdg_Test_ResetInitState()`
 *          （`WDG_UNIT_TEST` ビルドのみに存在するテスト専用関数、
 *          `Mcu_Test_ResetInitState()` と同じ設計）を使う。
 *
 *          実 HW（IWDT）への依存は `stub/Hal/Fake_Wdg_Hw.c` で満たす
 *          （2026-09、WdgIf/Wdg 自身の Det 検証のため実体リンクへ切り替えた
 *          際に新設）。
 *
 *          GoogleTest の main() は test_main.cpp に集約しているため、
 *          本ファイルでは定義しない。
 */
#include <gtest/gtest.h>

extern "C" {
#include "Wdg.h"
#include "Wdg_PBCfg.h"
#include "Fake_Wdg_Hw.h"
#include "Fake_Det_Hw.h"
}

namespace
{

class Bsw_Wdg_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeWdgHw_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        Wdg_Init(&Wdg_Config);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Wdg_Test_ResetInitState();
    }
};

// ------------------------------------------------------------
// Wdg_Init()
// ------------------------------------------------------------

TEST_F(Bsw_Wdg_Test, Wdg_Init_NG_NullConfigPtr)
{
    Wdg_Test_ResetInitState();

    Wdg_Init(NULL);

    EXPECT_EQ(FakeDetHw_LastErrorId, WDG_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// Wdg_SetMode()
// ------------------------------------------------------------

TEST_F(Bsw_Wdg_Test, Wdg_SetMode_NG_DriverState)
{
    Wdg_Test_ResetInitState();

    Std_ReturnType ret = Wdg_SetMode(WDGIF_FAST_MODE);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, WDG_E_DRIVER_STATE);
}

TEST_F(Bsw_Wdg_Test, Wdg_SetMode_NG_ParamMode)
{
    // WDGIF_SLOW_MODE は本プロジェクトが対応しない値（default 分岐、
    // Wdg.c 参照）。
    Std_ReturnType ret = Wdg_SetMode(static_cast<WdgIf_ModeType>(0xFFU));

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, WDG_E_PARAM_MODE);
}

// ------------------------------------------------------------
// Wdg_SetTriggerCondition()
// ------------------------------------------------------------

TEST_F(Bsw_Wdg_Test, Wdg_SetTriggerCondition_NG_DriverState)
{
    Wdg_Test_ResetInitState();

    Wdg_SetTriggerCondition(100U);

    EXPECT_EQ(FakeDetHw_LastErrorId, WDG_E_DRIVER_STATE);
}

TEST_F(Bsw_Wdg_Test, Wdg_SetTriggerCondition_NG_ParamTimeout)
{
    // Wdg_Config.DefaultTimeoutMs を超える timeout。
    uint16 tooLarge = static_cast<uint16>(Wdg_Config.DefaultTimeoutMs + 1U);

    Wdg_SetTriggerCondition(tooLarge);

    EXPECT_EQ(FakeDetHw_LastErrorId, WDG_E_PARAM_TIMEOUT);
}

// ------------------------------------------------------------
// Wdg_GetVersionInfo()
// ------------------------------------------------------------

TEST_F(Bsw_Wdg_Test, Wdg_GetVersionInfo_NG_NullPointer)
{
    Wdg_GetVersionInfo(NULL);

    EXPECT_EQ(FakeDetHw_LastErrorId, WDG_E_PARAM_POINTER);
}

}  // namespace
