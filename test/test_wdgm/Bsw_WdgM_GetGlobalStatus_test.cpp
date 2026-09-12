/**
 * \file    Bsw_WdgM_GetGlobalStatus_test.cpp
 * \brief   WdgM_GetGlobalStatus() の単体テスト
 * \details AUTOSAR SWS_WdgM_00360 が規定する4状態
 *          (OK/FAILED/EXPIRED/STOPPED、+ 未初期化時の DEACTIVATED) それぞれへの
 *          遷移を、既存の内部状態 (WdgM_GetLocalStatus()・WdgM_ExpiredCycleCount・
 *          WdgM_GlobalStopped) を集約する形で検証する。
 *          本番の WdgM_Config (WdgM_PBCfg.c、Entity 0=ENGINE/Entity 1=WARNING の
 *          2エンティティ構成) をそのまま使う。
 */
#include <gtest/gtest.h>
#include "WdgM.h"
#include "WdgIf_fake.h"
#include "Hal_Det_Hw_fake.h"
#include "Hal_Millis_fake.h"

class Bsw_WdgM_GetGlobalStatus_Test : public ::testing::Test
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

    /* CheckpointReached を一切呼ばずに両エンティティの Alive Supervision を
     * 失敗させ続け、STOPPED まで駆動するヘルパー。
     *
     * 2026-09 是正: エンティティ単位の EXPIRED 猶予
     * (WdgM_EntityExpiredCycleCount、Alive の連続 FAILED サイクル数が
     * WDGM_EXPIRED_SUPERVISION_CYCLE_TOL に達するまで) を使い切るまでは
     * Local Status はまだ FAILED であり Global も FAILED のまま
     * （[SWS_WdgM_00076]/[00217]）。Local Status が実際に EXPIRED へ
     * 昇格した周期で初めて Global も EXPIRED へラッチし
     * （[SWS_WdgM_00215]/[00077]）、そこから改めて
     * WDGM_EXPIRED_SUPERVISION_CYCLE_TOL 回（ラッチした周期を含め計 TOL+1
     * 周期）の間 EXPIRED を維持したのち STOPPED に遷移する
     * （[SWS_WdgM_00219]/[00220]）。以前はこの2段階の猶予が単一のカウンタに
     * 誤って統合されていたため、Alive が FAILED になった最初の1周期で
     * 即座に EXPIRED と誤判定されていた。 */
    void DriveAllEntitiesToStopped()
    {
        for (uint8 cycle = 0U; cycle < WDGM_EXPIRED_SUPERVISION_CYCLE_TOL - 1U; cycle++)
        {
            WdgM_MainFunction();
            WdgM_GlobalStatusType status;
            ASSERT_EQ(WdgM_GetGlobalStatus(&status), E_OK);
            EXPECT_EQ(status, WDGM_GLOBAL_STATUS_FAILED) << "pre-expired cycle " << (unsigned)cycle;
        }

        for (uint8 cycle = 0U; cycle < WDGM_EXPIRED_SUPERVISION_CYCLE_TOL + 1U; cycle++)
        {
            WdgM_MainFunction();
            WdgM_GlobalStatusType status;
            ASSERT_EQ(WdgM_GetGlobalStatus(&status), E_OK);
            EXPECT_EQ(status, WDGM_GLOBAL_STATUS_EXPIRED) << "expired cycle " << (unsigned)cycle;
        }

        /* グローバル猶予カウンタが尽きて初めて WdgM_GlobalStopped が立ち
         * STOPPED に遷移する。 */
        WdgM_MainFunction();
        WdgM_GlobalStatusType status;
        ASSERT_EQ(WdgM_GetGlobalStatus(&status), E_OK);
        EXPECT_EQ(status, WDGM_GLOBAL_STATUS_STOPPED);
    }
};

TEST_F(Bsw_WdgM_GetGlobalStatus_Test, GetGlobalStatus_NG_NullPointerReturnsErrorAndReportsDet)
{
    Std_ReturnType ret = WdgM_GetGlobalStatus(NULL);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, WDGM_E_INV_POINTER);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);
}

TEST_F(Bsw_WdgM_GetGlobalStatus_Test, GetGlobalStatus_NG_UninitializedReturnsDeactivatedAndReportsDet)
{
    WdgM_DeInit();
    FakeDetHw_Reset();

    WdgM_GlobalStatusType status = WDGM_GLOBAL_STATUS_OK;
    Std_ReturnType ret = WdgM_GetGlobalStatus(&status);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(status, WDGM_GLOBAL_STATUS_DEACTIVATED);
    EXPECT_EQ(FakeDetHw_LastErrorId, WDGM_E_NO_INIT);
}

TEST_F(Bsw_WdgM_GetGlobalStatus_Test, GetGlobalStatus_OK_ReturnsOkRightAfterInit)
{
    WdgM_GlobalStatusType status = WDGM_GLOBAL_STATUS_STOPPED;
    Std_ReturnType ret = WdgM_GetGlobalStatus(&status);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(status, WDGM_GLOBAL_STATUS_OK);
}

