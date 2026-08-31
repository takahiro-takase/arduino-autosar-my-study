/**
 * \file    Bsw_CanIf_ControllerMode_test.cpp
 * \brief   CanIf_SetControllerMode/CanIf_GetControllerMode の単体テスト
 *          （GoogleTest / PlatformIO `[env:native_chain]`）。
 *
 * \details 2026-08-30、IF シグネチャは仕様準拠という方針のもと、CanSM/ComM/Nm が
 *          CanIf 層を素通りして Can_SetControllerMode() を直接呼んでいたレイヤ
 *          違反を是正し、CanIf_SetControllerMode()/CanIf_GetControllerMode()
 *          （[SWS_CANIF_00003]/[SWS_CANIF_00229]）を新設した際に追加。
 *          /code-review で「新設した状態遷移判定ロジック（CanIf_ControllerMode[]
 *          による CAN_T_STOP/CAN_T_WAKEUP の使い分け）と NG 系に対するテストが
 *          無い」と指摘され追加した。
 *
 *          CanSM は経由せず、CanIf_SetControllerMode()/GetControllerMode() を
 *          直接叩いて Can.c の実体（Can_Test_GetControllerState()）で検証する。
 */
#include <gtest/gtest.h>

extern "C" {
#include "CanIf.h"
#include "Can.h"
#include "Can_Hw.h"
#include "Hal_Can_Hw_fake.h"
#include "Hal_Det_Hw_fake.h"
}

namespace
{

const CanIf_ConfigType kTestCanIfConfig = {
    /* TxPduConfig */ NULL,
    /* TxPduCount */  0U,
    /* RxPduConfig */ NULL,
    /* RxPduCount */  0U
};

class Bsw_CanIf_ControllerMode_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeCanHw_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制

        canConfig.filter.filterId = 0x0220U;
        canConfig.filter.mask     = 0x1FFFU;
        canConfig.csPin           = 10U;
        canConfig.intPin          = 2U;
        canConfig.baudrate        = 500000U;
        canConfig.crystalFreq     = CAN_CRYSTAL_16MHZ;

        Can_Init(&canConfig);
        CanIf_Init(&kTestCanIfConfig);
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

TEST_F(Bsw_CanIf_ControllerMode_Test, SetControllerMode_NG_InvalidControllerIdReturnsErrorAndReportsDet)
{
    Std_ReturnType ret = CanIf_SetControllerMode(CANIF_CONTROLLER_MAX, CAN_CS_STARTED);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_CONTROLLERID);
}

TEST_F(Bsw_CanIf_ControllerMode_Test, SetControllerMode_NG_InvalidControllerModeReturnsErrorAndReportsDet)
{
    Std_ReturnType ret = CanIf_SetControllerMode(0U, CAN_CS_UNINIT);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_CTRLMODE);
    /* 拒否された要求は Can 側にも CanIf の追跡状態にも影響しないこと。 */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STOPPED);
    Can_ControllerStateType mode;
    ASSERT_EQ(CanIf_GetControllerMode(0U, &mode), E_OK);
    EXPECT_EQ(mode, CAN_CS_STOPPED);
}

TEST_F(Bsw_CanIf_ControllerMode_Test, GetControllerMode_NG_InvalidControllerIdReturnsErrorAndReportsDet)
{
    Can_ControllerStateType mode;
    Std_ReturnType ret = CanIf_GetControllerMode(CANIF_CONTROLLER_MAX, &mode);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_CONTROLLERID);
}

TEST_F(Bsw_CanIf_ControllerMode_Test, GetControllerMode_NG_NullPointerReturnsErrorAndReportsDet)
{
    Std_ReturnType ret = CanIf_GetControllerMode(0U, NULL);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_POINTER);
}

