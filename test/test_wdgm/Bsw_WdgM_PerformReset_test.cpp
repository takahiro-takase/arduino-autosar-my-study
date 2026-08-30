/**
 * \file    Bsw_WdgM_PerformReset_test.cpp
 * \brief   WdgM_PerformReset() の単体テスト
 * \details AUTOSAR SWS_WdgM_00232/00233/00264/00270 が規定する、呼び出し以降
 *          HW ウォッチドッグの trigger を二度と行わなくなる（＝リセットが
 *          確実に迫る）挙動を、WdgM_TriggerHwWatchdog() の呼び出し記録
 *          （WdgIf_fake.h）で検証する。本番の WdgM_Config
 *          （WdgM_PBCfg.c、Entity 0=ENGINE/Entity 1=WARNING の2エンティティ
 *          構成）をそのまま使う。
 */
#include <gtest/gtest.h>
#include "WdgM.h"
#include "WdgIf_fake.h"
#include "Hal_Det_Hw_fake.h"
#include "Hal_Millis_fake.h"

class Bsw_WdgM_PerformReset_Test : public ::testing::Test
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

TEST_F(Bsw_WdgM_PerformReset_Test, PerformReset_NG_UninitializedReportsDetWithoutEffect)
{
    WdgM_DeInit();
    FakeDetHw_Reset();

    WdgM_PerformReset();

    EXPECT_EQ(FakeDetHw_LastErrorId, WDGM_E_NO_INIT);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);

    /* 未初期化時は無効果（[SWS_WdgM_00270]）。WdgM_TriggerHwWatchdog() 自体は
     * Cfg==NULL のガードで別途早期 return するため、ここでは呼ばない。 */
}

TEST_F(Bsw_WdgM_PerformReset_Test, PerformReset_OK_StopsHwWatchdogRefreshImmediately)
{
    /* 通常時は refresh される。 */
    WdgM_TriggerHwWatchdog();
    EXPECT_EQ(FakeWdgIf_SetTriggerConditionCount, 1U);

    WdgM_PerformReset();

    /* 呼び出し以降、何回呼んでも refresh されない。 */
    WdgM_TriggerHwWatchdog();
    WdgM_TriggerHwWatchdog();
    EXPECT_EQ(FakeWdgIf_SetTriggerConditionCount, 1U);
}

TEST_F(Bsw_WdgM_PerformReset_Test, PerformReset_OK_OverridesSupervisionSuppression)
{
    /* POST_RUN 相当（WdgM_SupervisionSuppressed 中）は本来 refresh を継続する。 */
    WdgM_DisableHwWatchdog();
    WdgM_TriggerHwWatchdog();
    EXPECT_EQ(FakeWdgIf_SetTriggerConditionCount, 1U);

    /* それでも WdgM_PerformReset() は最優先で refresh を止める
     * （[SWS_WdgM_00233]: 呼び出し後は二度とトリガ条件を更新しない）。 */
    WdgM_PerformReset();
    WdgM_TriggerHwWatchdog();
    EXPECT_EQ(FakeWdgIf_SetTriggerConditionCount, 1U);
}

TEST_F(Bsw_WdgM_PerformReset_Test, PerformReset_OK_NotUndoneByMainFunctionRecovery)
{
    WdgM_PerformReset();

    /* WdgM_MainFunction() が何度動いても（＝WdgM_GlobalStopped 側の自然回復
     * 条件を再評価しても）、WdgM_ResetRequested は独立したフラグのため
     * 巻き戻らない。判定結果自体（OK/FAILED）はここでは関知しない。 */
    WdgM_MainFunction();
    WdgM_MainFunction();

    WdgM_TriggerHwWatchdog();
    EXPECT_EQ(FakeWdgIf_SetTriggerConditionCount, 0U);
}
