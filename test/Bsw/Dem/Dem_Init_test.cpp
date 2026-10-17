/**
 * \file    Dem_Init_test.cpp
 * \brief   Dem_Init() の単体テスト
 * \details NvM に有効なマジックバイトがあれば、前回のステータスを復元して、経年回復（Aging）と PendingDTC の自動クリアを評価し、
 *          新しい操作サイクルを始める（TF・TFTOC をクリアし、TNCTOC を立てる）。無ければ初回起動として全イベントを初期状態にする。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Dem.h"
#include "Dem_Cfg.h"
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

/* Dem_Init() の単体テスト用フィクスチャ。
 * SetUp(): wrap と DET を初期化し、MemIf・NvM を初期化する（Dem は NvM の RAM ミラーから前回の状態を復元する）。
 *          Persist() は「前回の起動で保存された状態」を NvM へ置く補助関数（event 0 のステータスと経年回復カウンタだけを指定する）。
 *          SavedStatus0()/SavedAging0() は、Dem_Init() が書き戻した内容を読む補助関数。
 * TearDown(): Dem と NvM を未初期化へ戻す。 */
class Dem_Init_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        WrapDem_Reset();  // 他ファイルの故障注入が漏れ伝わらないよう防御的にリセット
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        MemIf_Init();
        NvM_Init(NULL);                // NvM の RAM ミラー（Dem が読み書きする先）を使えるようにする
        FakeDetHw_Reset();
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Dem_Test_ResetInitState();
        NvM_Test_ResetInitState();
    }

    /* 前回の起動で保存されたことにする。event 0 のステータスと経年回復カウンタを指定し、他のイベントは何も起きていない状態にする。 */
    static void Persist(uint8 status0, uint8 aging0)
    {
        uint8 magic = DEM_NVM_MAGIC_BYTE;
        uint8 status[DEM_EVENT_COUNT] = { 0U };
        uint8 aging[DEM_EVENT_COUNT]  = { 0U };
        uint8 extended[DEM_EVENT_COUNT] = { 0U };
        status[0] = status0;
        aging[0]  = aging0;
        (void)NvM_WriteBlock(NVM_BLOCK_ID_DEM_MAGIC, &magic);
        (void)NvM_WriteBlock(NVM_BLOCK_ID_DEM_STATUS, status);
        (void)NvM_WriteBlock(NVM_BLOCK_ID_DEM_AGING, aging);
        (void)NvM_WriteBlock(NVM_BLOCK_ID_DEM_EXTENDED, extended);
    }

    /* Dem_Init() が NvM へ書き戻した（RAM ミラー上の）event 0 のステータス。 */
    static uint8 SavedStatus0()
    {
        uint8 status[DEM_EVENT_COUNT] = { 0U };
        (void)NvM_ReadBlock(NVM_BLOCK_ID_DEM_STATUS, status);
        return status[0];
    }

    /* 同じく、event 0 の経年回復カウンタ。 */
    static uint8 SavedAging0()
    {
        uint8 aging[DEM_EVENT_COUNT] = { 0U };
        (void)NvM_ReadBlock(NVM_BLOCK_ID_DEM_AGING, aging);
        return aging[0];
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Dem_Init_Test, Dem_Init_OK_FirstRunInitializesEventsAndPersistsMagic)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // NvM の RAM ミラーには、有効なマジックバイトが無い（初回起動）。

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dem_Init(NULL);
    uint8 magic = 0U;
    (void)NvM_ReadBlock(NVM_BLOCK_ID_DEM_MAGIC, &magic);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(magic, DEM_NVM_MAGIC_BYTE);
    EXPECT_EQ(SavedStatus0(), (uint8)(DEM_STATUS_NOT_COMPLETED_SINCE_CLEAR | DEM_STATUS_NOT_COMPLETED_THIS_CYCLE));
    EXPECT_EQ(SavedAging0(), 0U);
}

TEST_F(Dem_Init_Test, Dem_Init_OK_RestoredEventStartsNewCycleAndReFailureResetsAging)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Persist(0x2BU, 3U);   // 確定済みで、前のサイクルで再び失敗した（TF・TFTOC が立っている）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dem_Init(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(SavedStatus0(), 0x68U);   // TF・TFTOC はクリアされ、TNCTOC が立つ。確定は残る
    EXPECT_EQ(SavedAging0(), 0U);               // 再び失敗したので、経年回復は最初からやり直す
}

TEST_F(Dem_Init_Test, Dem_Init_OK_AgingCountsUpAfterCleanCycleOfConfirmedEvent)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Persist(0x08U, 1U);   // 確定済みで、前のサイクルは失敗せずに完了した

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dem_Init(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(SavedAging0(), 2U);
    EXPECT_EQ(SavedStatus0(), 0x48U);   // 閾値（5）に届くまでは、確定のまま
}

TEST_F(Dem_Init_Test, Dem_Init_OK_AgingCompletionHealsConfirmedStatus)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Persist(0x08U, (uint8)(DEM_AGING_THRESHOLD_ENGINE_OVERHEAT - 1U));   // あと 1 サイクルで経年回復が完了する

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dem_Init(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(SavedStatus0(), 0x40U);   // 確定が外れる
    EXPECT_EQ(SavedAging0(), 0U);
}

TEST_F(Dem_Init_Test, Dem_Init_OK_AgingPausesWhenPreviousCycleWasNotCompleted)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Persist(0x48U, 2U);   // 確定済みで、前のサイクルは検査を完了しなかった（TNCTOC が立っている）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dem_Init(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(SavedAging0(), 2U);               // 数えない
    EXPECT_EQ(SavedStatus0(), 0x48U);
}

TEST_F(Dem_Init_Test, Dem_Init_OK_AgingResetsForUnconfirmedEvent)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Persist(0x20U, 3U);   // 確定していない（経年回復の対象ではない）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dem_Init(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(SavedAging0(), 0U);
}

TEST_F(Dem_Init_Test, Dem_Init_OK_PendingIsClearedAfterCleanCycle)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Persist(0x04U, 0U);   // PendingDTC が立っていて、前のサイクルは失敗せずに完了した

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dem_Init(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(SavedStatus0(), 0x40U);   // PendingDTC が外れる
}

TEST_F(Dem_Init_Test, Dem_Init_OK_PendingStaysWhenPreviousCycleFailed)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Persist(0x06U, 0U);   // PendingDTC が立っていて、前のサイクルで失敗した（TFTOC が立っている）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dem_Init(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(SavedStatus0(), 0x44U);
}

TEST_F(Dem_Init_Test, Dem_Init_OK_PendingStaysWhenPreviousCycleWasNotCompleted)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Persist(0x44U, 0U);   // PendingDTC が立っていて、前のサイクルは検査を完了しなかった

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dem_Init(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(SavedStatus0(), 0x44U);
}

}  // namespace
