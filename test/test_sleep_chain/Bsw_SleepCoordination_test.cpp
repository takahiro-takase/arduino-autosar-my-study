/**
 * \file    Bsw_SleepCoordination_test.cpp
 * \brief   Nm↔CanSM↔ComM 協調スリープ通知（2026-08、CanSM仲介からComM経由への
 *          移管）の単体テスト（GoogleTest / PlatformIO `[env:native_sleep_chain]`）。
 *
 * \details ComM.c/CanSM.c/Nm.c の実体をリンクし、以下を検証する:
 *
 *          1. VoluntarySleep_OK_DefersPhysicalSleepUntilNmReachesBusSleepMode:
 *             ComM_RequestComMode(NO_COM) の直後は物理スリープも
 *             ComM_ChannelMode/EcuM RUN 解放も起きず、Nm が実際に
 *             Bus-Sleep Mode へ到達して初めて起きること（[SWS_ComM_00133]/
 *             [SWS_ComM_00392]/[SWS_ComM_00637] 準拠）。
 *
 *          2. ReRequestFullCom_OK_CancelsPendingNmRelease:
 *             Nm が眠り切る前に FULL_COM を再要求すると、Nm の協調スリープが
 *             キャンセルされ、コントローラが一度も物理スリープしないこと
 *             （[SWS_ComM_00882] 相当）。
 *
 *          3. BusOffDuringNmWinddown_OK_DoesNotResurrectNm（設計時レビューで
 *             発見した回帰の防止）:
 *             Nm の協調スリープ待ち中に Bus-Off が発生すると、Nm 自身は
 *             独立したタイマで動き続けて Bus-Sleep Mode へ到達してしまう
 *             （ComM_Nm_BusSleepMode() 経由の CanSM_RequestComMode(NO_COM) は
 *             Bus-Off 中のため CanSM に一旦拒否される）。Bus-Off が回復して
 *             CanSM が FULL_COM を通知してきても、誰も再要求していない以上
 *             Nm を Nm_NetworkRequest() で誤って再起床させてはならず、
 *             最終的にチャネルは NO_COM へ正しく収束しなければならない。
 *
 *          EcuM（ComM の RUN 要求先）と BswM（ComM のモード通知先）は境界として
 *          フェイクに差し替える（Bsw_EcuM_fake.h/Bsw_BswM_fake.h 冒頭コメント
 *          参照。Bsw_ComM_fake.h と同じ考え方）。CanIf は Nm が
 *          CanIf_Transmit() を直接呼ぶために実体でリンクするが、TxPduCount=0
 *          の空設定を渡す（Bsw_WakeupChain_test.cpp の kTestCanIfConfig と
 *          同じパターン）ため送信は毎回 E_NOT_OK で終わり、Com/PduR は不要。
 */
#include <gtest/gtest.h>

extern "C" {
#include "Can.h"
#include "Can_Hw.h"
#include "CanIf.h"
#include "CanSM.h"
#include "ComM.h"
#include "Nm.h"
#include "Hal_Can_Hw_fake.h"
#include "Hal_Millis_fake.h"
#include "Hal_Det_Hw_fake.h"
#include "Bsw_Dem_fake.h"
#include "Bsw_EcuM_fake.h"
#include "Bsw_BswM_fake.h"
}

namespace
{

const CanIf_ConfigType kTestCanIfConfig = {
    /* TxPduConfig */ NULL,
    /* TxPduCount */  0U,
    /* RxPduConfig */ NULL,
    /* RxPduCount */  0U
};

class Bsw_SleepCoordination_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeCanHw_Reset();
        FakeDem_Reset();
        FakeEcuM_Reset();
        FakeBswM_Reset();
        FakeMillis_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制

        canConfig.filter.filterId = 0x0220U;
        canConfig.filter.mask     = 0x1FFFU;
        canConfig.csPin           = 10U;
        canConfig.intPin          = 2U;
        canConfig.baudrate        = 500000U;
        canConfig.crystalFreq     = CAN_CRYSTAL_16MHZ;

