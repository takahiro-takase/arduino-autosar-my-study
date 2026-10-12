/**
 * \file    Bsw_NvM_InitReport_test.cpp
 * \brief   NvM_Init() で検出した結果の Dem への報告（src/Bsw/NvM/NvM.c）の単体テスト
 * \details [SWS_NvM_00591]/[SWS_NvM_00595]: 起動時の読み込みで検出した CRC 不整合・冗長性喪失は、
 *          NvM_Init() の時点では Dem がまだ初期化されていないため、保留する。Dem が初期化済みになった後の、
 *          最初の NvM_MainFunction() が 1 回だけ報告する（以前は、独自 API の NvM_ReportBootDiagnosticsToDem() を
 *          EcuM_Init() が呼んでいた）。EEPROM が空の状態で NvM_Init() すると、ブロックはデフォルト値で復元され、
 *          整合性は PASSED として報告される。
 *
 *          GoogleTest の main() は test_main.cpp に集約しているため、本ファイルでは定義しない。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "NvM.h"
#include "NvM_Cfg.h"
#include "MemIf.h"
#include "Dem.h"
#include "Dem_Cfg.h"
#include "Fake_Det_Hw.h"
#include "Wrap_Dem.h"
}

namespace
{

/* NvM_Init() の結果の Dem への報告の単体テスト用フィクスチャ。
 * SetUp(): MemIf_Init() と NvM_Init(NULL) を呼び、Dem の wrap と DET の記録を初期化する（NvM_MainFunction() はまだ呼ばない）。
 * TearDown(): 未初期化の状態へ戻す。 */
class Bsw_NvM_InitReport_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;
        MemIf_Init();
        NvM_Init(NULL);
        WrapDem_Reset();
        FakeDetHw_Reset();
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        NvM_Test_ResetInitState();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Bsw_NvM_InitReport_Test, NvM_MainFunction_OK_FirstCallReportsInitResultToDemExactlyOnce)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(CallCount_Dem_SetEventStatus, 0U);   // NvM_Init() だけでは、Dem へ報告しない（保留する）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    NvM_MainFunction();
    uint32 afterFirst = CallCount_Dem_SetEventStatus;
    NvM_MainFunction();
    NvM_MainFunction();
    uint32 afterThird = CallCount_Dem_SetEventStatus;

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_GE(afterFirst, 1U);        // 最初の呼び出しで、整合性（PASSED など）を報告する
    EXPECT_EQ(afterThird, afterFirst); // 以降は、報告しない（1 回だけ）
}

}  // namespace
