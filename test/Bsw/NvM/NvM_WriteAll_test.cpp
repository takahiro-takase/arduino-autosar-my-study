/**
 * \file    NvM_WriteAll_test.cpp
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
#include "Wrap_MemIf.h"
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

/* NvM_WriteAll() の単体テスト用フィクスチャ。
 * SetUp(): MemIf_Init() と NvM_Init(NULL) を呼び、Dem の wrap と DET の記録を初期化する。書き込みデータは、既存の内容と異なる値で埋めておく（書き込みがスキップされないようにするため）。ResultOf() は、ブロックの要求結果を取得する補助関数。
 * TearDown(): 未初期化の状態へ戻す。 */
class NvM_WriteAll_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;
        MemIf_Init();
        NvM_Init(NULL);
        WrapDem_Reset();
        WrapMemIf_Reset();   // 起動時の処理で数えた回数を除き、故障注入も他のテストから漏れないようにする
        FakeDetHw_Reset();
        std::memset(statusData, 0x5A, sizeof(statusData));    // 既存内容と異なる値（書き込みスキップ回避）
        std::memset(agingData, 0x5A, sizeof(agingData));
        std::memset(extendedData, 0x5A, sizeof(extendedData));
        FakeDetHw_LogSuppressed = 0U;
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        WrapMemIf_Reset();   // 故障注入を残さない
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
TEST_F(NvM_WriteAll_Test, NvM_WriteAll_NG_NotInitializedReportsDet)
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

TEST_F(NvM_WriteAll_Test, NvM_WriteAll_NG_ReturnsAtIterationLimitWhenJobNeverCompletes)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_MemIf_GetJobResult = 1U;   // 書き込みジョブが、いつまでも終わらない
    ForcedReturn_MemIf_GetJobResult      = MEMIF_JOB_PENDING;
    ASSERT_EQ(NvM_WriteBlock(NVM_BLOCK_ID_DEM_STATUS, statusData), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    NvM_WriteAll();   // 終わらないジョブを待ち続けず、上限で戻る

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_MemIf_MainFunction, (uint32)NVM_WRITEALL_MAX_ITERATIONS);   // 回数の上限で打ち切る
    EXPECT_EQ(ResultOf(NVM_BLOCK_ID_DEM_STATUS), NVM_REQ_PENDING);                   // ジョブは終わっていない
}

}  // namespace
