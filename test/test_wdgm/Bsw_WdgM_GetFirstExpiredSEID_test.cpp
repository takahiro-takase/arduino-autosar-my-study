/**
 * \file    Bsw_WdgM_GetFirstExpiredSEID_test.cpp
 * \brief   WdgM_GetFirstExpiredSEID() の単体テスト
 * \details AUTOSAR SWS_WdgM_00346〜00349 が規定する、直前の HW ウォッチドッグ
 *          リセット原因を .noinit 領域の二重反転値で判定する挙動を検証する。
 *          `WdgM_Test_SetFirstExpiredSEIDRaw()`（WDGM_UNIT_TEST 限定アクセサ）で
 *          直接 .noinit 領域を操作し、「有効な値が残っている」「初回起動・
 *          電源断相当の不定値」の両方を模擬する。本番の WdgM_Config
 *          （WdgM_PBCfg.c、Entity 0=ENGINE/Entity 1=WARNING の2エンティティ
 *          構成）をそのまま使う。
 */
#include <gtest/gtest.h>
#include "WdgM.h"
#include "WdgIf_fake.h"
#include "Hal_Det_Hw_fake.h"
#include "Hal_Millis_fake.h"

class Bsw_WdgM_GetFirstExpiredSEID_Test : public ::testing::Test
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

    /* ENGINE には一切 CheckpointReached を呼ばず、Alive Supervision 不足で
     * FAILED のままにする。WARNING は許可された遷移のみを使い、Deadline
     * 範囲内に収まる実績のあるチェックポイント列
     * （Bsw_WdgM_GetGlobalStatus_test.cpp の RecoversToOk テストと同じ）を
     * 毎判定サイクル分繰り返して OK を維持する。サイクル境界（前サイクル末尾の
     * END から今サイクル先頭の START まで）にも END->START の Deadline 許容
     * 範囲 [300,1500]ms を満たす経過時間を入れる（入れないとサイクル2周目
     * 以降で WARNING の Deadline Supervision も意図せず FAILED ラッチしてしまう）。
     * WDGM_EXPIRED_SUPERVISION_CYCLE_TOL 回分の判定サイクルを消費させ、
     * STOPPED へ到達させる。走査順（SEID 昇順）では ENGINE（SEID=0）が
     * WARNING（SEID=1）より先に確認されるため、記録される SEID は
     * ENGINE になるはず（/simplify で重複ループの指摘を受け、共通ヘルパーへ
     * 抽出した）。 */
    void DriveEngineToStoppedViaWarningAliveOnly()
    {
        for (uint8 cycle = 0U; cycle <= WDGM_EXPIRED_SUPERVISION_CYCLE_TOL; cycle++)
        {
            FakeMillis_Value += 500UL;
            WdgM_CheckpointReached(WDGM_ENTITY_WARNING, WDGM_CP_WARNING_START);
            FakeMillis_Value += 50UL;
            WdgM_CheckpointReached(WDGM_ENTITY_WARNING, WDGM_CP_WARNING_END);
            FakeMillis_Value += 500UL;
            WdgM_CheckpointReached(WDGM_ENTITY_WARNING, WDGM_CP_WARNING_START);
            FakeMillis_Value += 50UL;
            WdgM_CheckpointReached(WDGM_ENTITY_WARNING, WDGM_CP_WARNING_END);
            FakeMillis_Value += 500UL;
            WdgM_CheckpointReached(WDGM_ENTITY_WARNING, WDGM_CP_WARNING_START);
            FakeMillis_Value += 50UL;
            WdgM_CheckpointReached(WDGM_ENTITY_WARNING, WDGM_CP_WARNING_END);

            WdgM_MainFunction();
        }

        WdgM_GlobalStatusType globalStatus;
        ASSERT_EQ(WdgM_GetGlobalStatus(&globalStatus), E_OK);
        ASSERT_EQ(globalStatus, WDGM_GLOBAL_STATUS_STOPPED);
        WdgM_SupervisedEntityIdType seid = 0xFFU;
        ASSERT_EQ(WdgM_GetFirstExpiredSEID(&seid), E_OK);
        ASSERT_EQ(seid, WDGM_ENTITY_ENGINE);
    }
};

TEST_F(Bsw_WdgM_GetFirstExpiredSEID_Test, GetFirstExpiredSEID_NG_NullPointerReturnsErrorAndReportsDet)
{
    Std_ReturnType ret = WdgM_GetFirstExpiredSEID(NULL);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, WDGM_E_INV_POINTER);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);
}

