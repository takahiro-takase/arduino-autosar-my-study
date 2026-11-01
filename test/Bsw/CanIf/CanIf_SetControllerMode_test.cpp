/**
 * \file    CanIf_SetControllerMode_test.cpp
 * \brief   CanIf_SetControllerMode() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 CanIf_test.cpp）。
 *
 *          分割前の共通の説明は、旧 CanIf_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "CanIf.h"
#include "CanSM.h"
#include "Can.h"
#include "Can_Hw.h"
#include "Fake_Can_Hw.h"
#include "Fake_Det_Hw.h"
#include "Wrap_CanIf.h"
#include "Wrap_Can.h"
#include "Wrap_ComM.h"
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

const CanIf_ConfigType kControllerModeCanIfConfig = {
    /* TxPduConfig */ NULL,
    /* TxPduCount */  0U,
    /* RxPduConfig */ NULL,
    /* RxPduCount */  0U
};

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* CanIf のコントローラモード系 API（SetControllerMode / GetControllerMode / GetControllerErrorState）の単体テスト用フィクスチャ。
 * SetUp(): 模擬 Can_Hw と wrap をリセットし、Can_Init() と、このグループ専用の CanIf 設定（kControllerModeCanIfConfig）で CanIf_Init() を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): CanIf_DeInit() で未初期化へ戻す。 */
class CanIf_SetControllerMode_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeCanHw_Reset();
        WrapCan_Reset();  // 同上（Can.c 側）
        WrapCanIf_Reset();  // 他ファイルの故障注入が漏れ伝わらないよう防御的にリセット
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制

        canConfig.filter.filterId = 0x0220U;
        canConfig.filter.mask     = 0x1FFFU;
        canConfig.csPin           = 10U;
        canConfig.intPin          = 2U;
        canConfig.baudrate        = 500000U;
        canConfig.crystalFreq     = CAN_CRYSTAL_16MHZ;

        Can_Init(&canConfig);
        CanIf_Init(&kControllerModeCanIfConfig);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        WrapCan_Reset();   // 故障注入を残さない
        CanIf_DeInit();
    }

    Can_ConfigType canConfig;
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(CanIf_SetControllerMode_Test, SetControllerMode_NG_InvalidControllerIdReturnsErrorAndReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CanIf_SetControllerMode(CANIF_CONTROLLER_MAX, CAN_CS_STARTED);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_CONTROLLERID);
}

TEST_F(CanIf_SetControllerMode_Test, SetControllerMode_NG_InvalidControllerModeReturnsErrorAndReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CanIf_SetControllerMode(0U, CAN_CS_UNINIT);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_CTRLMODE);
    /* 拒否された要求は Can 側にも CanIf の追跡状態にも影響しないこと。 */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STOPPED);
    Can_ControllerStateType mode;
    ASSERT_EQ(CanIf_GetControllerMode(0U, &mode), E_OK);
    EXPECT_EQ(mode, CAN_CS_STOPPED);
}

TEST_F(CanIf_SetControllerMode_Test, SetControllerMode_NG_UninitializedReturnsError)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanIf_DeInit();   // 未初期化にする

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanIf_SetControllerMode(0U, CAN_CS_STARTED);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);   // 未初期化は DET へ報告する（[SWS_CANIF_00901]）
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_UNINIT);
    EXPECT_EQ(FakeDetHw_LastApiId, CANIF_API_ID_SET_CONTROLLER_MODE);
}

TEST_F(CanIf_SetControllerMode_Test, SetControllerMode_NG_CanDriverRejectsTransition)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_Can_SetControllerMode = 0U;   // Can ドライバが、状態遷移を受け付けない
    ForcedReturn_Can_SetControllerMode      = CAN_NOT_OK;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanIf_SetControllerMode(0U, CAN_CS_STARTED);
    Can_ControllerStateType mode = CAN_CS_STARTED;
    const Std_ReturnType retMode = CanIf_GetControllerMode(0U, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    ASSERT_EQ(retMode, E_OK);
    EXPECT_EQ(mode, CAN_CS_STOPPED);   // 追跡している状態は変えない
}

TEST_F(CanIf_SetControllerMode_Test, SetControllerMode_OK_StartedFromStoppedUsesCanTStart)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CanIf_SetControllerMode(0U, CAN_CS_STARTED);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);
    Can_ControllerStateType mode;
    ASSERT_EQ(CanIf_GetControllerMode(0U, &mode), E_OK);
    EXPECT_EQ(mode, CAN_CS_STARTED);
}

/* CAN_CS_STOPPED の要求は、CanIf が追跡する遷移元状態によって Can_T_STOP
 * （STARTED から）と CAN_T_WAKEUP（SLEEP から）を使い分ける、本モジュールの
 * 中核ロジック。両方の遷移元を独立して検証する。 */

TEST_F(CanIf_SetControllerMode_Test, SetControllerMode_OK_StoppedFromStartedUsesCanTStop)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(CanIf_SetControllerMode(0U, CAN_CS_STARTED), E_OK);
    ASSERT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CanIf_SetControllerMode(0U, CAN_CS_STOPPED);

    Can_ControllerStateType canState = Can_Test_GetControllerState();
    Can_ControllerStateType mode;
    Std_ReturnType retGet = CanIf_GetControllerMode(0U, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(canState, CAN_CS_STOPPED);
    ASSERT_EQ(retGet, E_OK);
    EXPECT_EQ(mode, CAN_CS_STOPPED);
}

TEST_F(CanIf_SetControllerMode_Test, SetControllerMode_OK_StoppedFromSleepUsesCanTWakeup)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(CanIf_SetControllerMode(0U, CAN_CS_SLEEP), E_OK);
    ASSERT_EQ(Can_Test_GetControllerState(), CAN_CS_SLEEP);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CanIf_SetControllerMode(0U, CAN_CS_STOPPED);

    Can_ControllerStateType canState = Can_Test_GetControllerState();
    Can_ControllerStateType mode;
    Std_ReturnType retGet = CanIf_GetControllerMode(0U, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    /* CAN_T_STOP は CAN_CS_SLEEP からの遷移を拒否する（Can.c 参照）ため、
     * ここで実際に CAN_CS_STOPPED へ遷移していれば CAN_T_WAKEUP が
     * 選ばれたことの間接的な証明になる。 */
    EXPECT_EQ(canState, CAN_CS_STOPPED);
    ASSERT_EQ(retGet, E_OK);
    EXPECT_EQ(mode, CAN_CS_STOPPED);
}

}  // namespace
