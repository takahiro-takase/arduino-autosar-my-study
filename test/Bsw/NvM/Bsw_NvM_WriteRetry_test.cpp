/**
 * \file    Bsw_NvM_WriteRetry_test.cpp
 * \brief   NvM_WriteBlock() の書き込みリトライ上限（src/Bsw/NvM/NvM.c）の単体テスト
 * \details [SWS_NvM_00213]/[SWS_NvM_00659]: 書き込みジョブで MemIf が書き込みを拒否した、
 *          またはジョブが失敗するたびにリトライカウンタを +1 し、NVM_MAX_NUM_OF_WRITE_RETRIES
 *          を超えたらブロックの要求結果を NVM_REQ_NOT_OK にして NVM_E_REQ_FAILED
 *          （DEM_EVENT_NVM_REQ_FAILED）を Dem へ報告する。以前はリトライ上限が無く、
 *          拒否が続くと同じジョブを永久に再試行し、要求結果も PENDING のまま
 *          後続のブロックも処理されなかった。
 *
 *          書き込みの拒否は、テスト側が先に MemIf_Write() で Fee を BUSY にしておく
 *          ことで再現する（MemIf_MainFunction() を呼ばない限り Fee は BUSY のまま。
 *          NvM の MemIf_Write() は E_NOT_OK で拒否される）。Dem への報告は
 *          `Wrap_Dem.h` の呼び出し記録で観測する。
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
#include "Dem.h"
#include "Dem_Cfg.h"
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

class Bsw_NvM_WriteRetry_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;
        MemIf_Init();
        NvM_Init(NULL);
        WrapDem_Reset();
        FakeDetHw_Reset();
        std::memset(newData, 0x5A, sizeof(newData));  // 既存内容（全 0 ほか）と異なる値（書き込みスキップ回避）
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        NvM_Test_ResetInitState();
    }

    /** Fee を BUSY にする（テスト側が MemIf_Write() を投入し、MemIf_MainFunction() を呼ばない）。 */
    void MakeFeeBusy()
    {
        static const uint8 dummy = 0xA5U;
        ASSERT_EQ(MemIf_Write(MEMIF_DEVICE_0, kScratchAddress, &dummy, 1U), E_OK);
        ASSERT_EQ(MemIf_GetStatus(MEMIF_DEVICE_0), MEMIF_BUSY);
    }

    /** BUSY にしておいた Fee のジョブを完了させて解放する。 */
    void ReleaseFee()
    {
        for (uint8 i = 0U; i < 50U && MemIf_GetStatus(MEMIF_DEVICE_0) == MEMIF_BUSY; i++)
        {
            MemIf_MainFunction();
        }
        ASSERT_NE(MemIf_GetStatus(MEMIF_DEVICE_0), MEMIF_BUSY);
    }

    /** NvM が処理を終えるまで NvM_MainFunction()/MemIf_MainFunction() を回す。 */
    void RunUntilIdle()
    {
        NvM_RequestResultType r = NVM_REQ_PENDING;
        for (uint16 i = 0U; i < 500U; i++)
        {
            NvM_MainFunction();
            MemIf_MainFunction();
            (void)NvM_GetErrorStatus(NVM_BLOCK_ID_DEM_STATUS, &r);
            if (r != NVM_REQ_PENDING)
            {
                break;
            }
        }
    }

    NvM_RequestResultType ResultOf(NvM_BlockIdType id)
    {
        NvM_RequestResultType r = NVM_REQ_PENDING;
        (void)NvM_GetErrorStatus(id, &r);
        return r;
    }

    static const uint16 kScratchAddress = 0x0100U;  // NvM が管理する領域の外（Fee を塞ぐためのダミー書き込み先）
    uint8 newData[NVM_BLOCK_DEM_STATUS_LENGTH];
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Bsw_NvM_WriteRetry_Test, NvM_MainFunction_NG_WriteRejectedBeyondRetryLimitFailsBlockAndReportsToDem)
{
    MakeFeeBusy();
    ASSERT_EQ(NvM_WriteBlock(NVM_BLOCK_ID_DEM_STATUS, newData), E_OK);

    // 1 回目の拒否〜上限回数までは、まだリトライ中（要求結果は PENDING のまま、Dem へも報告しない）。
    for (uint8 i = 0U; i < NVM_MAX_NUM_OF_WRITE_RETRIES; i++)
    {
        NvM_MainFunction();
        EXPECT_EQ(ResultOf(NVM_BLOCK_ID_DEM_STATUS), NVM_REQ_PENDING) << "retry " << (unsigned)(i + 1U);
    }
    EXPECT_EQ(CallCount_Dem_SetEventStatus, 0U);

    // 上限を超える拒否: ブロックを諦め、NVM_E_REQ_FAILED（FAILED）を Dem へ報告する。
    NvM_MainFunction();

    EXPECT_EQ(CallCount_Dem_SetEventStatus, 1U);
    EXPECT_EQ(LastEventId_Dem_SetEventStatus, DEM_EVENT_NVM_REQ_FAILED);
    EXPECT_EQ(LastEventStatus_Dem_SetEventStatus, DEM_EVENT_STATUS_FAILED);

    // 実 Dem は FAILED 確定で自身のステータスブロック（ここで失敗させたブロックと同じ）を
    // 書き直そうとするため、その再書き込みも同様に諦めるまで回してから最終結果を確認する。
    for (uint8 i = 0U; i < 20U; i++)
    {
        NvM_MainFunction();
    }
    EXPECT_EQ(ResultOf(NVM_BLOCK_ID_DEM_STATUS), NVM_REQ_NOT_OK);
}

