/**
 * \file    Bsw_NvM_WriteAll_test.cpp
 * \brief   NvM_WriteAll()（src/Bsw/NvM/NvM.c）の単体テスト
 * \details ECU リセット直前（Dcm の ECUReset など）に、NvM_WriteBlock() で積んだ
 *          書き込みジョブをすべて EEPROM へ書き切る同期処理
 *          （[SWS_NvM_00018]/[SWS_Dem_00341]）。以前はこの経路が無く、
 *          NvM_MainFunction() の周期実行（10ms 周期で 1 バイトずつ）を待たずに
 *          リセットすると、書き込み途中で失われていた。
 *
 *          Fee/MemIf は実体でリンクされ、模擬 EEPROM（stub/Hal/Fake_Fee_Hw.c）に
 *          書かれる。完了の確認は NvM_GetErrorStatus() が NVM_REQ_OK になることで行う。
 *
 *          GoogleTest の main() は test_main.cpp に集約しているため、
 *          本ファイルでは定義しない。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "NvM.h"
#include "NvM_Cfg.h"
#include "MemIf.h"
#include "Fake_Det_Hw.h"
#include "Wrap_Dem.h"
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

class Bsw_NvM_WriteAll_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;
        MemIf_Init();
        NvM_Init(NULL);
        WrapDem_Reset();
        FakeDetHw_Reset();
        std::memset(statusData, 0x5A, sizeof(statusData));    // 既存内容と異なる値（書き込みスキップ回避）
        std::memset(agingData, 0x5A, sizeof(agingData));
        std::memset(extendedData, 0x5A, sizeof(extendedData));
        FakeDetHw_LogSuppressed = 0U;
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        NvM_Test_ResetInitState();
    }

    NvM_RequestResultType ResultOf(NvM_BlockIdType id)
    {
        NvM_RequestResultType r = NVM_REQ_PENDING;
        (void)NvM_GetErrorStatus(id, &r);
        return r;
    }

    uint8 statusData[NVM_BLOCK_DEM_STATUS_LENGTH];
    uint8 agingData[NVM_BLOCK_DEM_AGING_LENGTH];
    uint8 extendedData[NVM_BLOCK_DEM_EXTENDED_LENGTH];
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

// ------------------------------------------------------------
// NG: NvM_Init() 前に呼ぶと NVM_E_NOT_INITIALIZED を DET へ報告して戻る（[SWS_NvM_00647]）。
// ------------------------------------------------------------
TEST_F(Bsw_NvM_WriteAll_Test, NvM_WriteAll_NG_NotInitializedReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    NvM_Test_ResetInitState();
    FakeDetHw_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    NvM_WriteAll();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);
    EXPECT_EQ(FakeDetHw_LastApiId, NVM_API_ID_WRITE_ALL);
    EXPECT_EQ(FakeDetHw_LastErrorId, NVM_E_NOT_INITIALIZED);
}

}  // namespace
