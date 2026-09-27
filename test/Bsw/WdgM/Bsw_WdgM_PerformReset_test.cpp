/**
 * \file    Bsw_WdgM_PerformReset_test.cpp
 * \brief   WdgM_PerformReset() の単体テスト
 * \details AUTOSAR SWS_WdgM_00232/00233/00264/00270 が規定する、呼び出し以降
 *          HW ウォッチドッグの trigger を二度と行わなくなる（＝リセットが
 *          確実に迫る）挙動を、WdgM_TriggerHwWatchdog() の呼び出し記録
 *          （Fake_Wdg_Hw.h の FakeWdgHw_RefreshCount。2026-09、WdgIf/Wdg
 *          自身の Det 検証のため実体リンクへ切り替え、旧 Fake_WdgIf.h の
 *          FakeWdgIf_SetTriggerConditionCount から一段深いレイヤへ移した）
 *          で検証する。本番の WdgM_Config
 *          （WdgM_PBCfg.c、Entity 0=ENGINE/Entity 1=WARNING の2エンティティ
 *          構成）をそのまま使う。
 */
#include <gtest/gtest.h>
#include "WdgM.h"
#include "Wdg.h"
#include "Wdg_PBCfg.h"
#include "Fake_Wdg_Hw.h"
#include "Wrap_Dem.h"
#include "Fake_Det_Hw.h"
#include "Fake_Millis.h"

class Bsw_WdgM_PerformReset_Test : public ::testing::Test
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
    EXPECT_EQ(FakeWdgHw_RefreshCount, 1U);

    WdgM_PerformReset();

    /* 呼び出し以降、何回呼んでも refresh されない。 */
    WdgM_TriggerHwWatchdog();
    WdgM_TriggerHwWatchdog();
    EXPECT_EQ(FakeWdgHw_RefreshCount, 1U);
}

TEST_F(Bsw_WdgM_PerformReset_Test, PerformReset_OK_OverridesSupervisionSuppression)
{
    /* POST_RUN 相当（WdgM_SupervisionSuppressed 中）は本来 refresh を継続する。 */
    WdgM_DisableHwWatchdog();
    WdgM_TriggerHwWatchdog();
    EXPECT_EQ(FakeWdgHw_RefreshCount, 1U);

    /* それでも WdgM_PerformReset() は最優先で refresh を止める
     * （[SWS_WdgM_00233]: 呼び出し後は二度とトリガ条件を更新しない）。 */
    WdgM_PerformReset();
    WdgM_TriggerHwWatchdog();
    EXPECT_EQ(FakeWdgHw_RefreshCount, 1U);
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
    EXPECT_EQ(FakeWdgHw_RefreshCount, 0U);
}