TEST_F(Bsw_NvM_WriteRetry_Test, NvM_MainFunction_NG_BlockAfterFailedBlockIsStillProcessed)
{
    MakeFeeBusy();
    ASSERT_EQ(NvM_WriteBlock(NVM_BLOCK_ID_DEM_STATUS, newData), E_OK);
    // リトライを使い切り、Dem の再書き込みも含めて諦めるまで回す（前のテストが残した実 Dem の状態に
    // よって再書き込みの有無が変わるため、tick 数を厳密には仮定しない）。
    for (uint8 i = 0U; i < 20U; i++)
    {
        NvM_MainFunction();
    }
    ASSERT_EQ(ResultOf(NVM_BLOCK_ID_DEM_STATUS), NVM_REQ_NOT_OK);

    // Fee が空いた後の別ブロックの書き込みは通常どおり完了する（以前は無限に再試行して詰まっていた）。
    ReleaseFee();
    uint8 agingData[NVM_BLOCK_DEM_AGING_LENGTH];
    std::memset(agingData, 0x3C, sizeof(agingData));
    ASSERT_EQ(NvM_WriteBlock(NVM_BLOCK_ID_DEM_AGING, agingData), E_OK);
    for (uint16 i = 0U; i < 500U && ResultOf(NVM_BLOCK_ID_DEM_AGING) == NVM_REQ_PENDING; i++)
    {
        NvM_MainFunction();
        MemIf_MainFunction();
    }

    EXPECT_EQ(ResultOf(NVM_BLOCK_ID_DEM_AGING), NVM_REQ_OK);
    // 失敗からの回復として、成功した書き込みで PASSED が報告される（[SWS_NvM_00873]）。
    EXPECT_EQ(LastEventId_Dem_SetEventStatus, DEM_EVENT_NVM_REQ_FAILED);
    EXPECT_EQ(LastEventStatus_Dem_SetEventStatus, DEM_EVENT_STATUS_PASSED);
}

TEST_F(Bsw_NvM_WriteRetry_Test, NvM_MainFunction_OK_WriteRecoversWithinRetryLimitWithoutDemReport)
{
    MakeFeeBusy();
    ASSERT_EQ(NvM_WriteBlock(NVM_BLOCK_ID_DEM_STATUS, newData), E_OK);

    // 上限以内の拒否（2 回）の後で Fee が空けば、書き込みは成功する。
    NvM_MainFunction();
    NvM_MainFunction();
    ASSERT_EQ(ResultOf(NVM_BLOCK_ID_DEM_STATUS), NVM_REQ_PENDING);
    ReleaseFee();
    RunUntilIdle();

    EXPECT_EQ(ResultOf(NVM_BLOCK_ID_DEM_STATUS), NVM_REQ_OK);
    EXPECT_EQ(CallCount_Dem_SetEventStatus, 0U);  // 上限内で成功すれば失敗も回復も報告しない
}

}  // namespace
