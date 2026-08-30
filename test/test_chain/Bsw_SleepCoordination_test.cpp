/**
 * \file    Bsw_SleepCoordination_test.cpp
 * \brief   Nm↔CanSM↔ComM 協調スリープ通知（2026-08、CanSM仲介からComM経由への
 *          移管、および同年の ComM_Nm_PrepareBusSleepMode()/
 *          ComM_Nm_NetworkMode() 追加）の単体テスト
 *          （GoogleTest / PlatformIO `[env:native_chain]`）。
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
 *          4. ReRequestFullComDuringBusOff_OK_RestoresFullComAfterRecovery:
 *             上記3と類似だが、Bus-Off 回復待ち中にユーザーが FULL_COM を
 *             再要求したケース（/code-review で指摘された回帰の防止）。
 *
 *          5〜8（2026-08 追加、ComM_Nm_PrepareBusSleepMode()/
 *          ComM_Nm_NetworkMode()、[SWS_ComM_00826]/[SWS_ComM_00296]）:
 *          5. VoluntarySleep_OK_SilencesChannelAtPrepareBusSleep:
 *             Prepare Bus-Sleep Mode 到達時にチャネルが SILENT_COM
 *             （受信専用、EcuM RUN は維持）へ切り替わり、その後 Bus-Sleep
 *             Mode 到達で NO_COM へ収束すること。
 *          6. RxCancelsPrepareBusSleep_OK_RestoresFullComAndTransmits:
 *             SILENT_COM 中に他ノードの NM フレームを受信すると、Nm の
 *             自律復帰と同時にチャネルが FULL_COM へ戻り、かつ
 *             （フラグだけでなく）実際に再アナウンスフレームの送信が
 *             HW まで到達すること。
 *          7. ReRequestFullComAfterPrepareBusSleep_OK_RestoresFullCom:
 *             SILENT_COM 中にユーザー API 経由で FULL_COM を再要求しても
 *             同様に復帰すること。
 *          8. BusOffDuringRxCancelledPrepareBusSleep_OK_ConvergesToFullCom
 *             （設計時の Plan エージェントレビューで発見した相互作用の回帰
 *             テスト）: Bus-Off 中に Nm が Prepare Bus-Sleep Mode へ到達し、
 *             さらに Bus-Off 中に他ノード RX で自律復帰した場合でも、
 *             ComM_NmReleasePending の無条件クリアにより最終的に FULL_COM
 *             へ正しく収束すること。
 *
 *          9. BusOffDuringSilentCom_OK_RestoresSilentComAfterRecovery
 *             （2026-08 追加、SILENT_COMMUNICATION を
 *             CanIf_SetPduMode(CANIF_TX_OFFLINE) ベースへ移行した際の回帰
 *             テスト）: コントローラが CAN_CS_STARTED のまま維持される本当の
 *             SILENT_COM 中に Bus-Off が発生しても無視されず処理され、回復後は
 *             FULL_COM ではなく元の SILENT_COM へ正しく戻ること。
 *
 *          EcuM（ComM の RUN 要求先）と BswM（ComM のモード通知先）は境界として
 *          フェイクに差し替える（Bsw_EcuM_fake.h/Bsw_BswM_fake.h 冒頭コメント
 *          参照）。CanIf は Nm が CanIf_Transmit() を直接呼ぶために実体で
 *          リンクするが、既定は TxPduCount=0 の空設定を渡す
 *          （Bsw_WakeupChain_test.cpp の kTestCanIfConfig と同じパターン）ため
 *          送信は毎回 E_NOT_OK で終わり、Com/PduR は不要。テスト6のみ、
 *          実際に送信が HW まで到達したことを検証するため
 *          `kTestCanIfConfigWithNmTx`（NM_CANIF_TX_PDU_ID のみ有効化）に
 *          差し替える。
 *
 *          本ファイルは当初 `[env:native_sleep_chain]` という別envに分離して
 *          いたが（協調スリープ移管作業のリスクを既存チェーンテストから
 *          切り離すため）、実機検証まで完了し安定したため `[env:native_chain]`
 *          へ統合した（Bsw_TxChain_test.cpp/Bsw_WakeupChain_test.cpp 等と
 *          CanSM.c/ComM.c/Nm.c の実体を共有する）。
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

/** テスト6専用: NM_CANIF_TX_PDU_ID（値2、Nm_Cfg.h）だけを有効化した TX PDU
 *  設定。「Nm の再送信が実際に Can_Hw_Send() まで到達したか」を
 *  FakeCanHw_SendCount で検証するために必要（既定の kTestCanIfConfig は
 *  TxPduCount=0 のため CanIf_Transmit() がコントローラの状態に関わらず常に
 *  E_NOT_OK で終わり、送信成否の検証に使えない）。index 0/1 はダミー。 */
