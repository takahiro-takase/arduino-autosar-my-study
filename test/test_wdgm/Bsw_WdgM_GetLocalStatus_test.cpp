/**
 * \file    Bsw_WdgM_GetLocalStatus_test.cpp
 * \brief   WdgM_GetLocalStatus() の単体テスト
 * \details AUTOSAR SWS_WdgM_00169 準拠のシグネチャ
 *          (`Std_ReturnType WdgM_GetLocalStatus(SEID, WdgM_LocalStatusType* Status)`)
 *          へ変更した際に新設。異常系 (NULL ポインタ・未初期化・SEID 不正) と、
 *          Alive/Logical Supervision の結果反映を検証する。
 *          本番の WdgM_Config (WdgM_PBCfg.c、Entity 0=ENGINE/Entity 1=WARNING の
 *          2エンティティ構成) をそのまま使う。
 */
#include <gtest/gtest.h>
#include "WdgM.h"
#include "WdgIf_fake.h"
#include "Hal_Det_Hw_fake.h"
#include "Hal_Millis_fake.h"

class Bsw_WdgM_GetLocalStatus_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        FakeDetHw_Reset();
        FakeWdgIf_Reset();
        WdgM_Init(&WdgM_Config);
        FakeDetHw_Reset();  /* Init 自体が出す DET ログ・記録を後続の検証対象から除く */
    }
};

TEST_F(Bsw_WdgM_GetLocalStatus_Test, GetLocalStatus_NG_NullPointerReturnsErrorAndReportsDet)
{
    Std_ReturnType ret = WdgM_GetLocalStatus(WDGM_ENTITY_ENGINE, NULL);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, WDGM_E_INV_POINTER);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);
}

TEST_F(Bsw_WdgM_GetLocalStatus_Test, GetLocalStatus_NG_UninitializedReturnsDeactivatedAndReportsDet)
{
    WdgM_DeInit();
    FakeDetHw_Reset();

    WdgM_LocalStatusType status = WDGM_LOCAL_STATUS_OK;
    Std_ReturnType ret = WdgM_GetLocalStatus(WDGM_ENTITY_ENGINE, &status);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(status, WDGM_LOCAL_STATUS_DEACTIVATED);
    EXPECT_EQ(FakeDetHw_LastErrorId, WDGM_E_NO_INIT);
}

TEST_F(Bsw_WdgM_GetLocalStatus_Test, GetLocalStatus_NG_InvalidSeidReturnsDeactivatedAndReportsDet)
{
    WdgM_LocalStatusType status = WDGM_LOCAL_STATUS_OK;
    Std_ReturnType ret = WdgM_GetLocalStatus(WdgM_Config.EntityCount, &status);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(status, WDGM_LOCAL_STATUS_DEACTIVATED);
    EXPECT_EQ(FakeDetHw_LastErrorId, WDGM_E_PARAM_SEID);
}

TEST_F(Bsw_WdgM_GetLocalStatus_Test, GetLocalStatus_OK_ReturnsOkRightAfterInit)
{
    WdgM_LocalStatusType status = WDGM_LOCAL_STATUS_FAILED;
    Std_ReturnType ret = WdgM_GetLocalStatus(WDGM_ENTITY_ENGINE, &status);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(status, WDGM_LOCAL_STATUS_OK);
}

TEST_F(Bsw_WdgM_GetLocalStatus_Test, GetLocalStatus_OK_ReturnsFailedAfterAliveShortfall)
{
    /* CheckpointReached を一切呼ばずに WdgM_MainFunction() を実行すると、
     * 両エンティティとも Alive Supervision が期待回数を満たせず FAILED になる。 */
    WdgM_MainFunction();

    WdgM_LocalStatusType status = WDGM_LOCAL_STATUS_OK;
    ASSERT_EQ(WdgM_GetLocalStatus(WDGM_ENTITY_WARNING, &status), E_OK);
    EXPECT_EQ(status, WDGM_LOCAL_STATUS_FAILED);
}

TEST_F(Bsw_WdgM_GetLocalStatus_Test, GetLocalStatus_OK_ReturnsExpiredImmediatelyAfterLogicalViolation)
{
    /* ENGINE の許可遷移は INITIAL->START/START->END/END->START のみ。
     * INITIAL から直接 END へ遷移させ、Logical Supervision 違反を即座に起こす。
     * WdgM_MainFunction() を待たず、この時点で既に EXPIRED になる
     * ([SWS_WdgM_00202]/[SWS_WdgM_00206]: Logical/Deadline には Alive と
     * 異なり猶予サイクルが無く、検出した瞬間に直接 EXPIRED へ遷移する)。 */
    Std_ReturnType cpRet = WdgM_CheckpointReached(WDGM_ENTITY_ENGINE, WDGM_CP_ENGINE_END);
    ASSERT_EQ(cpRet, E_OK);

    WdgM_LocalStatusType status = WDGM_LOCAL_STATUS_OK;
    ASSERT_EQ(WdgM_GetLocalStatus(WDGM_ENTITY_ENGINE, &status), E_OK);
    EXPECT_EQ(status, WDGM_LOCAL_STATUS_EXPIRED);
}