        Can_Init(&canConfig);
        CanIf_Init(&kTestCanIfConfig);
        CanSM_Init(NULL);
        ComM_Init(NULL);
        Nm_Init();

        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;  // DeInit() のログを抑制
        Nm_DeInit();
        ComM_DeInit();
        CanSM_DeInit();
        CanIf_DeInit();
    }

    /** ComM_USER_0 に FULL_COM を要求し、Nm が Repeat Message State を抜けて
     *  安定した Normal Operation State に達するまで進める。呼び出し後、
     *  各フェイクの記録はリセットして各テストの Act 区間だけを対象にする。 */
    void ArrangeFullCom()
    {
        ASSERT_EQ(ComM_RequestComMode(COMM_USER_0, COMM_FULL_COMMUNICATION), E_OK);
        ASSERT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);

        FakeMillis_Value += NM_REPEAT_MESSAGE_MS + 100UL;
        Nm_MainFunction();
        Nm_StateType state;
        Nm_ModeType  mode;
        ASSERT_EQ(Nm_GetState(&state, &mode), E_OK);
        ASSERT_EQ(state, NM_STATE_NORMAL_OPERATION);

        FakeCanHw_Reset();
        FakeDem_Reset();
        FakeEcuM_Reset();
        FakeBswM_Reset();
    }

    /** Nm_MainFunction() を呼びながら millis を NM_CYCLE_MS ずつ進め、Nm が
     *  target 状態へ到達するまで繰り返す。maxTicks 回試行しても到達しなければ
     *  ASSERT で失敗させる（無限ループを避ける安全弁。Repeat Message(1500ms)+
     *  Ready Sleep 中の NM-Timeout(3000ms)+Prepare Bus-Sleep(1500ms)=6000ms
     *  相当を NM_CYCLE_MS=1000ms 刻みで辿るには最低7ティック必要なため、
     *  既定 15 で十分な余裕を持たせている）。 */
    static void DriveNmUntil(Nm_StateType target, int maxTicks = 15)
    {
        Nm_StateType state;
        Nm_ModeType  mode;
        for (int i = 0; i < maxTicks; i++)
        {
            ASSERT_EQ(Nm_GetState(&state, &mode), E_OK);
            if (state == target)
                return;
            FakeMillis_Value += NM_CYCLE_MS;
            Nm_MainFunction();
        }
        ASSERT_EQ(Nm_GetState(&state, &mode), E_OK);
        ASSERT_EQ(state, target);
    }

    Can_ConfigType canConfig;
};

TEST_F(Bsw_SleepCoordination_Test, VoluntarySleep_OK_DefersPhysicalSleepUntilNmReachesBusSleepMode)
{
    ArrangeFullCom();

    /* 実行 (Act) */
    Std_ReturnType ret = ComM_RequestComMode(COMM_USER_0, COMM_NO_COMMUNICATION);
    ASSERT_EQ(ret, E_OK);

    /* 評価 (Assert): 要求直後は何も物理的に変化していない */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);
    EXPECT_EQ(FakeCanHw_SetModeCount, 0U);
    EXPECT_EQ(FakeEcuM_ReleaseRUNCount, 0U);
    ComM_ModeType mode = COMM_NO_COMMUNICATION;
    ASSERT_EQ(ComM_GetCurrentComMode(COMM_USER_0, &mode), E_OK);
    EXPECT_EQ(mode, static_cast<ComM_ModeType>(COMM_FULL_COMMUNICATION));

    /* 実行 (Act): Nm を Bus-Sleep Mode まで進める */
    DriveNmUntil(NM_STATE_BUS_SLEEP);

    /* 評価 (Assert): ここで初めて物理スリープと ComM/EcuM の更新が起きる */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_SLEEP);
    EXPECT_EQ(FakeEcuM_ReleaseRUNCount, 1U);
    ASSERT_EQ(ComM_GetCurrentComMode(COMM_USER_0, &mode), E_OK);
    EXPECT_EQ(mode, static_cast<ComM_ModeType>(COMM_NO_COMMUNICATION));
}

TEST_F(Bsw_SleepCoordination_Test, ReRequestFullCom_OK_CancelsPendingNmRelease)
{
    ArrangeFullCom();

    /* 実行 (Act): NO_COM 要求後、Nm が眠り切る前に FULL_COM を再要求する */
    ASSERT_EQ(ComM_RequestComMode(COMM_USER_0, COMM_NO_COMMUNICATION), E_OK);
    FakeMillis_Value += NM_CYCLE_MS;
    Nm_MainFunction();
    ASSERT_EQ(ComM_RequestComMode(COMM_USER_0, COMM_FULL_COMMUNICATION), E_OK);

    /* 評価 (Assert): 十分な時間 Nm_MainFunction を回しても Bus-Sleep Mode へは
     * 到達しない（再要求でキャンセルされているはず）。コントローラも
     * 一度も物理スリープしない。 */
    Nm_StateType state;
    Nm_ModeType  mode;
    for (int i = 0; i < 15; i++)
    {
        FakeMillis_Value += NM_CYCLE_MS;
        Nm_MainFunction();
        ASSERT_EQ(Nm_GetState(&state, &mode), E_OK);
        ASSERT_NE(state, NM_STATE_BUS_SLEEP);
    }
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);
    EXPECT_EQ(FakeCanHw_SetModeCount, 0U);
}

