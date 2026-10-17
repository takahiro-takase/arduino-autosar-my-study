/**
 * \file    WdgM_MainFunction_test.cpp
 * \brief   WdgM_MainFunction() の判定と HW ウォッチドッグのリフレッシュの単体テスト
 * \details Deadline 違反が原因のエンティティを、原因として記録する。POST_RUN の抑制中は、判定サイクルで異常を数えず、Global が STOPPED でも
 *          HW ウォッチドッグのリフレッシュを続ける。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "WdgM.h"
#include "WdgM_TestUtil.h"
#include "Wdg.h"
#include "Wdg_PBCfg.h"
#include "Fake_Wdg_Hw.h"
#include "Wrap_Dem.h"
#include "Fake_Det_Hw.h"
#include "Fake_Millis.h"
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

/* WdgM_MainFunction() の単体テスト用フィクスチャ。
 * SetUp(): 時刻・模擬 Wdg_Hw・DET・Dem の wrap を初期化し、Dem_Init(NULL)・Wdg_Init()・WdgM_Init() を EcuM_Init() と同じ順序で呼ぶ（Init 自体の記録は消す）。
 *          補助関数: ViolateEngineDeadline()（ENGINE の Deadline 違反を作る）、DriveToStoppedByEngineDeadline()（その違反が原因で STOPPED まで進める）。 */
class WdgM_MainFunction_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        FakeDetHw_Reset();
        FakeWdgHw_Reset();
        WrapDem_Reset();
        Dem_Init(NULL);  // Demの内部状態を毎テスト決定的にリセットする（Fake_NvM.cにより常に「初回起動」）
        Wdg_Init(&Wdg_Config);
        WdgM_Init(&WdgM_Config);
        FakeWdgHw_RefreshCount = 0U;
        FakeDetHw_Reset();
    }

    /* WARNING の Local Status を読む。 */
    static WdgM_LocalStatusType WarningStatus()
    {
        WdgM_LocalStatusType status = WDGM_LOCAL_STATUS_OK;
        (void)WdgM_GetLocalStatus(WDGM_ENTITY_WARNING, &status);
        return status;
    }

    /* WARNING の Local Status を読む。 */
    static WdgM_LocalStatusType WarningStatusOf()
    {
        WdgM_LocalStatusType status = WDGM_LOCAL_STATUS_OK;
        (void)WdgM_GetLocalStatus(WDGM_ENTITY_WARNING, &status);
        return status;
    }

    /* ENGINE の START→END を、Deadline の上限（500ms）を超える間隔で報告して、Deadline 違反（EXPIRED）にする。 */
    static void ViolateEngineDeadline()
    {
        WdgM_CheckpointReached(WDGM_ENTITY_ENGINE, WDGM_CP_ENGINE_START);
        FakeMillis_Value += WDGM_ENGINE_DEADLINE_START_TO_END_MAX_MS + 1UL;
        WdgM_CheckpointReached(WDGM_ENTITY_ENGINE, WDGM_CP_ENGINE_END);
    }

    /* WARNING が、許可された遷移・Deadline の範囲内で START→END を n 組報告する（1 組 550ms、Alive は 2n 回）。
     * WARNING の END→START は 1500ms 以内に報告する必要があるため、ENGINE の報告の合間（ENGINE の END→START は
     * 2500ms 以上空ける）にも、WARNING の報告を続ける。 */
    static void ReportWarningPairs(uint8 n)
    {
        for (uint8 i = 0U; i < n; i++)
        {
            FakeMillis_Value += 500UL;
            WdgM_CheckpointReached(WDGM_ENTITY_WARNING, WDGM_CP_WARNING_START);
            FakeMillis_Value += 50UL;
            WdgM_CheckpointReached(WDGM_ENTITY_WARNING, WDGM_CP_WARNING_END);
        }
    }

    /* ENGINE が、START→END を 1 組（Alive は 2 回）報告する。直前の ENGINE の報告から 2500ms 以上空いていること。 */
    static void ReportEnginePair()
    {
        WdgM_CheckpointReached(WDGM_ENTITY_ENGINE, WDGM_CP_ENGINE_START);
        FakeMillis_Value += 100UL;
        WdgM_CheckpointReached(WDGM_ENTITY_ENGINE, WDGM_CP_ENGINE_END);
    }

    /* WARNING と ENGINE の両方が、判定サイクル 1 回分の Alive を満たし、Logical/Deadline も違反しない報告をする
     * （WARNING の 5 組 = 2750ms の後に ENGINE を報告するので、ENGINE の END→START は 2750ms 空く）。 */
    static void ReportAllAliveOk()
    {
        ReportWarningPairs(5U);
        ReportEnginePair();
    }

    /* 判定サイクル 1 回分。ENGINE は毎サイクル 1 回（START/END を交互に）報告して Alive を満たす。WARNING は何も報告しない。 */
    uint8 engineNextIsStart = 0U;   // 直前の ViolateEngineDeadline() の続き（END の次は START）
    void RunCycleKeepingEngineAlive()
    {
        FakeMillis_Value += 3000UL;
        WdgM_CheckpointReached(WDGM_ENTITY_ENGINE, (engineNextIsStart != 0U) ? WDGM_CP_ENGINE_START : WDGM_CP_ENGINE_END);
        engineNextIsStart = (uint8)(engineNextIsStart == 0U ? 1U : 0U);
        WdgM_RunSupervisionCycle();
    }

    /* Deadline 違反のエンティティ（ENGINE）が原因で、Global Supervision Status を STOPPED まで進める。 */
    void DriveToStoppedByEngineDeadline()
    {
        ViolateEngineDeadline();
        engineNextIsStart = 1U;
        for (uint8 cycle = 0U; cycle < 8U; cycle++)
        {
            RunCycleKeepingEngineAlive();
            WdgM_GlobalStatusType global = WDGM_GLOBAL_STATUS_OK;
            (void)WdgM_GetGlobalStatus(&global);
            if (global == WDGM_GLOBAL_STATUS_STOPPED)
            {
                return;
            }
        }
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(WdgM_MainFunction_Test, MainFunction_NG_UninitializedDoesNothing)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    WdgM_DeInit();   // 未初期化にする
    FakeWdgHw_RefreshCount = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    WdgM_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeWdgHw_RefreshCount, 0U);   // ウォッチドッグへ触れない
}