const CanIf_TxPduConfigType kTestCanIfTxPduConfigWithNmTx[3] = {
    { 0U, 0U, 0U, 0U, NULL },
    { 0U, 0U, 0U, 0U, NULL },
    { /* UpperLayerTxPduId */ NM_CANIF_TX_PDU_ID,
      /* CanId */             0x400U,
      /* Dlc */               NM_DLC,
      /* Hth */               0U,
      /* TxConfirmFct */      Nm_TxConfirmation }
};

const CanIf_ConfigType kTestCanIfConfigWithNmTx = {
    /* TxPduConfig */ kTestCanIfTxPduConfigWithNmTx,
    /* TxPduCount */  3U,
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
        Nm_Init(NULL);

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
        ASSERT_EQ(Nm_GetState(NM_MAIN_NETWORK_HANDLE, &state, &mode), E_OK);
        ASSERT_EQ(state, NM_STATE_NORMAL_OPERATION);

        FakeCanHw_Reset();
        FakeDem_Reset();
        FakeEcuM_Reset();
        FakeBswM_Reset();
    }

    /** NO_COM を要求し、Nm が Prepare Bus-Sleep Mode（チャネルは SILENT_COM）
     *  へ到達するまで進める。ArrangeFullCom() の後に呼ぶこと。 */
    void ArrangeSilentComAtPrepareBusSleep()
    {
        ASSERT_EQ(ComM_RequestComMode(COMM_USER_0, COMM_NO_COMMUNICATION), E_OK);
        DriveNmUntil(NM_STATE_PREPARE_BUS_SLEEP);
        ComM_ModeType mode = COMM_FULL_COMMUNICATION;
        ASSERT_EQ(ComM_GetCurrentComMode(COMM_USER_0, &mode), E_OK);
        ASSERT_EQ(mode, static_cast<ComM_ModeType>(COMM_SILENT_COMMUNICATION));
        /* 2026-08 変更: SILENT_COM は CanIf_SetPduMode(CANIF_TX_OFFLINE) で
         * TX のみ抑制する方式になったため、コントローラ自体は CAN_CS_STARTED
         * のまま（旧: Can_T_STOP による Listen-Only、CAN_CS_STOPPED）。 */
        ASSERT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);
    }

    /** 他ノード(node=0x02)の NM フレーム受信を模擬する。 */
    static void SimulateOtherNodeNmRx()
    {
        uint8 rxData[NM_DLC] = { 0x00U, 0x02U };
        PduInfoType rxPdu = { rxData, NM_DLC };
        Nm_RxIndication(0U, &rxPdu);
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
            ASSERT_EQ(Nm_GetState(NM_MAIN_NETWORK_HANDLE, &state, &mode), E_OK);
            if (state == target)
                return;
            FakeMillis_Value += NM_CYCLE_MS;
            Nm_MainFunction();
        }
        ASSERT_EQ(Nm_GetState(NM_MAIN_NETWORK_HANDLE, &state, &mode), E_OK);
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
        ASSERT_EQ(Nm_GetState(NM_MAIN_NETWORK_HANDLE, &state, &mode), E_OK);
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
    ASSERT_EQ(Nm_GetState(NM_MAIN_NETWORK_HANDLE, &state, &nmMode), E_OK);
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
    ASSERT_EQ(Nm_GetState(NM_MAIN_NETWORK_HANDLE, &state, &nmMode), E_OK);
    EXPECT_EQ(nmMode, NM_MODE_NETWORK);
    EXPECT_NE(state, NM_STATE_BUS_SLEEP);
}