TEST_F(Bsw_WdgM_GetFirstExpiredSEID_Test, GetFirstExpiredSEID_NG_UninitializedStillWorksAndReturnsError)
{
    /* [SWS_WdgM_00348]: WdgM_Init() 前でも呼び出せる。ここでは
     * 「不定値のまま（未書き込み）」を模擬し、値/反転値を意図的に
     * 矛盾させて E_NOT_OK を確認する。 */
    WdgM_DeInit();
    WdgM_Test_SetFirstExpiredSEIDRaw(1U, 1U);  /* 反転関係になっていない不正値 */
    FakeDetHw_Reset();

    WdgM_SupervisedEntityIdType seid = 0xFFU;
    Std_ReturnType ret = WdgM_GetFirstExpiredSEID(&seid);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(seid, 0U);
    /* 未初期化ガードを行わない仕様のため DET は報告されない。 */
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(Bsw_WdgM_GetFirstExpiredSEID_Test, GetFirstExpiredSEID_OK_ReturnsValueWhenInverseMatches)
{
    WdgM_Test_SetFirstExpiredSEIDRaw(WDGM_ENTITY_WARNING, (WdgM_SupervisedEntityIdType)(~WDGM_ENTITY_WARNING));

    WdgM_SupervisedEntityIdType seid = 0xFFU;
    Std_ReturnType ret = WdgM_GetFirstExpiredSEID(&seid);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(seid, WDGM_ENTITY_WARNING);
}

TEST_F(Bsw_WdgM_GetFirstExpiredSEID_Test, GetFirstExpiredSEID_OK_LatchedAutomaticallyWhenGlobalSupervisionStops)
{
    DriveEngineToStoppedViaWarningAliveOnly();

    WdgM_SupervisedEntityIdType seid = 0xFFU;
    Std_ReturnType ret = WdgM_GetFirstExpiredSEID(&seid);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(seid, WDGM_ENTITY_ENGINE);
}

TEST_F(Bsw_WdgM_GetFirstExpiredSEID_Test,
       GetFirstExpiredSEID_NG_InvalidatedIfGlobalStatusRecoversBeforeActualReset)
{
    DriveEngineToStoppedViaWarningAliveOnly();

    /* 実 HW リセットに至る前に（本テストは実 HW を持たないため無条件に）
     * ENGINE も Alive Supervision を満たすようにし、両エンティティが OK に
     * 戻る（＝実機なら WdgM_DisableHwWatchdog() 等でリセットを免れて回復した
     * ケースに相当）。 */
    WdgM_CheckpointReached(WDGM_ENTITY_ENGINE, WDGM_CP_ENGINE_START);
    /* WARNING の直前チェックポイントは上のループの最後の END（[300,1500]ms
     * 経過後に START へ遷移する必要がある）。経過させずに呼ぶと Deadline
     * Supervision 違反でラッチされ、以降 WdgM_Init() まで回復不能になる。 */
    FakeMillis_Value += 500UL;
    WdgM_CheckpointReached(WDGM_ENTITY_WARNING, WDGM_CP_WARNING_START);
    FakeMillis_Value += 50UL;
    WdgM_CheckpointReached(WDGM_ENTITY_WARNING, WDGM_CP_WARNING_END);
    FakeMillis_Value += 500UL;
    WdgM_CheckpointReached(WDGM_ENTITY_WARNING, WDGM_CP_WARNING_START);
    FakeMillis_Value += 50UL;
    WdgM_CheckpointReached(WDGM_ENTITY_WARNING, WDGM_CP_WARNING_END);
    FakeMillis_Value += 500UL;
    WdgM_CheckpointReached(WDGM_ENTITY_WARNING, WDGM_CP_WARNING_START);
    FakeMillis_Value += 50UL;
    WdgM_CheckpointReached(WDGM_ENTITY_WARNING, WDGM_CP_WARNING_END);
    WdgM_MainFunction();

    WdgM_GlobalStatusType globalStatus;
    ASSERT_EQ(WdgM_GetGlobalStatus(&globalStatus), E_OK);
    ASSERT_EQ(globalStatus, WDGM_GLOBAL_STATUS_OK);

    /* リセットへ至らずに回復したため、古い SEID 記録は無効化されているはず
     * （後で無関係な原因でリセットされた際に誤って ENGINE を疑わないため。
     * /code-review で指摘）。 */
    WdgM_SupervisedEntityIdType seidAfter = 0xFFU;
    Std_ReturnType ret = WdgM_GetFirstExpiredSEID(&seidAfter);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(seidAfter, 0U);
}
