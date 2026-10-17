/**
 * \file    NvM_InitCrcRecovery_test.cpp
 * \brief   NvM_Init() の CRC 不一致時の自動復元（EEPROM への同期書き戻し）の単体テスト
 * \details 空の EEPROM（全 0xFF、test_main.cpp が毎テスト前に初期化する）に対する最初の
 *          NvM_Init() は、全ブロックで CRC 不一致を検出し、デフォルト値で復元して EEPROM へ書き戻す
 *          （NvM_WriteJobSync() が MemIf_Write() で発行したジョブを完了まで進める）。
 *          書き戻しが正しく行われていれば、続けて 2 回目の NvM_Init() を呼んだとき、
 *          復元したブロックも含めて全ブロックの CRC が一致し、NVM_REQ_OK になる。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "NvM.h"
#include "NvM_Cfg.h"
#include "MemIf.h"
#include "Fake_Det_Hw.h"
#include "Wrap_Dem.h"
#include "Wrap_MemIf.h"
}

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

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

/* NvM_Init() の CRC 不一致時の復元の単体テスト用フィクスチャ。
 * SetUp(): DET の記録と Dem の wrap を初期化し、MemIf_Init() を呼ぶ（NvM_Init() は各テストが呼ぶ）。
 * TearDown(): NvM を未初期化へ戻す。 */
class NvM_InitCrcRecovery_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;
        WrapMemIf_Reset();   // 他のテストファイルの故障注入が漏れ伝わらないようリセット
        WrapDem_Reset();
        FakeDetHw_Reset();
        MemIf_Init();
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        WrapMemIf_Reset();   // 故障注入を残さない
        NvM_Test_ResetInitState();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(NvM_InitCrcRecovery_Test, NvM_Init_NG_RecoveryWriteRejectedLeavesEepromBlank)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_MemIf_Write = 1U;   // MemIf が、書き込みを受け付けない
    NvM_Init(NULL);   // 空の EEPROM: 復元して書き戻そうとするが、書き込みを拒否される
    ASSERT_GT(CallCount_MemIf_Write, 0U);   // 書き戻しを試みた
    WrapMemIf_Reset();   // 以降は正常に書ける

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    NvM_Init(NULL);   // もう一度読み直す

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    uint8 notOkCount = 0U;
    for (uint8 id = 0U; id < NVM_BLOCK_COUNT; id++)
    {
        NvM_RequestResultType result = NVM_REQ_OK;
        ASSERT_EQ(NvM_GetErrorStatus(id, &result), E_OK);
        if (result != NVM_REQ_OK)
        {
            notOkCount++;
        }
    }
    EXPECT_GT(notOkCount, 0U);   // 書き戻せていないので、EEPROM は空のまま（CRC 不一致が続く）
}

TEST_F(NvM_InitCrcRecovery_Test, NvM_Init_NG_RecoveryWriteThatNeverCompletesIsCancelled)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_MemIf_GetStatus = 1U;       // MemIf が、いつまでも BUSY を返す（ジョブが終わらない）
    ForcedReturn_MemIf_GetStatus      = MEMIF_BUSY;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    NvM_Init(NULL);   // 空の EEPROM: 復元して書き戻そうとする

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_GT(CallCount_MemIf_Cancel, 0U);   // 待ちきれなかったジョブを、中断する
}

TEST_F(NvM_InitCrcRecovery_Test, NvM_Init_NG_RecoveryWriteJobFailureDoesNotHang)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_MemIf_GetJobResult = 1U;   // MemIf が、ジョブの失敗を返す
    ForcedReturn_MemIf_GetJobResult      = MEMIF_JOB_FAILED;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    NvM_Init(NULL);   // 空の EEPROM: 復元して書き戻そうとする

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_GT(CallCount_MemIf_GetJobResult, 0U);   // ジョブの結果を確認し、失敗でも先へ進む
    EXPECT_EQ(MemIf_GetStatus(MEMIF_DEVICE_0), MEMIF_IDLE);
}

TEST_F(NvM_InitCrcRecovery_Test, NvM_Init_OK_RecoveredBlocksPassCrcOnNextInit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    NvM_Init(NULL);   // 空の EEPROM: 全ブロックで CRC 不一致 → 復元して書き戻す
    NvM_RequestResultType firstResult[NVM_BLOCK_COUNT];
    uint8 recoveredCount = 0U;
    for (uint8 id = 0U; id < NVM_BLOCK_COUNT; id++)
    {
        ASSERT_EQ(NvM_GetErrorStatus(id, &firstResult[id]), E_OK);
        if (firstResult[id] != NVM_REQ_OK)
        {
            recoveredCount++;   // CRC 不一致を検出して復元した（空の EEPROM の全 0xFF が偶然一致するブロックは除く）
        }
    }
    ASSERT_GT(recoveredCount, 0U);
    ASSERT_EQ(MemIf_GetStatus(MEMIF_DEVICE_0), MEMIF_IDLE);   // 書き込みジョブが残っていない
    ASSERT_EQ(FakeDetHw_ReportCount, 0U);                     // MemIf_Write の拒否（FEE_E_BUSY 等）が出ていない

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    NvM_Init(NULL);   // 書き戻された内容を読み直す

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    for (uint8 id = 0U; id < NVM_BLOCK_COUNT; id++)
    {
        NvM_RequestResultType result = NVM_REQ_NOT_OK;
        ASSERT_EQ(NvM_GetErrorStatus(id, &result), E_OK);
        EXPECT_EQ(result, NVM_REQ_OK);            // 書き戻しが正しければ、復元したブロックも CRC が一致する
    }
}

}  // namespace