/**
 * \brief   2026-08 追加（ComM_Nm_PrepareBusSleepMode()、[SWS_ComM_00826]）:
 *          Prepare Bus-Sleep Mode 到達時点でチャネルが SILENT_COM（受信専用）
 *          へ切り替わり、まだ物理スリープはしないこと。
 */
TEST_F(Bsw_SleepCoordination_Test, VoluntarySleep_OK_SilencesChannelAtPrepareBusSleep)
{
    ArrangeFullCom();

    /* 実行 (Act): Nm を Prepare Bus-Sleep Mode まで進める */
    ArrangeSilentComAtPrepareBusSleep();

    /* 評価 (Assert): SILENT_COM（受信専用）へ切り替わり済みだが、まだ物理
     * スリープはしておらず EcuM RUN も維持されている。 */
    EXPECT_EQ(FakeEcuM_ReleaseRUNCount, 0U);

    /* 実行 (Act): Nm を Bus-Sleep Mode まで進める */
    DriveNmUntil(NM_STATE_BUS_SLEEP);

    /* 評価 (Assert): 既存テスト（VoluntarySleep_OK_...）と同じ最終状態へ収束する */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_SLEEP);
    EXPECT_EQ(FakeEcuM_ReleaseRUNCount, 1U);
    ComM_ModeType mode = COMM_SILENT_COMMUNICATION;
    ASSERT_EQ(ComM_GetCurrentComMode(COMM_USER_0, &mode), E_OK);
    EXPECT_EQ(mode, static_cast<ComM_ModeType>(COMM_NO_COMMUNICATION));
}

/**
 * \brief   2026-08 追加（ComM_Nm_NetworkMode()、[SWS_ComM_00296]）:
 *          SILENT_COM 中（Prepare Bus-Sleep Mode）に他ノードの NM フレームを
 *          受信すると、Nm の自律復帰（[SWS_CanNm_00124]）と同時にチャネルが
 *          FULL_COM へ戻り、かつ（フラグだけでなく）再アナウンスフレームの
 *          送信が実際に HW まで到達すること。
 *          `kTestCanIfConfigWithNmTx` で NM_CANIF_TX_PDU_ID を有効化する。
 */
TEST_F(Bsw_SleepCoordination_Test, RxCancelsPrepareBusSleep_OK_RestoresFullComAndTransmits)
{
    CanIf_Init(&kTestCanIfConfigWithNmTx);

    ArrangeFullCom();
    ArrangeSilentComAtPrepareBusSleep();

    FakeCanHw_Reset();
    FakeDem_Reset();
    FakeEcuM_Reset();
    FakeBswM_Reset();

    /* 実行 (Act): 他ノード(node=0x02)の NM フレーム受信を模擬する */
    SimulateOtherNodeNmRx();

    /* 評価 (Assert) */
    Nm_StateType state;
    Nm_ModeType  nmMode;
    ASSERT_EQ(Nm_GetState(NM_MAIN_NETWORK_HANDLE, &state, &nmMode), E_OK);
    EXPECT_EQ(state, NM_STATE_REPEAT_MESSAGE);
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);
    ComM_ModeType mode = COMM_SILENT_COMMUNICATION;
    ASSERT_EQ(ComM_GetCurrentComMode(COMM_USER_0, &mode), E_OK);
    EXPECT_EQ(mode, static_cast<ComM_ModeType>(COMM_FULL_COMMUNICATION));
    /* フラグだけでなく、再アナウンスフレームの送信が実際に HW まで
     * 到達したことを確認する（この設計の核心的な検証）。 */
    EXPECT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendDlc, static_cast<uint8_t>(NM_DLC));
    EXPECT_EQ(FakeEcuM_ReleaseRUNCount, 0U);
    EXPECT_EQ(FakeEcuM_RequestRUNCount, 0U);  /* RUN は SILENT_COM 中も維持されたまま */
}

