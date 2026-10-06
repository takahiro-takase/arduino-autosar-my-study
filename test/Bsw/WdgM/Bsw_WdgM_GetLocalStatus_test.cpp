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

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>
#include "WdgM.h"
#include "Wdg.h"
#include "Wdg_PBCfg.h"
#include "Fake_Wdg_Hw.h"
#include "Wrap_Dem.h"
#include "Fake_Det_Hw.h"
#include "Fake_Millis.h"

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

/* WdgM_GetLocalStatus() の単体テスト用フィクスチャ。
 * SetUp(): 時刻・模擬 Wdg_Hw・DET・Dem の wrap を初期化し、Dem_Init(NULL)・Wdg_Init()・WdgM_Init() を EcuM_Init() と同じ順序で呼ぶ（Init 自体の DET 記録は消す）。 */
class Bsw_WdgM_GetLocalStatus_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        FakeDetHw_Reset();
        FakeWdgHw_Reset();
        WrapDem_Reset();
        Dem_Init(NULL);  // Demの内部状態を毎テスト決定的にリセットする（Fake_NvM.cにより常に「初回起動」）
        Wdg_Init(&Wdg_Config);  // WdgIf/Wdg を実体リンクへ切り替えた際に追加（EcuM_Init() と同じ順序）
        WdgM_Init(&WdgM_Config);
        FakeDetHw_Reset();  /* Init 自体が出す DET ログ・記録を後続の検証対象から除く */
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Bsw_WdgM_GetLocalStatus_Test, GetLocalStatus_OK_ReturnsOkRightAfterInit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    WdgM_LocalStatusType status = WDGM_LOCAL_STATUS_FAILED;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = WdgM_GetLocalStatus(WDGM_ENTITY_ENGINE, &status);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(status, WDGM_LOCAL_STATUS_OK);
}

TEST_F(Bsw_WdgM_GetLocalStatus_Test, GetLocalStatus_OK_ReturnsFailedAfterAliveShortfall)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    /* CheckpointReached を一切呼ばずに WdgM_MainFunction() を実行すると、
     * 両エンティティとも Alive Supervision が期待回数を満たせず FAILED になる。 */
    WdgM_MainFunction();

    WdgM_LocalStatusType status = WDGM_LOCAL_STATUS_OK;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = WdgM_GetLocalStatus(WDGM_ENTITY_WARNING, &status);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_EQ(ret, E_OK);
    EXPECT_EQ(status, WDGM_LOCAL_STATUS_FAILED);
}

TEST_F(Bsw_WdgM_GetLocalStatus_Test, GetLocalStatus_OK_ReturnsExpiredImmediatelyAfterLogicalViolation)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    /* ENGINE の許可遷移は INITIAL->START/START->END/END->START のみ。
     * INITIAL から直接 END へ遷移させ、Logical Supervision 違反を即座に起こす。
     * WdgM_MainFunction() を待たず、この時点で既に EXPIRED になる
     * ([SWS_WdgM_00202]/[SWS_WdgM_00206]: Logical/Deadline には Alive と
     * 異なり猶予サイクルが無く、検出した瞬間に直接 EXPIRED へ遷移する)。 */
    Std_ReturnType cpRet = WdgM_CheckpointReached(WDGM_ENTITY_ENGINE, WDGM_CP_ENGINE_END);
    ASSERT_EQ(cpRet, E_OK);

    WdgM_LocalStatusType status = WDGM_LOCAL_STATUS_OK;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = WdgM_GetLocalStatus(WDGM_ENTITY_ENGINE, &status);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_EQ(ret, E_OK);
    EXPECT_EQ(status, WDGM_LOCAL_STATUS_EXPIRED);
}

TEST_F(Bsw_WdgM_GetLocalStatus_Test, GetLocalStatus_OK_ReturnsExpiredAfterSustainedAliveShortfall)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    /* Alive Supervision のみは猶予サイクルを持つ簡易実装
     * (WDGM_EXPIRED_SUPERVISION_CYCLE_TOL=2、WdgM_Cfg.h 参照)。
     * 1 回目の判定サイクルは FAILED のまま
     * (GetLocalStatus_OK_ReturnsFailedAfterAliveShortfall で検証済み)、
     * 2 回連続で Alive 不足が続くと EXPIRED へ遷移する。 */
    WdgM_MainFunction();
    WdgM_MainFunction();

    WdgM_LocalStatusType status = WDGM_LOCAL_STATUS_OK;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = WdgM_GetLocalStatus(WDGM_ENTITY_WARNING, &status);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_EQ(ret, E_OK);
    EXPECT_EQ(status, WDGM_LOCAL_STATUS_EXPIRED);
}

