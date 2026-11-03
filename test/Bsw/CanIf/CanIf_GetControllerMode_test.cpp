/**
 * \file    CanIf_GetControllerMode_test.cpp
 * \brief   CanIf_GetControllerMode() の単体テスト
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
class CanIf_GetControllerMode_Test : public ::testing::Test
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

        Can_Test_SetConfigPtr(NULL);  // 未初期化へ戻す（Can_Init() は初期化済みだと CAN_E_TRANSITION で拒否する）
        Can_Init(&canConfig);
        CanIf_Init(&kControllerModeCanIfConfig);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        CanIf_DeInit();
    }

    Can_ConfigType canConfig;
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(CanIf_GetControllerMode_Test, GetControllerMode_NG_InvalidControllerIdReturnsErrorAndReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_ControllerStateType mode;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CanIf_GetControllerMode(CANIF_CONTROLLER_MAX, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_CONTROLLERID);
}

TEST_F(CanIf_GetControllerMode_Test, GetControllerMode_NG_NullPointerReturnsErrorAndReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CanIf_GetControllerMode(0U, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_POINTER);
}

TEST_F(CanIf_GetControllerMode_Test, GetControllerMode_NG_UninitializedReturnsError)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanIf_DeInit();   // 未初期化にする
    Can_ControllerStateType mode = CAN_CS_STARTED;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanIf_GetControllerMode(0U, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);   // 未初期化は DET へ報告する（[SWS_CANIF_00901]）
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_UNINIT);
    EXPECT_EQ(FakeDetHw_LastApiId, CANIF_API_ID_GET_CONTROLLER_MODE);
}

TEST_F(CanIf_GetControllerMode_Test, GetControllerMode_OK_ReflectsStoppedRightAfterInit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_ControllerStateType mode;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CanIf_GetControllerMode(0U, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_EQ(ret, E_OK);
    EXPECT_EQ(mode, CAN_CS_STOPPED);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(CanIf_GetControllerMode_Test, GetControllerMode_OK_ForwardsToCanDriverState)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // CanIf を経由せず、Can ドライバのモードだけを変える（CanIf が持つ記録は、STOPPED のまま）。
    ASSERT_EQ(Can_SetControllerMode(0U, CAN_CS_STARTED), E_OK);
    Can_ControllerStateType mode = CAN_CS_UNINIT;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanIf_GetControllerMode(0U, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(mode, CAN_CS_STARTED);   // [SWS_CANIF_00229]: Can ドライバの状態を返す（CanIf の記録ではない）
}

TEST_F(CanIf_GetControllerMode_Test, GetControllerMode_NG_CanDriverFailureIsReturned)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Can_Test_SetConfigPtr(NULL);   // Can ドライバを未初期化にする（Can_GetControllerMode() が失敗する）
    FakeDetHw_Reset();
    Can_ControllerStateType mode = CAN_CS_STARTED;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanIf_GetControllerMode(0U, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(mode, CAN_CS_STARTED);   // 失敗のときは、格納先を書き換えない
    EXPECT_EQ(FakeDetHw_LastErrorId, CAN_E_UNINIT);   // Can ドライバが、未初期化を報告する

    Can_Init(&canConfig);   // 後続のテストへ、未初期化を残さない
}

TEST_F(CanIf_GetControllerMode_Test, GetControllerMode_OK_ForwardsSleepState)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Can_SetControllerMode(0U, CAN_CS_SLEEP), E_OK);   // CanIf を経由せず、Can ドライバだけを SLEEP にする
    Can_ControllerStateType mode = CAN_CS_UNINIT;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanIf_GetControllerMode(0U, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(mode, CAN_CS_SLEEP);
}

}  // namespace