/**
 * \brief   2026-08 追加: SILENT_COM 中（Prepare Bus-Sleep Mode）にユーザー
 *          API 経由で FULL_COM を再要求した場合も正しく復帰すること。
 *          `ComM_RequestComMode()` の再要求キャンセル分岐が
 *          `Nm_NetworkRequest()` → `Nm_EnterRepeatMessage()` →
 *          `ComM_Nm_NetworkMode()` 経由で初めて間接的に CanSM へ到達する
 *          経路の確認。
 */
TEST_F(Bsw_SleepCoordination_Test, ReRequestFullComAfterPrepareBusSleep_OK_RestoresFullCom)
{
    ArrangeFullCom();
    ArrangeSilentComAtPrepareBusSleep();

    /* 実行 (Act): ユーザー API 経由で FULL_COM を再要求する */
    ASSERT_EQ(ComM_RequestComMode(COMM_USER_0, COMM_FULL_COMMUNICATION), E_OK);

    /* 評価 (Assert) */
    ComM_ModeType mode = COMM_SILENT_COMMUNICATION;
    ASSERT_EQ(ComM_GetCurrentComMode(COMM_USER_0, &mode), E_OK);
    EXPECT_EQ(mode, static_cast<ComM_ModeType>(COMM_FULL_COMMUNICATION));
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);

    Nm_StateType state;
    Nm_ModeType  nmMode;
    ASSERT_EQ(Nm_GetState(NM_MAIN_NETWORK_HANDLE, &state, &nmMode), E_OK);
    EXPECT_EQ(nmMode, NM_MODE_NETWORK);
    EXPECT_NE(state, NM_STATE_BUS_SLEEP);

    /* さらに何ティックか進めても再スリープしないことを確認する
     * （既存テスト ReRequestFullCom_OK_CancelsPendingNmRelease と同じ形式）。 */
    for (int i = 0; i < 15; i++)
    {
        FakeMillis_Value += NM_CYCLE_MS;
        Nm_MainFunction();
        ASSERT_EQ(Nm_GetState(NM_MAIN_NETWORK_HANDLE, &state, &nmMode), E_OK);
        ASSERT_NE(state, NM_STATE_BUS_SLEEP);
    }
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);
}

/**
 * \brief   2026-08 追加（実機ログで発見した回帰の防止、最重要）:
 *          App_EngineManager は ENGINE_STATE_OFF が続く限り、毎周期
 *          ComM_RequestComMode(COMM_USER_0, COMM_NO_COMMUNICATION) を
 *          冗長に呼び続ける（App_EngineManager.c 参照）。Nm が Prepare
 *          Bus-Sleep Mode へ到達しチャネルが SILENT_COM になった後にこの
 *          冗長な再要求が来ても、Nm が Bus-Sleep Mode へ到達するまでは
 *          コントローラを物理スリープさせてはならない。
 */
TEST_F(Bsw_SleepCoordination_Test, RedundantNoComRequestDuringSilentCom_OK_DoesNotSleepEarly)
{
    ArrangeFullCom();
    ArrangeSilentComAtPrepareBusSleep();

    /* 実行 (Act): App_EngineManager が実機で行う冗長な再要求を模擬する */
    ASSERT_EQ(ComM_RequestComMode(COMM_USER_0, COMM_NO_COMMUNICATION), E_OK);

    /* 評価 (Assert): まだ物理スリープしていない。Nm もまだ Prepare Bus-Sleep
     * Mode のまま（他ノードの NM フレームによるキャンセルをまだ待てる）。
     * SILENT_COM 中はコントローラ自体は CAN_CS_STARTED のまま
     * （CanIf_SetPduMode(CANIF_TX_OFFLINE) で TX のみ抑制、2026-08 変更）。 */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);
    ComM_ModeType mode = COMM_FULL_COMMUNICATION;
    ASSERT_EQ(ComM_GetCurrentComMode(COMM_USER_0, &mode), E_OK);
    EXPECT_EQ(mode, static_cast<ComM_ModeType>(COMM_SILENT_COMMUNICATION));
    Nm_StateType state;
    Nm_ModeType  nmMode;
    ASSERT_EQ(Nm_GetState(NM_MAIN_NETWORK_HANDLE, &state, &nmMode), E_OK);
    EXPECT_EQ(state, NM_STATE_PREPARE_BUS_SLEEP);

    /* 実行 (Act): Nm を Bus-Sleep Mode まで進める */
    DriveNmUntil(NM_STATE_BUS_SLEEP);

    /* 評価 (Assert): ここで初めて物理スリープする */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_SLEEP);
    ASSERT_EQ(ComM_GetCurrentComMode(COMM_USER_0, &mode), E_OK);
    EXPECT_EQ(mode, static_cast<ComM_ModeType>(COMM_NO_COMMUNICATION));
}

