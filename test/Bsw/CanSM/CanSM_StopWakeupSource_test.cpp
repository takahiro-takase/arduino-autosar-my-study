/**
 * \file    CanSM_StopWakeupSource_test.cpp
 * \brief   CanSM_StopWakeupSource() の単体テスト
 * \details [SWS_CanSM_00610]/[SWS_CanSM_00622]/[SWS_CanSM_00608]。WAKEUP_VALIDATING のときだけ、検証を止める（コントローラを
 *          再びスリープさせて、NO_COM へ戻る）。それ以外の状態では、何もしない（要求は受理する）。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Can.h"
#include "Can_Hw.h"
#include "CanIf.h"
#include "CanSM.h"
#include "CanSM_Cfg.h"
#include "ComM.h"
#include "Fake_Can_Hw.h"
#include "Fake_Millis.h"
#include "Fake_Det_Hw.h"
#include "Wrap_Dem.h"
#include "Wrap_CanIf.h"
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

/* CanIf を wrap で成功させる（実体の Can を呼ばない）ため、CanSM が待っているコントローラのモード変更の完了通知を、
 * CanSM へ直接流す。STOPPED → STARTED → SLEEP の順に流すと、FULL_COM へ入る手順（STOPPED → STARTED）も、
 * NO_COM へ入る手順（STOPPED → SLEEP）も、完了する（待っていないモードの通知は、無視される）。 */
void SettleCanSmModeRequests()
{
    CanSM_ControllerModeIndication(0U, CAN_CS_STOPPED);
    CanSM_ControllerModeIndication(0U, CAN_CS_STARTED);
    CanSM_ControllerModeIndication(0U, CAN_CS_SLEEP);
}


/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* CanSM_StopWakeupSource() の単体テスト用フィクスチャ。
 * SetUp(): CanSM_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): CanSM_DeInit() で未初期化へ戻す。 */
class CanSM_StopWakeupSource_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        WrapCanIf_Reset();             // 他のテストファイルの故障注入が漏れ伝わらないようリセット
        FakeMillis_Reset();
        CanSM_Init(NULL);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        WrapCanIf_Reset();   // 故障注入を残さない
        CanSM_DeInit();
    }

    static const NetworkHandleType kInvalidNetwork = CANSM_CHANNEL_COUNT;
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(CanSM_StopWakeupSource_Test, CanSM_StopWakeupSource_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanSM_DeInit();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanSM_StopWakeupSource(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANSM_E_UNINIT);
}

TEST_F(CanSM_StopWakeupSource_Test, CanSM_StopWakeupSource_NG_InvalidNetwork)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanSM_StopWakeupSource(kInvalidNetwork);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANSM_E_INVALID_NETWORK_HANDLE);
}

// 検証中なら、検証を止めて、コントローラを再スリープさせる。タイムアウトの再スリープは、重ねて要求しない
TEST_F(CanSM_StopWakeupSource_Test, CanSM_StopWakeupSource_OK_StopsValidationAndSleepsAgain)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanIf_SetControllerMode = 0U;
    ForcedReturn_CanIf_SetControllerMode      = E_OK;
    ASSERT_EQ(CanSM_StartWakeupSource(0U), E_OK);   // 検証中へ
    const uint32 setModeBefore = CallCount_CanIf_SetControllerMode;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanSM_StopWakeupSource(0U);
    const uint32 afterStop = CallCount_CanIf_SetControllerMode;
    FakeMillis_Value += CANSM_WAKEUP_VALIDATION_MS + 1UL;
    CanSM_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(afterStop, setModeBefore + 1U);                          // SLEEP の要求
    EXPECT_EQ(CallCount_CanIf_SetControllerMode, afterStop);           // 検証中ではないので、タイムアウトの要求は出ない
}

// 検証中ではないとき（NO_COM）は、要求だけ受理して、何もしない
TEST_F(CanSM_StopWakeupSource_Test, CanSM_StopWakeupSource_OK_IgnoredWhenNotValidating)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanIf_SetControllerMode = 0U;
    ForcedReturn_CanIf_SetControllerMode      = E_OK;
    const uint32 setModeBefore = CallCount_CanIf_SetControllerMode;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanSM_StopWakeupSource(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(CallCount_CanIf_SetControllerMode, setModeBefore);
}

}  // namespace
