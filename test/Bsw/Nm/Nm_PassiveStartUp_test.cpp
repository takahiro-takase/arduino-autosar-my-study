/**
 * \file    Nm_PassiveStartUp_test.cpp
 * \brief   Nm_PassiveStartUp() の単体テスト
 * \details [SWS_Nm_00031]。CanNm_PassiveStartUp() へ委譲する。OK の確認のため、CanNm を実体のまま初期化する
 *          （ComM は未初期化にして、Repeat Message State への進入の通知がカスケードしないようにする）。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "ComM.h"
#include "CanNm.h"
#include "Nm.h"
#include "Fake_Millis.h"
#include "Fake_Det_Hw.h"
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

/* Nm_PassiveStartUp() の単体テスト用フィクスチャ。
 * SetUp(): ComM_DeInit() の後、CanNm_Init(NULL) と Nm_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。範囲外のチャネルを表す kInvalidChannel も用意する。
 * TearDown(): CanNm と Nm を未初期化の状態へ戻す。 */
class Nm_PassiveStartUp_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        ComM_DeInit();
        CanNm_Init(NULL);
        Nm_Init(NULL);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        CanNm_DeInit();
        Nm_Test_ResetInitState();
    }

    static const NetworkHandleType kInvalidChannel = NM_MAIN_NETWORK_HANDLE + 1U;
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Nm_PassiveStartUp_Test, Nm_PassiveStartUp_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Nm_Test_ResetInitState();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Nm_PassiveStartUp(NM_MAIN_NETWORK_HANDLE);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, NM_E_UNINIT);
}

TEST_F(Nm_PassiveStartUp_Test, Nm_PassiveStartUp_NG_InvalidChannel)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Nm_PassiveStartUp(kInvalidChannel);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, NM_E_INVALID_CHANNEL);
}

TEST_F(Nm_PassiveStartUp_Test, Nm_PassiveStartUp_NG_CanNmFailureIsPropagated)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(CanNm_NetworkRequest(CANNM_MAIN_NETWORK_HANDLE), E_OK);   // CanNm は既に Network Mode

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Nm_PassiveStartUp(NM_MAIN_NETWORK_HANDLE);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);   // CanNm_PassiveStartUp() が [SWS_CanNm_00147] で拒否する
}

TEST_F(Nm_PassiveStartUp_Test, Nm_PassiveStartUp_OK_DelegatesToCanNm)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanNm_StateType state = CANNM_STATE_NORMAL_OPERATION;
    CanNm_ModeType  mode;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Nm_PassiveStartUp(NM_MAIN_NETWORK_HANDLE);
    (void)CanNm_GetState(CANNM_MAIN_NETWORK_HANDLE, &state, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(state, CANNM_STATE_REPEAT_MESSAGE);
}

}  // namespace