/**
 * \brief   2026-08 追加（設計時の Plan エージェントレビューで発見した相互作用の
 *          回帰テスト、最重要）:
 *          Bus-Off 中に Nm が Prepare Bus-Sleep Mode へ独立して到達し
 *          （Stage 1 は SILENT_COM 済みのため no-op）、さらに Bus-Off 中に
 *          他ノード RX で Nm がローカルに自律復帰した場合
 *          （CANSM_STATE_BUS_OFF 中もコントローラは受信を継続するため、
 *          これは実際に起こりうる）、CanSM は Bus-Off 中のため FULL_COM 要求を
 *          一旦拒否するが、ComM_Nm_NetworkMode() が ComM_NmReleasePending を
 *          無条件でクリアしているおかげで、Bus-Off 回復後に既存の
 *          リトライガードが誤発火せず、最終的に FULL_COM へ正しく収束する
 *          こと（このクリアが条件付きだった場合に失敗する）。
 */
TEST_F(Bsw_SleepCoordination_Test, BusOffDuringRxCancelledPrepareBusSleep_OK_ConvergesToFullCom)
{
    ArrangeFullCom();
    ASSERT_EQ(ComM_RequestComMode(COMM_USER_0, COMM_NO_COMMUNICATION), E_OK);

    /* 実行 (Act): Nm が Network Mode 中に Bus-Off が発生する */
    CanSM_ControllerBusOff(0U);
    ComM_ModeType mode = COMM_FULL_COMMUNICATION;
    ASSERT_EQ(ComM_GetCurrentComMode(COMM_USER_0, &mode), E_OK);
    ASSERT_EQ(mode, static_cast<ComM_ModeType>(COMM_SILENT_COMMUNICATION));

    /* Nm は Bus-Off とは無関係に独立して Prepare Bus-Sleep Mode へ到達する。
     * ComM_Nm_PrepareBusSleepMode() はチャネルが既に SILENT_COM のため
     * no-op のはず（Bus-Off 回復と衝突しない）。 */
    DriveNmUntil(NM_STATE_PREPARE_BUS_SLEEP);
    ASSERT_EQ(ComM_GetCurrentComMode(COMM_USER_0, &mode), E_OK);
    ASSERT_EQ(mode, static_cast<ComM_ModeType>(COMM_SILENT_COMMUNICATION));

    /* Bus-Off 中に他ノードの NM フレームを受信する（コントローラは
     * Listen-Only で受信は継続している）。 */
    SimulateOtherNodeNmRx();

    /* 評価 (Assert): Nm はローカルに自律復帰したが、CanSM は Bus-Off 中の
     * ため FULL_COM 要求を拒否し、チャネルは SILENT_COM のまま。 */
    Nm_StateType state;
    Nm_ModeType  nmMode;
    ASSERT_EQ(Nm_GetState(NM_MAIN_NETWORK_HANDLE, &state, &nmMode), E_OK);
    EXPECT_EQ(state, NM_STATE_REPEAT_MESSAGE);
    ASSERT_EQ(ComM_GetCurrentComMode(COMM_USER_0, &mode), E_OK);
    EXPECT_EQ(mode, static_cast<ComM_ModeType>(COMM_SILENT_COMMUNICATION));

    /* 実行 (Act): Bus-Off から回復させる（L1 周期経過） */
    FakeMillis_Value += CANSM_BUSOFF_RECOVERY_L1_MS + 1UL;
    CanSM_MainFunction();

    /* 評価 (Assert): ComM_NmReleasePending が無条件クリアされていたおかげで、
     * 既存のリトライガードは誤発火せず、FULL_COM へ正しく収束する。 */
    ASSERT_EQ(ComM_GetCurrentComMode(COMM_USER_0, &mode), E_OK);
    EXPECT_EQ(mode, static_cast<ComM_ModeType>(COMM_FULL_COMMUNICATION));
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);
    ASSERT_EQ(Nm_GetState(NM_MAIN_NETWORK_HANDLE, &state, &nmMode), E_OK);
    EXPECT_EQ(nmMode, NM_MODE_NETWORK);
    EXPECT_NE(state, NM_STATE_BUS_SLEEP);
}