TEST_F(Bsw_WdgM_GetGlobalStatus_Test,
       GetGlobalStatus_OK_ReturnsOkDuringSuppressionEvenWhileEntitiesAreFailing)
{
    /* POST_RUN 突入相当。Rte_Engine/Rte_Warning が意図的に停止するのを模して
     * 一切 CheckpointReached を呼ばないまま WdgM_MainFunction() を回す。 */
    WdgM_DisableHwWatchdog();

    WdgM_MainFunction();
    WdgM_MainFunction();
    WdgM_MainFunction();

    /* 抑制中はグローバル猶予カウンタが凍結されるため anyNotOk はまだ立って
     * いるはずだが（Alive Supervision は実際に FAILED になる）、
     * WdgM_TriggerHwWatchdog() が無条件に refresh を続けるのと同じ理由で、
     * WdgM_GetGlobalStatus() も OK を返すべき（本物の異常ではないため）。 */
    WdgM_GlobalStatusType status = WDGM_GLOBAL_STATUS_STOPPED;
    Std_ReturnType ret = WdgM_GetGlobalStatus(&status);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(status, WDGM_GLOBAL_STATUS_OK);
}

TEST_F(Bsw_WdgM_GetGlobalStatus_Test,
       GetGlobalStatus_OK_ReturnsFailedAfterLogicalViolationBeforeNextMainFunctionCycle)
{
    /* ENGINE の許可遷移は INITIAL->START/START->END/END->START のみ。
     * INITIAL から直接 END へ遷移させ、Logical Supervision 違反を即座に起こす。 */
    Std_ReturnType cpRet = WdgM_CheckpointReached(WDGM_ENTITY_ENGINE, WDGM_CP_ENGINE_END);
    ASSERT_EQ(cpRet, E_OK);

    /* WdgM_MainFunction() はまだ呼んでいないため、Global Supervision Status
     * は([SWS_WdgM_00214]により毎周期1回だけ計算される値のため)まだ更新
     * されていない。ローカル違反だけが即座に検出された「FAILED」状態を
     * 観測できるはず（2026-09 是正後も本テストの期待値自体は変わらない。
     * WdgM_GlobalExpired は WdgM_MainFunction() 内でのみラッチされるため）。 */
    WdgM_GlobalStatusType status = WDGM_GLOBAL_STATUS_OK;
    Std_ReturnType ret = WdgM_GetGlobalStatus(&status);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(status, WDGM_GLOBAL_STATUS_FAILED);
}

TEST_F(Bsw_WdgM_GetGlobalStatus_Test,
       GetGlobalStatus_OK_BecomesExpiredOnFirstMainFunctionCycleAfterLogicalViolation)
{
    /* [SWS_WdgM_00215]/[00077] の回帰テスト(2026-09 追加): Logical/Deadline
     * 違反は Alive Supervision と異なり猶予なしで即座に Local Status が
     * EXPIRED になるため([SWS_WdgM_00202])、Global は Alive のような
     * 複数周期のランプアップ無しで、違反後最初の WdgM_MainFunction() 呼び出し
     * 1回で直接 EXPIRED へラッチするべき（是正前は「いずれかのエンティティが
     * FAILED（EXPIRED か否か問わず）」というグローバル猶予カウンタでしか
     * EXPIRED を判定できず、Alive 用の複数周期ぶんの猶予を誤って必要として
     * いた）。 */
    Std_ReturnType cpRet = WdgM_CheckpointReached(WDGM_ENTITY_ENGINE, WDGM_CP_ENGINE_END);
    ASSERT_EQ(cpRet, E_OK);

    WdgM_MainFunction();

    WdgM_GlobalStatusType status;
    ASSERT_EQ(WdgM_GetGlobalStatus(&status), E_OK);
    EXPECT_EQ(status, WDGM_GLOBAL_STATUS_EXPIRED);
}

TEST_F(Bsw_WdgM_GetGlobalStatus_Test, GetGlobalStatus_OK_ExpiresThenStopsAfterToleranceExhausted)
{
    DriveAllEntitiesToStopped();
}

TEST_F(Bsw_WdgM_GetGlobalStatus_Test,
       GetGlobalStatus_OK_RecoversToOkAfterStoppedOnceAliveSupervisionIsSatisfied)
{
    DriveAllEntitiesToStopped();

    /* 両エンティティの Alive Supervision を満たす（Logical/Deadline は違反させない
     * よう、許可された遷移のみを使い、Deadline 許容範囲内になるよう millis() を
     * 進める）。ENGINE は期待回数 1 回、WARNING は期待回数 6 回。 */
    WdgM_CheckpointReached(WDGM_ENTITY_ENGINE, WDGM_CP_ENGINE_START);  /* Alive=1 (期待値 1 を満たす) */

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

    /* WdgM_AliveStatus[] は WdgM_MainFunction() の中でしか再評価されないため、
     * この時点ではまだ DriveAllEntitiesToStopped() 内の最終サイクル時点の
     * FAILED が残っている (WdgM_GetLocalStatus() での確認は次の
     * WdgM_MainFunction() 実行後に行う)。 */

    /* AliveCount が両エンティティとも期待値を満たしたことで Alive Supervision
     * が回復し、Logical/Deadline も違反させていないため、全エンティティが
     * OK に戻る。これによりグローバル猶予カウンタと STOPPED フラグの両方が
     * クリアされ、OK に回復する。 */
    WdgM_MainFunction();
    WdgM_LocalStatusType engineStatus;
    WdgM_LocalStatusType warningStatus;
    ASSERT_EQ(WdgM_GetLocalStatus(WDGM_ENTITY_ENGINE, &engineStatus), E_OK);
    ASSERT_EQ(WdgM_GetLocalStatus(WDGM_ENTITY_WARNING, &warningStatus), E_OK);
    EXPECT_EQ(engineStatus, WDGM_LOCAL_STATUS_OK);
    EXPECT_EQ(warningStatus, WDGM_LOCAL_STATUS_OK);

    WdgM_GlobalStatusType status;
    ASSERT_EQ(WdgM_GetGlobalStatus(&status), E_OK);
    EXPECT_EQ(status, WDGM_GLOBAL_STATUS_OK);
}