TEST_F(Bsw_WdgM_GetLocalStatus_Test, GetLocalStatus_OK_ReturnsExpiredAfterSustainedAliveShortfall)
{
    /* Alive Supervision のみは猶予サイクルを持つ簡易実装
     * (WDGM_EXPIRED_SUPERVISION_CYCLE_TOL=2、WdgM_Cfg.h 参照)。
     * 1 回目の判定サイクルは FAILED のまま
     * (GetLocalStatus_OK_ReturnsFailedAfterAliveShortfall で検証済み)、
     * 2 回連続で Alive 不足が続くと EXPIRED へ遷移する。 */
    WdgM_MainFunction();
    WdgM_MainFunction();

    WdgM_LocalStatusType status = WDGM_LOCAL_STATUS_OK;
    ASSERT_EQ(WdgM_GetLocalStatus(WDGM_ENTITY_WARNING, &status), E_OK);
    EXPECT_EQ(status, WDGM_LOCAL_STATUS_EXPIRED);
}

TEST_F(Bsw_WdgM_GetLocalStatus_Test, GetLocalStatus_OK_StaysFailedDuringSuppressionRegardlessOfCycleCount)
{
    /* WdgM_SupervisionSuppressed 中（POST_RUN 中の意図的な Alive 不足）は、
     * エンティティ単位の EXPIRED 猶予カウンタもグローバル側と同じく凍結される
     * べき（/code-review で指摘、修正済み）。猶予サイクル数を大幅に超えて
     * 抑制状態が続いても FAILED のまま EXPIRED へエスカレーションしないことを
     * 確認する。 */
    WdgM_DisableHwWatchdog();

    for (uint8 i = 0U; i < WDGM_EXPIRED_SUPERVISION_CYCLE_TOL + 3U; i++)
        WdgM_MainFunction();

    WdgM_LocalStatusType status = WDGM_LOCAL_STATUS_OK;
    ASSERT_EQ(WdgM_GetLocalStatus(WDGM_ENTITY_WARNING, &status), E_OK);
    EXPECT_EQ(status, WDGM_LOCAL_STATUS_FAILED);

    /* 抑制解除後は、凍結されていた分の蓄積がいきなり反映されるのではなく、
     * 解除後にあらためて WDGM_EXPIRED_SUPERVISION_CYCLE_TOL 判定サイクル分の
     * 継続 FAILED を要して EXPIRED へ遷移することを確認する。 */
    WdgM_EnableHwWatchdog();
    WdgM_MainFunction();
    ASSERT_EQ(WdgM_GetLocalStatus(WDGM_ENTITY_WARNING, &status), E_OK);
    EXPECT_EQ(status, WDGM_LOCAL_STATUS_FAILED);

    WdgM_MainFunction();
    ASSERT_EQ(WdgM_GetLocalStatus(WDGM_ENTITY_WARNING, &status), E_OK);
    EXPECT_EQ(status, WDGM_LOCAL_STATUS_EXPIRED);
}

TEST_F(Bsw_WdgM_GetLocalStatus_Test, GetLocalStatus_OK_ReturnsOkAfterAliveRecoveryResetsExpiredCounter)
{
    /* 猶予カウンタ消費中でも、Alive Supervision が実際に回復すれば即座に
     * OK へ戻り、猶予カウンタもリセットされる（Global 側と同じ単純な
     * 二値復帰。実仕様の 1 段階ずつの減算は簡略化のため採用していない）。 */
    WdgM_MainFunction();
    WdgM_MainFunction();
    WdgM_LocalStatusType expiredStatus = WDGM_LOCAL_STATUS_OK;
    ASSERT_EQ(WdgM_GetLocalStatus(WDGM_ENTITY_WARNING, &expiredStatus), E_OK);
    ASSERT_EQ(expiredStatus, WDGM_LOCAL_STATUS_EXPIRED);

    /* Logical/Deadline を違反させないよう、許可された遷移のみを使い、
     * Deadline 許容範囲内になるよう millis() を進める
     * (Bsw_WdgM_GetGlobalStatus_test.cpp の同種テストと同じ手順)。 */
    WdgM_CheckpointReached(WDGM_ENTITY_WARNING, WDGM_CP_WARNING_START);  /* INITIAL->START: Deadline対象外 */
    FakeMillis_Value += 50UL;
    WdgM_CheckpointReached(WDGM_ENTITY_WARNING, WDGM_CP_WARNING_END);    /* START->END: [0,200]ms 以内 */
    FakeMillis_Value += 500UL;
    WdgM_CheckpointReached(WDGM_ENTITY_WARNING, WDGM_CP_WARNING_START);  /* END->START: [300,1500]ms 以内 */
    FakeMillis_Value += 50UL;
    WdgM_CheckpointReached(WDGM_ENTITY_WARNING, WDGM_CP_WARNING_END);
    FakeMillis_Value += 500UL;
    WdgM_CheckpointReached(WDGM_ENTITY_WARNING, WDGM_CP_WARNING_START);
    FakeMillis_Value += 50UL;
    WdgM_CheckpointReached(WDGM_ENTITY_WARNING, WDGM_CP_WARNING_END);    /* Alive=6 (期待値 6 を満たす) */
    WdgM_MainFunction();

    WdgM_LocalStatusType status = WDGM_LOCAL_STATUS_FAILED;
    ASSERT_EQ(WdgM_GetLocalStatus(WDGM_ENTITY_WARNING, &status), E_OK);
    EXPECT_EQ(status, WDGM_LOCAL_STATUS_OK);
}