/**
 * \brief   2026-08 追加（CanSM の SILENT_COMMUNICATION を
 *          CanIf_SetPduMode(CANIF_TX_OFFLINE) ベースへ移行した際の回帰テスト）:
 *          コントローラが CAN_CS_STARTED のまま維持される本当の
 *          CANSM_STATE_SILENT_COM の最中に Bus-Off が発生した場合、
 *          CanSM_ControllerBusOff() のガードに無視されず正しく BUS_OFF へ
 *          遷移すること。旧設計（Listen-Only で Bus-Off しない）ではこの経路
 *          自体が存在しなかった。回復後は FULL_COM ではなく、Bus-Off 発生
 *          直前の SILENT_COM へ戻ること（CanSM_PreBusOffState 参照）。
 *
 *          Nm 協調スリープ経由（ArrangeSilentComAtPrepareBusSleep()）ではなく
 *          CanSM_RequestComMode(SILENT_COM) を直接呼んで SILENT_COM へ到達
 *          させる。前者は必ず ComM_NmReleasePending を立てた状態になり、
 *          回復時に ComM 側の別のリトライ（Nm の解放要求の仕切り直し、
 *          BusOffDuringSilentCom_OK_ConvergesToNoComWhenNmReachesBusSleep
 *          DuringOutage 参照）が働いて最終的に NO_COM まで収束してしまうため、
 *          「CanSM が Bus-Off 発生直前の状態そのものへ戻る」という本テストの
 *          関心事（ComM 側のポリシーとは独立した CanSM 単体の性質）を
 *          NmReleasePending の影響を受けずに検証するため。
 */
TEST_F(Bsw_SleepCoordination_Test, BusOffDuringSilentCom_OK_RestoresSilentComAfterRecovery)
{
    ArrangeFullCom();
    ASSERT_EQ(CanSM_RequestComMode(0U, COMM_SILENT_COMMUNICATION), E_OK);
    ASSERT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);

    /* 実行 (Act): 本当に SILENT_COM 状態（コントローラ CAN_CS_STARTED）の
     * 最中に Bus-Off が発生する。 */
    CanSM_ControllerBusOff(0U);

    /* 評価 (Assert): 無視されず処理され、コントローラは Listen-Only
     * (CAN_CS_STOPPED) へ落ちる。SILENT_COM 自体は Can_SetControllerMode()
     * を一切呼ばない設計のため、ここで STOPPED になったことは Bus-Off が
     * 正しく処理された証拠になる（ガードで無視されていれば STARTED のまま）。 */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STOPPED);
    ComM_ModeType mode = COMM_FULL_COMMUNICATION;
    ASSERT_EQ(ComM_GetCurrentComMode(COMM_USER_0, &mode), E_OK);
    EXPECT_EQ(mode, static_cast<ComM_ModeType>(COMM_SILENT_COMMUNICATION));

    /* 実行 (Act): Bus-Off から回復させる（L1 周期経過） */
    FakeMillis_Value += CANSM_BUSOFF_RECOVERY_L1_MS + 1UL;
    CanSM_MainFunction();

    /* 評価 (Assert): FULL_COM ではなく、Bus-Off 発生直前の SILENT_COM へ
     * 戻ること。コントローラは CAN_CS_STARTED（TX は CanIf 側で抑制されたまま）。 */
    ASSERT_EQ(ComM_GetCurrentComMode(COMM_USER_0, &mode), E_OK);
    EXPECT_EQ(mode, static_cast<ComM_ModeType>(COMM_SILENT_COMMUNICATION));
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);

    /* 評価 (Assert): CanIf 側の PDU モードは Bus-Off 中も再設定不要で
     * CANIF_TX_OFFLINE のまま保持されていること（設計の核心）。 */
    CanIf_PduModeType pduMode = CANIF_ONLINE;
    ASSERT_EQ(CanIf_GetPduMode(0U, &pduMode), E_OK);
    EXPECT_EQ(pduMode, CANIF_TX_OFFLINE);
}