TEST_F(WdgM_MainFunction_Test, MainFunction_OK_SuppressedHealthyCycleKeepsGlobalOk)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(WdgM_SetMode(WDGM_MODE_POST_RUN), E_OK);   // 監視の判定を抑制する（POST_RUN）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    WdgM_RunSupervisionCycle();   // どのエンティティも報告しないが、抑制中なので異常にはならない
    WdgM_GlobalStatusType global = WDGM_GLOBAL_STATUS_STOPPED;
    ASSERT_EQ(WdgM_GetGlobalStatus(&global), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(global, WDGM_GLOBAL_STATUS_OK);
}

TEST_F(WdgM_MainFunction_Test, MainFunction_OK_DeadlineViolationIsRecordedAsFirstExpiredEntity)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // ENGINE は Deadline 違反（Alive は毎サイクル満たす）。WARNING は何も報告せず、Alive 不足になる。

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    DriveToStoppedByEngineDeadline();
    WdgM_GlobalStatusType global = WDGM_GLOBAL_STATUS_OK;
    ASSERT_EQ(WdgM_GetGlobalStatus(&global), E_OK);
    WdgM_SupervisedEntityIdType seid = 0xFFU;
    ASSERT_EQ(WdgM_GetFirstExpiredSEID(&seid), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(global, WDGM_GLOBAL_STATUS_STOPPED);
    EXPECT_EQ(seid, WDGM_ENTITY_ENGINE);   // 走査順では WARNING も異常だが、先に確認する ENGINE（Deadline 違反）が原因として残る
}

TEST_F(WdgM_MainFunction_Test, MainFunction_OK_SuppressionKeepsRefreshingHwWatchdogEvenWhenStopped)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    DriveToStoppedByEngineDeadline();
    ASSERT_EQ(WdgM_SetMode(WDGM_MODE_POST_RUN), E_OK);   // 抑制を始める（Global は STOPPED のまま）
    FakeWdgHw_RefreshCount = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    WdgM_MainFunction();   // 判定サイクルに達しない呼び出し

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeWdgHw_RefreshCount, 1U);   // STOPPED でも、抑制中はリフレッシュする（リセットさせない）
}

TEST_F(WdgM_MainFunction_Test, MainFunction_OK_AllEntitiesOkKeepsGlobalOkWithoutRecoveryProcessing)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み、どの異常状態も立っていない）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ReportAllAliveOk();
    WdgM_RunSupervisionCycle();
    WdgM_GlobalStatusType global = WDGM_GLOBAL_STATUS_STOPPED;
    ASSERT_EQ(WdgM_GetGlobalStatus(&global), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(global, WDGM_GLOBAL_STATUS_OK);
}

TEST_F(WdgM_MainFunction_Test, MainFunction_OK_GlobalRecoversFromExpiredWhenAllEntitiesOkAgain)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // ENGINE だけが Alive 不足（WARNING は正常）を WDGM_EXPIRED_SUPERVISION_CYCLE_TOL 回続け、Global が EXPIRED になる。
    for (uint8 cycle = 0U; cycle < WDGM_EXPIRED_SUPERVISION_CYCLE_TOL; cycle++)
    {
        ReportWarningPairs(5U);
        WdgM_RunSupervisionCycle();
    }
    WdgM_GlobalStatusType expired = WDGM_GLOBAL_STATUS_OK;
    ASSERT_EQ(WdgM_GetGlobalStatus(&expired), E_OK);
    ASSERT_EQ(expired, WDGM_GLOBAL_STATUS_EXPIRED);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ReportAllAliveOk();      // ENGINE が回復する
    WdgM_RunSupervisionCycle();
    WdgM_GlobalStatusType global = WDGM_GLOBAL_STATUS_STOPPED;
    ASSERT_EQ(WdgM_GetGlobalStatus(&global), E_OK);
    WdgM_SupervisedEntityIdType seid = 0xFFU;
    const Std_ReturnType retSeid = WdgM_GetFirstExpiredSEID(&seid);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(global, WDGM_GLOBAL_STATUS_OK);   // 全エンティティが OK に戻ったので、EXPIRED から回復する
    EXPECT_NE(retSeid, E_OK);                    // 解消済みなので、原因のエンティティの記録は無効になる
}

TEST_F(WdgM_MainFunction_Test, MainFunction_OK_FirstJudgmentAfterResumeIsSkipped)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    WdgM_ResumeSupervision();   // 基準を取り直す（再開直後の判定は 1 回見送る）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    WdgM_RunSupervisionCycle();   // どのエンティティも報告していないが、見送られる
    const WdgM_LocalStatusType afterFirst = WarningStatusOf();
    WdgM_RunSupervisionCycle();   // 2 回目は判定する（報告が無いので Alive 不足）
    const WdgM_LocalStatusType afterSecond = WarningStatusOf();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(afterFirst, WDGM_LOCAL_STATUS_OK);
    EXPECT_NE(afterSecond, WDGM_LOCAL_STATUS_OK);
}

}  // namespace