TEST_F(Bsw_SleepCoordination_Test, BusOffDuringNmWinddown_OK_DoesNotResurrectNm)
{
    ArrangeFullCom();
    ASSERT_EQ(ComM_RequestComMode(COMM_USER_0, COMM_NO_COMMUNICATION), E_OK);

    /* 実行 (Act): Nm の協調スリープ待ち中に Bus-Off が発生する */
    CanSM_ControllerBusOff(0U);

    /* Nm 自身は Bus-Off とは無関係な独立したタイマで動き続け、
     * Bus-Sleep Mode まで到達してしまう（ComM_Nm_BusSleepMode() 経由の
     * CanSM_RequestComMode(NO_COM) は Bus-Off 中のため CanSM に拒否される）。 */
    DriveNmUntil(NM_STATE_BUS_SLEEP);

    /* Bus-Off から回復させる（L1 周期経過） */
    FakeMillis_Value += CANSM_BUSOFF_RECOVERY_L1_MS + 1UL;
    CanSM_MainFunction();

    /* 評価 (Assert): Nm は誤って再起床していない（Bus-Sleep Mode のまま）。
     * チャネルは最終的に NO_COM へ正しく収束し、物理的にも実際にスリープする。 */
    Nm_StateType state;
    Nm_ModeType  nmMode;
    ASSERT_EQ(Nm_GetState(&state, &nmMode), E_OK);
    EXPECT_EQ(state, NM_STATE_BUS_SLEEP);

    ComM_ModeType comMode = COMM_FULL_COMMUNICATION;
    ASSERT_EQ(ComM_GetCurrentComMode(COMM_USER_0, &comMode), E_OK);
    EXPECT_EQ(comMode, static_cast<ComM_ModeType>(COMM_NO_COMMUNICATION));
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_SLEEP);
}

/**
 * \brief   /code-review で指摘された回帰シナリオ:
 *          Nm 協調スリープ待ち中に Bus-Off が発生し（ComM_ChannelMode が
 *          一時的に SILENT_COMMUNICATION になる）、その最中にユーザーが
 *          FULL_COM を再要求した場合。
 *
 *          この再要求は ComM_ChannelMode（SILENT_COM）とは異なるため、
 *          単純な「channel==aggregated なら再要求とみなしてキャンセル」判定
 *          には引っかからず、CanSM_RequestComMode(FULL_COM) まで落ちるが
 *          Bus-Off 回復中のため拒否される。ここで ComM_NmReleasePending を
 *          解除し損ねると、Bus-Off 回復後に「誰も再要求していない」と
 *          誤認してコントローラを眠らせてしまい、CAN_CS_SLEEP からの
 *          CAN_T_START は Can.c が拒否するため、外部 CAN ウェイクアップ
 *          割り込みが来るまで ECU が誤って眠り続ける。
 */
TEST_F(Bsw_SleepCoordination_Test, ReRequestFullComDuringBusOff_OK_RestoresFullComAfterRecovery)
{
    ArrangeFullCom();
    ASSERT_EQ(ComM_RequestComMode(COMM_USER_0, COMM_NO_COMMUNICATION), E_OK);

    /* 実行 (Act): Nm の協調スリープ待ち中に Bus-Off が発生する */
    CanSM_ControllerBusOff(0U);
    ComM_ModeType comMode = COMM_FULL_COMMUNICATION;
    ASSERT_EQ(ComM_GetCurrentComMode(COMM_USER_0, &comMode), E_OK);
    ASSERT_EQ(comMode, static_cast<ComM_ModeType>(COMM_SILENT_COMMUNICATION));

    /* Bus-Off 回復前に、ユーザーが FULL_COM を再要求する
     * （エンジンが再始動した等）。CanSM は Bus-Off 回復中のため
     * CanSM_RequestComMode(FULL_COM) は内部的に拒否されるはずだが、
     * ComM_NmReleasePending は解除されなければならない。 */
    ASSERT_EQ(ComM_RequestComMode(COMM_USER_0, COMM_FULL_COMMUNICATION), E_OK);

    /* Bus-Off から回復させる（L1 周期経過） */
    FakeMillis_Value += CANSM_BUSOFF_RECOVERY_L1_MS + 1UL;
    CanSM_MainFunction();

    /* 評価 (Assert): 誰も望んでいないのにコントローラが眠ってしまうことなく、
     * FULL_COM へ正しく復帰する。Nm も再起床（送信再開）している。 */
    ASSERT_EQ(ComM_GetCurrentComMode(COMM_USER_0, &comMode), E_OK);
    EXPECT_EQ(comMode, static_cast<ComM_ModeType>(COMM_FULL_COMMUNICATION));
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);

    Nm_StateType state;
    Nm_ModeType  nmMode;
    ASSERT_EQ(Nm_GetState(&state, &nmMode), E_OK);
    EXPECT_EQ(nmMode, NM_MODE_NETWORK);
    EXPECT_NE(state, NM_STATE_BUS_SLEEP);
}

}  // namespace
