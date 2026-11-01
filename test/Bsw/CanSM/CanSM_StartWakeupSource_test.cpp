/**
 * \file    CanSM_StartWakeupSource_test.cpp
 * \brief   CanSM_StartWakeupSource() の単体テスト
 * \details [SWS_CanSM_00609]/[SWS_CanSM_00616]/[SWS_CanSM_00607]。NO_COM のときだけ、起床の検証を始める（コントローラを
 *          Listen-Only へ移し、WAKEUP_VALIDATING へ進む）。それ以外の状態では、何もしない（要求は受理する）。
 *          CanIf は wrap で、実体を呼ばずに成功・失敗を注入する。
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

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* CanSM_StartWakeupSource() の単体テスト用フィクスチャ。
 * SetUp(): CanSM_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): CanSM_DeInit() で未初期化へ戻す。 */
class CanSM_StartWakeupSource_Test : public ::testing::Test
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

TEST_F(CanSM_StartWakeupSource_Test, CanSM_StartWakeupSource_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanSM_DeInit();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanSM_StartWakeupSource(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANSM_E_UNINIT);
}

TEST_F(CanSM_StartWakeupSource_Test, CanSM_StartWakeupSource_NG_InvalidNetwork)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanSM_StartWakeupSource(kInvalidNetwork);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANSM_E_INVALID_NETWORK_HANDLE);
}

// NO_COM のとき、Listen-Only（STOPPED）への移行を要求して、検証中へ進む。検証がタイムアウトすれば、再スリープする
TEST_F(CanSM_StartWakeupSource_Test, CanSM_StartWakeupSource_OK_StartsValidationInNoCom)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanIf_SetControllerMode = 0U;   // コントローラの状態変更は、常に成功する
    ForcedReturn_CanIf_SetControllerMode      = E_OK;
    const uint32 setModeBefore = CallCount_CanIf_SetControllerMode;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanSM_StartWakeupSource(0U);
    const uint32 afterStart = CallCount_CanIf_SetControllerMode;
    FakeMillis_Value += CANSM_WAKEUP_VALIDATION_MS + 1UL;
    CanSM_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(afterStart, setModeBefore + 1U);                               // STOPPED（Listen-Only）の要求
    EXPECT_EQ(CallCount_CanIf_SetControllerMode, afterStart + 1U);           // 検証がタイムアウトして、再スリープの要求
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

// NO_COM ではないとき（FULL_COM）は、要求だけ受理して、何もしない
TEST_F(CanSM_StartWakeupSource_Test, CanSM_StartWakeupSource_OK_IgnoredOutsideNoCom)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanIf_SetControllerMode = 0U;
    ForcedReturn_CanIf_SetControllerMode      = E_OK;
    FailFromCallCount_CanIf_SetPduMode = 0U;
    ForcedReturn_CanIf_SetPduMode      = E_OK;
    ASSERT_EQ(CanSM_RequestComMode(0U, COMM_FULL_COMMUNICATION), E_OK);
    const uint32 setModeBefore = CallCount_CanIf_SetControllerMode;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanSM_StartWakeupSource(0U);
    ComM_ModeType mode = COMM_NO_COMMUNICATION;
    (void)CanSM_GetCurrentComMode(0U, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(mode, COMM_FULL_COMMUNICATION);                          // 状態は変わらない
    EXPECT_EQ(CallCount_CanIf_SetControllerMode, setModeBefore);       // コントローラへも何も要求しない
}

// コントローラが Listen-Only へ移れなければ、NO_COM のまま据え置く（検証のタイムアウトも、働かない）
TEST_F(CanSM_StartWakeupSource_Test, CanSM_StartWakeupSource_NG_StaysInNoComWhenControllerCannotStop)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanIf_SetControllerMode = 0U;   // コントローラの状態変更は、常に失敗する
    ForcedReturn_CanIf_SetControllerMode      = E_NOT_OK;
    const uint32 setModeBefore = CallCount_CanIf_SetControllerMode;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanSM_StartWakeupSource(0U);
    const uint32 afterStart = CallCount_CanIf_SetControllerMode;
    FakeMillis_Value += CANSM_WAKEUP_VALIDATION_MS + 1UL;
    CanSM_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);                                              // 要求自体は受理する（[SWS_CanSM_00616]）
    EXPECT_EQ(afterStart, setModeBefore + 1U);                         // 停止を要求したが失敗した
    EXPECT_EQ(CallCount_CanIf_SetControllerMode, afterStart);          // 検証中ではないので、タイムアウトの再スリープも要求しない
}

}  // namespace