TEST_F(Bsw_CanIf_ControllerMode_Test, GetControllerMode_OK_ReflectsStoppedRightAfterInit)
{
    Can_ControllerStateType mode;

    ASSERT_EQ(CanIf_GetControllerMode(0U, &mode), E_OK);
    EXPECT_EQ(mode, CAN_CS_STOPPED);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(Bsw_CanIf_ControllerMode_Test, SetControllerMode_OK_StartedFromStoppedUsesCanTStart)
{
    Std_ReturnType ret = CanIf_SetControllerMode(0U, CAN_CS_STARTED);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);
    Can_ControllerStateType mode;
    ASSERT_EQ(CanIf_GetControllerMode(0U, &mode), E_OK);
    EXPECT_EQ(mode, CAN_CS_STARTED);
}

/* CAN_CS_STOPPED の要求は、CanIf が追跡する遷移元状態によって Can_T_STOP
 * （STARTED から）と CAN_T_WAKEUP（SLEEP から）を使い分ける、本モジュールの
 * 中核ロジック。両方の遷移元を独立して検証する。 */

TEST_F(Bsw_CanIf_ControllerMode_Test, SetControllerMode_OK_StoppedFromStartedUsesCanTStop)
{
    ASSERT_EQ(CanIf_SetControllerMode(0U, CAN_CS_STARTED), E_OK);
    ASSERT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);

    Std_ReturnType ret = CanIf_SetControllerMode(0U, CAN_CS_STOPPED);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STOPPED);
    Can_ControllerStateType mode;
    ASSERT_EQ(CanIf_GetControllerMode(0U, &mode), E_OK);
    EXPECT_EQ(mode, CAN_CS_STOPPED);
}

TEST_F(Bsw_CanIf_ControllerMode_Test, SetControllerMode_OK_StoppedFromSleepUsesCanTWakeup)
{
    ASSERT_EQ(CanIf_SetControllerMode(0U, CAN_CS_SLEEP), E_OK);
    ASSERT_EQ(Can_Test_GetControllerState(), CAN_CS_SLEEP);

    Std_ReturnType ret = CanIf_SetControllerMode(0U, CAN_CS_STOPPED);

    EXPECT_EQ(ret, E_OK);
    /* CAN_T_STOP は CAN_CS_SLEEP からの遷移を拒否する（Can.c 参照）ため、
     * ここで実際に CAN_CS_STOPPED へ遷移していれば CAN_T_WAKEUP が
     * 選ばれたことの間接的な証明になる。 */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STOPPED);
    Can_ControllerStateType mode;
    ASSERT_EQ(CanIf_GetControllerMode(0U, &mode), E_OK);
    EXPECT_EQ(mode, CAN_CS_STOPPED);
}

TEST_F(Bsw_CanIf_ControllerMode_Test, SetControllerMode_OK_SleepFromStartedUsesCanTSleep)
{
    ASSERT_EQ(CanIf_SetControllerMode(0U, CAN_CS_STARTED), E_OK);

    Std_ReturnType ret = CanIf_SetControllerMode(0U, CAN_CS_SLEEP);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_SLEEP);
}

// ------------------------------------------------------------------------
// CanIf_GetControllerErrorState() の単体テスト（[SWS_CANIF_91001]、
// 2026-08-31 追加。CanIf → Can.c → Can_Hw.c（フェイク）の実チェーンで検証）。
// ------------------------------------------------------------------------

TEST_F(Bsw_CanIf_ControllerMode_Test, GetControllerErrorState_NG_InvalidControllerIdReturnsErrorAndReportsDet)
{
    Can_ErrorStateType state;
    Std_ReturnType ret = CanIf_GetControllerErrorState(CANIF_CONTROLLER_MAX, &state);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_CONTROLLERID);
}

TEST_F(Bsw_CanIf_ControllerMode_Test, GetControllerErrorState_NG_NullPointerReturnsErrorAndReportsDet)
{
    Std_ReturnType ret = CanIf_GetControllerErrorState(0U, NULL);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_POINTER);
}

TEST_F(Bsw_CanIf_ControllerMode_Test, GetControllerErrorState_OK_ReflectsBusOffFromHwChain)
{
    FakeCanHw_ErrorState = 2U;  /* CAN_ERRORSTATE_BUSOFF */

    Can_ErrorStateType state;
    Std_ReturnType ret = CanIf_GetControllerErrorState(0U, &state);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(state, CAN_ERRORSTATE_BUSOFF);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

}  // namespace
