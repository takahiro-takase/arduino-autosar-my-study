/**
 * \file    Bsw_BswM_test.cpp
 * \brief   BswM.c（src/Bsw/BswM/BswM.c）の単体テスト（NG系）
 * \details BswM.c 全公開APIの `Det_ReportError()` 呼び出し箇所（NG ケースのみ）を
 *          1ファイルにまとめる。各ケースで、報告される ErrorId が仕様どおり
 *          正しい値になっていることを `Fake_Det_Hw.h`（`FakeDetHw_LastErrorId`）
 *          で検証する。
 *
 *          BswM には `BswM_Deinit()` が存在するため、Mcu/PduR/CanTp と異なり
 *          「未初期化状態」の検証にテスト専用のリセット関数は不要（各テストが
 *          明示的に `BswM_Deinit()` を呼んでから検証する）。
 *
 *          GoogleTest の main() は test_main.cpp に集約しているため、
 *          本ファイルでは定義しない。
 */
#include <gtest/gtest.h>

extern "C" {
#include "BswM.h"
#include "BswM_PBCfg.h"
#include "Wrap_BswM.h"
#include "Fake_Det_Hw.h"
}

namespace
{

class Bsw_BswM_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        WrapBswM_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        BswM_Init(&BswM_Config);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        BswM_Deinit();
    }
};

// ------------------------------------------------------------
// BswM_Init()
// ------------------------------------------------------------

TEST_F(Bsw_BswM_Test, BswM_Init_NG_NullConfigPtr)
{
    BswM_Deinit();

    BswM_Init(NULL);

    EXPECT_EQ(FakeDetHw_LastErrorId, BSWM_E_PARAM_CONFIG);
}

// ------------------------------------------------------------
// BswM_Deinit()
// ------------------------------------------------------------

TEST_F(Bsw_BswM_Test, BswM_Deinit_NG_NoInit)
{
    BswM_Deinit();  // 1回目: SetUp() の Init を正常に解除する
    FakeDetHw_Reset();

    BswM_Deinit();  // 2回目: 既に未初期化のため NG

    EXPECT_EQ(FakeDetHw_LastErrorId, BSWM_E_NO_INIT);
}

// ------------------------------------------------------------
// BswM_GetVersionInfo()
// ------------------------------------------------------------

TEST_F(Bsw_BswM_Test, BswM_GetVersionInfo_NG_NullPointer)
{
    BswM_GetVersionInfo(NULL);

    EXPECT_EQ(FakeDetHw_LastErrorId, BSWM_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// BswM_EcuM_CurrentState()
// ------------------------------------------------------------

TEST_F(Bsw_BswM_Test, BswM_EcuM_CurrentState_NG_NoInit)
{
    BswM_Deinit();

    BswM_EcuM_CurrentState(ECUM_STATE_RUN);

    EXPECT_EQ(FakeDetHw_LastErrorId, BSWM_E_NO_INIT);
}

TEST_F(Bsw_BswM_Test, BswM_EcuM_CurrentState_NG_ReqModeOutOfRange)
{
    BswM_EcuM_CurrentState(static_cast<EcuM_StateType>(0xFFU));

    EXPECT_EQ(FakeDetHw_LastErrorId, BSWM_E_REQ_MODE_OUT_OF_RANGE);
}

// ------------------------------------------------------------
// BswM_ComM_CurrentMode()
// ------------------------------------------------------------

TEST_F(Bsw_BswM_Test, BswM_ComM_CurrentMode_NG_NoInit)
{
    BswM_Deinit();

    BswM_ComM_CurrentMode(0U, COMM_FULL_COMMUNICATION);

    EXPECT_EQ(FakeDetHw_LastErrorId, BSWM_E_NO_INIT);
}

TEST_F(Bsw_BswM_Test, BswM_ComM_CurrentMode_NG_ReqModeOutOfRange)
{
    BswM_ComM_CurrentMode(0U, static_cast<ComM_ModeType>(COMM_FULL_COMMUNICATION + 1U));

    EXPECT_EQ(FakeDetHw_LastErrorId, BSWM_E_REQ_MODE_OUT_OF_RANGE);
}

// ------------------------------------------------------------
// BswM_Dcm_CommunicationMode_CurrentState()
// ------------------------------------------------------------

TEST_F(Bsw_BswM_Test, BswM_Dcm_CommunicationMode_CurrentState_NG_NoInit)
{
    BswM_Deinit();

    BswM_Dcm_CommunicationMode_CurrentState(0U, DCM_DISABLE_RX_TX_NORM_NM);

    EXPECT_EQ(FakeDetHw_LastErrorId, BSWM_E_NO_INIT);
}

TEST_F(Bsw_BswM_Test, BswM_Dcm_CommunicationMode_CurrentState_NG_ReqModeOutOfRange)
{
    BswM_Dcm_CommunicationMode_CurrentState(
        0U, static_cast<Dcm_CommunicationModeType>(DCM_DISABLE_RX_TX_NORM_NM + 1U));

    EXPECT_EQ(FakeDetHw_LastErrorId, BSWM_E_REQ_MODE_OUT_OF_RANGE);
}

}  // namespace
