/**
 * \file    Nm_GetState_test.cpp
 * \brief   Nm_GetState() の単体テスト
 * \details CanNm の状態とモードを、Nm の型（Nm_StateType / Nm_ModeType）へ変換して返す。CanNm_Init() 直後は Bus-Sleep Mode で、
 *          状態は NM_STATE_BUS_SLEEP、モードは NM_MODE_BUS_SLEEP になる。ポインタのどちらかが NULL なら、その値は書かない。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Nm.h"
#include "Nm_Cfg.h"
#include "CanNm.h"
#include "CanNm_Cfg.h"
#include "Fake_Det_Hw.h"
}

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

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

/* Nm_GetState() の単体テスト用フィクスチャ。
 * SetUp(): DET の記録を初期化し、CanNm_Init(NULL)・Nm_Init(NULL) を呼ぶ（Init 自体の記録は消す）。
 * TearDown(): Nm・CanNm を未初期化へ戻す。 */
class Nm_GetState_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;
        FakeDetHw_Reset();
        CanNm_Init(NULL);
        Nm_Init(NULL);
        FakeDetHw_Reset();
        FakeDetHw_LogSuppressed = 0U;
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Nm_Test_ResetInitState();
        CanNm_DeInit();
    }
};

/* Nm の単体テスト用フィクスチャ。
 * SetUp(): Nm_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。範囲外のチャネルを表す kInvalidChannel も用意する。
 * TearDown(): 未初期化の状態へ戻す。 */
class Nm_GetState_Base_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        Nm_Init(NULL);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Nm_Test_ResetInitState();
    }

    static const NetworkHandleType kInvalidChannel = NM_MAIN_NETWORK_HANDLE + 1U;
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Nm_GetState_Base_Test, Nm_GetState_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Nm_Test_ResetInitState();
    Nm_StateType state;
    Nm_ModeType  mode;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Nm_GetState(NM_MAIN_NETWORK_HANDLE, &state, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, NM_E_UNINIT);
}

TEST_F(Nm_GetState_Base_Test, Nm_GetState_NG_InvalidChannel)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Nm_StateType state;
    Nm_ModeType  mode;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Nm_GetState(kInvalidChannel, &state, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, NM_E_INVALID_CHANNEL);
}

TEST_F(Nm_GetState_Test, Nm_GetState_OK_MapsBusSleepStateAndMode)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Nm_StateType state = NM_STATE_NORMAL_OPERATION;
    Nm_ModeType mode = NM_MODE_NETWORK;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Nm_GetState(NM_MAIN_NETWORK_HANDLE, &state, &mode);
    const Std_ReturnType retNoState = Nm_GetState(NM_MAIN_NETWORK_HANDLE, NULL, &mode);
    const Std_ReturnType retNoMode = Nm_GetState(NM_MAIN_NETWORK_HANDLE, &state, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(state, NM_STATE_BUS_SLEEP);
    EXPECT_EQ(mode, NM_MODE_BUS_SLEEP);
    EXPECT_EQ(retNoState, E_OK);   // NULL の側は書かない
    EXPECT_EQ(retNoMode, E_OK);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

}  // namespace