/**
 * \brief   2026-08 追加（/code-review で発見した回帰の防止、最重要）:
 *          本当の SILENT_COM 中に Bus-Off が発生し、その最中に Nm が
 *          （Bus-Off とは無関係に独立したタイマで）Bus-Sleep Mode へ到達した
 *          場合、ComM_Nm_BusSleepMode() 経由の CanSM_RequestComMode(NO_COM)
 *          は CanSM が Bus-Off 回復中のため一旦拒否される。Nm 側はこの一発勝負
 *          の通知を再送しないため、Bus-Off 回復後に ComM が自ら
 *          ComM_NmReleasePending を見てリトライしない限り、チャネルは
 *          SILENT_COM のまま永久に取り残され、ECU は二度と物理スリープしない
 *          （ComM_BusSM_ModeIndication() の COMM_SILENT_COMMUNICATION 分岐に
 *          追加したリトライで対応、ComM.c 参照）。
 */
TEST_F(Bsw_SleepCoordination_Test, BusOffDuringSilentCom_OK_ConvergesToNoComWhenNmReachesBusSleepDuringOutage)
{
    ArrangeFullCom();
    ArrangeSilentComAtPrepareBusSleep();

    /* 実行 (Act): 本当に SILENT_COM 状態の最中に Bus-Off が発生する。 */
    CanSM_ControllerBusOff(0U);
    ASSERT_EQ(Can_Test_GetControllerState(), CAN_CS_STOPPED);

    /* Nm は Bus-Off とは無関係に独立して Bus-Sleep Mode へ到達する。この
     * 通知（ComM_Nm_BusSleepMode() 経由の CanSM_RequestComMode(NO_COM)）は
     * CanSM が Bus-Off 回復中のため、この時点では拒否される。 */
    DriveNmUntil(NM_STATE_BUS_SLEEP);
    ComM_ModeType mode = COMM_FULL_COMMUNICATION;
    ASSERT_EQ(ComM_GetCurrentComMode(COMM_USER_0, &mode), E_OK);
    EXPECT_EQ(mode, static_cast<ComM_ModeType>(COMM_SILENT_COMMUNICATION));  // まだ収束していない

    /* 実行 (Act): Bus-Off から回復させる（L1 周期経過） */
    FakeMillis_Value += CANSM_BUSOFF_RECOVERY_L1_MS + 1UL;
    CanSM_MainFunction();

    /* 評価 (Assert): CanSM は Bus-Off 発生直前の SILENT_COM へ復帰するが、
     * ComM 側のリトライにより Nm の解放要求が仕切り直され、最終的に
     * NO_COM（物理スリープ）へ正しく収束する。取り残されない。 */
    ASSERT_EQ(ComM_GetCurrentComMode(COMM_USER_0, &mode), E_OK);
    EXPECT_EQ(mode, static_cast<ComM_ModeType>(COMM_NO_COMMUNICATION));
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_SLEEP);
    EXPECT_EQ(FakeEcuM_ReleaseRUNCount, 1U);
}

}  // namespace
