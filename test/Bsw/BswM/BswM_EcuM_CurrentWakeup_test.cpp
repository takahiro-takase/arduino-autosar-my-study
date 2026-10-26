/**
 * \file    BswM_EcuM_CurrentWakeup_test.cpp
 * \brief   BswM_EcuM_CurrentWakeup() の単体テスト
 *
 * \details EcuM からの起床要因の状態の通知（[SWS_BswM_00131]、BswMEcuMWakeupSource）を受け取り、
 *          `BSWM_MODE_SRC_ECUM_WAKEUP` のモードとして保持し、その状態を条件に含むルールを評価することを確認する。
 *          本番のルール表には、この状態を使うルールが無いため、テスト用に、ルール 1 本（起床要因が EXPIRED になったら、
 *          タスク 0 を有効化する）の設定を作って使う。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "BswM.h"
#include "BswM_PBCfg.h"
#include "EcuM_Cfg.h"
#include "Wrap_BswM.h"
#include "Wrap_Os.h"
#include "Fake_Det_Hw.h"
}

/* ======================================================================
 * Global Variables
 * ====================================================================== */

namespace
{

/* 起床要因が EXPIRED になったら、タスク 0 を有効化するルール 1 本。 */
const BswM_RuleType kWakeupExpiredRule = {
    /* Operator */       BSWM_OP_AND,
    /* Condition */      { { BSWM_MODE_SRC_ECUM_WAKEUP, ECUM_WKSTATUS_EXPIRED } },
    /* ConditionCount */ 1U,
    /* Action */         BSWM_ACTION_ACTIVATE,
    /* TaskMask */       0x00000001UL,
    /* IpduGroupId */    0U,
    /* Initialize */     FALSE
};

const BswM_ConfigType kWakeupConfig = { &kWakeupExpiredRule, 1U };

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* BswM_EcuM_CurrentWakeup() の単体テスト用フィクスチャ。
 * SetUp(): wrap と DET の記録を初期化する。BswM_Init() は各テストが呼ぶ（未初期化のテストがあるため）。
 * TearDown(): BswM_Deinit() で未初期化へ戻す。 */
class BswM_EcuM_CurrentWakeup_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        WrapOs_Reset();
        WrapBswM_Reset();
        FakeDetHw_LogSuppressed = 1U;
        FakeDetHw_Reset();
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        BswM_Deinit();
    }

    /** テスト用の設定で BswM を初期化し、Init 自体の記録を消す。 */
    void InitBswM()
    {
        BswM_Init(&kWakeupConfig);
        WrapOs_Reset();
        FakeDetHw_Reset();
        FakeDetHw_LogSuppressed = 0U;   // ここから各 TEST_F の実行(Act)区間
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

// NG: BswM_Init() の前に呼ぶと、BSWM_E_NO_INIT を報告して無視する（[SWS_BswM_00132]）。
TEST_F(BswM_EcuM_CurrentWakeup_Test, BswM_EcuM_CurrentWakeup_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FakeDetHw_LogSuppressed = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BswM_EcuM_CurrentWakeup(ECUM_WKSOURCE_CAN, ECUM_WKSTATUS_EXPIRED);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, BSWM_E_NO_INIT);
    EXPECT_EQ(FakeDetHw_LastApiId, BSWM_API_ID_ECUM_CURRENT_WAKEUP);
    EXPECT_EQ(CallCount_Os_SetTaskActive, 0U);
}

// NG: 範囲外の状態を渡すと、BSWM_E_REQ_MODE_OUT_OF_RANGE を報告して無視する（[SWS_BswM_00133]）。
TEST_F(BswM_EcuM_CurrentWakeup_Test, BswM_EcuM_CurrentWakeup_NG_StateOutOfRange)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    InitBswM();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BswM_EcuM_CurrentWakeup(ECUM_WKSOURCE_CAN, (EcuM_WakeupStatusType)(ECUM_WKSTATUS_EXPIRED + 1U));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, BSWM_E_REQ_MODE_OUT_OF_RANGE);
    EXPECT_EQ(FakeDetHw_LastApiId, BSWM_API_ID_ECUM_CURRENT_WAKEUP);
    EXPECT_EQ(CallCount_Os_SetTaskActive, 0U);
}

// OK: 起床要因（CAN）の状態が、ルールの条件の状態（EXPIRED）になると、そのルールのアクションを実行する。
TEST_F(BswM_EcuM_CurrentWakeup_Test, BswM_EcuM_CurrentWakeup_OK_RuleFiresWhenStateMatchesCondition)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    InitBswM();
    BswM_EcuM_CurrentWakeup(ECUM_WKSOURCE_CAN, ECUM_WKSTATUS_PENDING);   // 条件の状態ではない
    ASSERT_EQ(CallCount_Os_SetTaskActive, 0U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BswM_EcuM_CurrentWakeup(ECUM_WKSOURCE_CAN, ECUM_WKSTATUS_EXPIRED);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Os_SetTaskActive, 1U);
}

// OK: 同じ状態の通知を続けても、アクションは繰り返さない。いったん別の状態へ戻った後に、再び条件の状態になれば、再び実行する。
TEST_F(BswM_EcuM_CurrentWakeup_Test, BswM_EcuM_CurrentWakeup_OK_RuleFiresOnlyOnTransition)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    InitBswM();
    BswM_EcuM_CurrentWakeup(ECUM_WKSOURCE_CAN, ECUM_WKSTATUS_EXPIRED);
    ASSERT_EQ(CallCount_Os_SetTaskActive, 1U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BswM_EcuM_CurrentWakeup(ECUM_WKSOURCE_CAN, ECUM_WKSTATUS_EXPIRED);   // 同じ状態の通知
    const uint32 afterRepeat = CallCount_Os_SetTaskActive;
    BswM_EcuM_CurrentWakeup(ECUM_WKSOURCE_CAN, ECUM_WKSTATUS_NONE);      // 条件を外れる
    BswM_EcuM_CurrentWakeup(ECUM_WKSOURCE_CAN, ECUM_WKSTATUS_EXPIRED);   // 再び条件の状態になる

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(afterRepeat, 1U);
    EXPECT_EQ(CallCount_Os_SetTaskActive, 2U);
}

// OK: CAN の起床要因のビットを含まない要因（本プロジェクトが持たない要因）の通知は、無視する。
TEST_F(BswM_EcuM_CurrentWakeup_Test, BswM_EcuM_CurrentWakeup_OK_IgnoresSourceWithoutCanBit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    InitBswM();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BswM_EcuM_CurrentWakeup(0x00000001UL, ECUM_WKSTATUS_EXPIRED);   // ECUM_WKSOURCE_POWER など、CAN 以外

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Os_SetTaskActive, 0U);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

}  // namespace