TEST_F(Bsw_WdgM_GetLocalStatus_Test, GetLocalStatus_OK_StaysFailedDuringSuppressionRegardlessOfCycleCount)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    /* WdgM_SupervisionSuppressed 中（POST_RUN 中の意図的な Alive 不足）は、
     * エンティティ単位の EXPIRED 猶予カウンタもグローバル側と同じく凍結される
     * べき（/code-review で指摘、修正済み）。猶予サイクル数を大幅に超えて
     * 抑制状態が続いても FAILED のまま EXPIRED へエスカレーションしないことを
     * 確認する。 */
    WdgM_DisableHwWatchdog();

    for (uint8 i = 0U; i < WDGM_EXPIRED_SUPERVISION_CYCLE_TOL + 3U; i++)
        WdgM_MainFunction();

    WdgM_LocalStatusType status = WDGM_LOCAL_STATUS_OK;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType retSuppressed = WdgM_GetLocalStatus(WDGM_ENTITY_WARNING, &status);
    WdgM_LocalStatusType statusSuppressed = status;

    /* 抑制解除後は、凍結されていた分の蓄積がいきなり反映されるのではなく、
     * 解除後にあらためて WDGM_EXPIRED_SUPERVISION_CYCLE_TOL 判定サイクル分の
     * 継続 FAILED を要して EXPIRED へ遷移することを確認する。 */
    WdgM_EnableHwWatchdog();
    WdgM_MainFunction();
    Std_ReturnType retCycle1 = WdgM_GetLocalStatus(WDGM_ENTITY_WARNING, &status);
    WdgM_LocalStatusType statusCycle1 = status;

    WdgM_MainFunction();
    Std_ReturnType retCycle2 = WdgM_GetLocalStatus(WDGM_ENTITY_WARNING, &status);
    WdgM_LocalStatusType statusCycle2 = status;

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_EQ(retSuppressed, E_OK);
    EXPECT_EQ(statusSuppressed, WDGM_LOCAL_STATUS_FAILED);

    ASSERT_EQ(retCycle1, E_OK);
    EXPECT_EQ(statusCycle1, WDGM_LOCAL_STATUS_FAILED);

    ASSERT_EQ(retCycle2, E_OK);
    EXPECT_EQ(statusCycle2, WDGM_LOCAL_STATUS_EXPIRED);
}

TEST_F(Bsw_WdgM_GetLocalStatus_Test, GetLocalStatus_OK_ReturnsOkAfterAliveRecoveryResetsExpiredCounter)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    /* 猶予カウンタ消費中でも、Alive Supervision が実際に回復すれば即座に
     * OK へ戻り、猶予カウンタもリセットされる（Global 側と同じ単純な
     * 二値復帰。実仕様の 1 段階ずつの減算は簡略化のため採用していない）。 */
    WdgM_MainFunction();
    WdgM_MainFunction();
    WdgM_LocalStatusType expiredStatus = WDGM_LOCAL_STATUS_OK;
    ASSERT_EQ(WdgM_GetLocalStatus(WDGM_ENTITY_WARNING, &expiredStatus), E_OK);
    ASSERT_EQ(expiredStatus, WDGM_LOCAL_STATUS_EXPIRED);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
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
    Std_ReturnType ret = WdgM_GetLocalStatus(WDGM_ENTITY_WARNING, &status);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_EQ(ret, E_OK);
    EXPECT_EQ(status, WDGM_LOCAL_STATUS_OK);
}

TEST_F(Bsw_WdgM_GetLocalStatus_Test, GetLocalStatus_NG_NullPointerReturnsErrorAndReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = WdgM_GetLocalStatus(WDGM_ENTITY_ENGINE, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, WDGM_E_INV_POINTER);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);
}

TEST_F(Bsw_WdgM_GetLocalStatus_Test, GetLocalStatus_NG_UninitializedReturnsDeactivatedAndReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    WdgM_DeInit();
    FakeDetHw_Reset();

    WdgM_LocalStatusType status = WDGM_LOCAL_STATUS_OK;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = WdgM_GetLocalStatus(WDGM_ENTITY_ENGINE, &status);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(status, WDGM_LOCAL_STATUS_DEACTIVATED);
    EXPECT_EQ(FakeDetHw_LastErrorId, WDGM_E_NO_INIT);
}

TEST_F(Bsw_WdgM_GetLocalStatus_Test, GetLocalStatus_NG_InvalidSeidReturnsDeactivatedAndReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    WdgM_LocalStatusType status = WDGM_LOCAL_STATUS_OK;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = WdgM_GetLocalStatus(WdgM_Config.EntityCount, &status);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(status, WDGM_LOCAL_STATUS_DEACTIVATED);
    EXPECT_EQ(FakeDetHw_LastErrorId, WDGM_E_PARAM_SEID);
}
