/**
 * \file    Bsw_Nm_CommunicationControlTimeout_test.cpp
 * \brief   診断 CommunicationControl(UDS 0x28) 中の NM-Timeout Timer 停止/再起動
 *          （[SWS_CanNm_00174]/[SWS_CanNm_00179]）の単体テスト
 *          （GoogleTest / PlatformIO `[env:native_chain]`）。
 *
 * \details 2026-09 是正の対象: `Nm_DisableCommunication()` で NM PDU 送信を
 *          無効化しても、`Nm_MainFunction()` の NM-Timeout Timer 満了判定は
 *          止まらず、他ノードが存在しない/自ノードの送信も止まっている状況で
 *          `NM_E_NETWORK_TIMEOUT` の DET 報告が周期的に空しく繰り返されて
 *          いた不具合。`Nm_TxEnabled` を満了判定の条件に加え、再有効化時
 *          （`Nm_EnableCommunication()`）にタイマーを再起動するよう修正した。
 *
 *          `kTestCanIfConfig`（NM_CANIF_TX_PDU_ID のみ有効化、
 *          `Bsw_SleepCoordination_test.cpp` の `kTestCanIfConfigWithNmTx` と
 *          同じパターン）を使う。TxPduCount=0 の空設定だと、Nm の周期送信
 *          （`Nm_TransmitPdu()` → `CanIf_Transmit()`）のたびに
 *          `CANIF_E_INVALID_TXPDUID` が DET 報告されてしまい、
 *          `NM_E_NETWORK_TIMEOUT` 報告の有無を検証したい DET スパイの
 *          `FakeDetHw_Last*`（直近1件のみ記録）にノイズが混ざるため。
 */
#include <gtest/gtest.h>

extern "C" {
#include "Can.h"
#include "Can_Hw.h"
#include "CanIf.h"
#include "CanSM.h"
#include "ComM.h"
#include "Nm.h"
#include "Nm_Cfg.h"
#include "Hal_Can_Hw_fake.h"
#include "Hal_Millis_fake.h"
#include "Hal_Det_Hw_fake.h"
#include "Wrap_Dem.h"
#include "Bsw_EcuM_fake.h"
#include "Bsw_BswM_fake.h"
}

namespace
{

/* Nm の周期送信(NM_CANIF_TX_PDU_ID)が CanIf 層で CANIF_E_INVALID_TXPDUID を
 * 報告してしまうと、NM_E_NETWORK_TIMEOUT の DET 報告有無の検証にノイズが
 * 混ざる(Bsw_SleepCoordination_test.cpp の kTestCanIfConfigWithNmTx と同じ
 * 理由・同じパターン)。index 0/1 はダミー。 */
const CanIf_TxPduConfigType kTestCanIfTxPduConfigWithNmTx[3] = {
    { 0U, 0U, 0U, 0U, NULL },
    { 0U, 0U, 0U, 0U, NULL },
    { /* UpperLayerTxPduId */ NM_CANIF_TX_PDU_ID,
      /* CanId */             0x400U,
      /* Dlc */               NM_DLC,
      /* Hth */               0U,
      /* TxConfirmFct */      Nm_TxConfirmation }
};

const CanIf_ConfigType kTestCanIfConfig = {
    /* TxPduConfig */ kTestCanIfTxPduConfigWithNmTx,
    /* TxPduCount */  3U,
    /* RxPduConfig */ NULL,
    /* RxPduCount */  0U
};

class Bsw_Nm_CommunicationControlTimeout_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeCanHw_Reset();
        WrapDemSetEventStatus_Reset();
        Dem_Init(NULL);  // Demの内部状態を毎テスト決定的にリセットする（NvM_fake.cにより常に「初回起動」）
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
        ComM_CommunicationAllowed(COMM_CHANNEL_0, TRUE);  // 実 EcuM_Init() と同じく起動時に許可
        Nm_Init(NULL);

        // NORMAL_OPERATION State まで進める(Bsw_SleepCoordination_test.cpp の
        // ArrangeFullCom() と同じ流儀)。
        ASSERT_EQ(ComM_RequestComMode(COMM_USER_0, COMM_FULL_COMMUNICATION), E_OK);
        FakeMillis_Value += NM_REPEAT_MESSAGE_MS + 100UL;
        Nm_MainFunction();
        Nm_StateType state;
        Nm_ModeType  mode;
        ASSERT_EQ(Nm_GetState(NM_MAIN_NETWORK_HANDLE, &state, &mode), E_OK);
        ASSERT_EQ(state, NM_STATE_NORMAL_OPERATION);

        FakeDetHw_Reset();              // ここまでの DET 記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;   // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Nm_DeInit();
        ComM_DeInit();
        CanSM_DeInit();
        CanIf_DeInit();
    }

    static bool NetworkTimeoutReported()
    {
        return FakeDetHw_ReportCount > 0U && FakeDetHw_LastModuleId == NM_MODULE_ID
               && FakeDetHw_LastApiId == NM_API_ID_MAIN_FUNCTION && FakeDetHw_LastErrorId == NM_E_NETWORK_TIMEOUT;
    }

    Can_ConfigType canConfig;
};

// ------------------------------------------------------------
// 制御群: 送信有効のまま(既定)放置すれば、通常どおり NM-Timeout Timer は
// 満了して NM_E_NETWORK_TIMEOUT が報告される(是正前から変わらない挙動)。
// ------------------------------------------------------------
TEST_F(Bsw_Nm_CommunicationControlTimeout_Test, MainFunction_NG_TimeoutFiresNormallyWhenEnabled)
{
    FakeMillis_Value += NM_TIMEOUT_MS + 1UL;

    Nm_MainFunction();

    EXPECT_TRUE(NetworkTimeoutReported());
}

// ------------------------------------------------------------
// 2026-09 是正の本題: 送信無効化中は NM-Timeout Timer の満了判定自体を
// 止めるため、いくら時間が経っても NM_E_NETWORK_TIMEOUT は報告されない。
// ------------------------------------------------------------
TEST_F(Bsw_Nm_CommunicationControlTimeout_Test, MainFunction_OK_TimeoutDoesNotFireWhileDisabled)
{
    ASSERT_EQ(Nm_DisableCommunication(NM_MAIN_NETWORK_HANDLE), E_OK);

    for (uint8 i = 0U; i < 5U; i++)
    {
        FakeMillis_Value += NM_TIMEOUT_MS + 1UL;
        Nm_MainFunction();
    }

    EXPECT_FALSE(NetworkTimeoutReported());
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);

    Nm_StateType state;
    Nm_ModeType  mode;
    ASSERT_EQ(Nm_GetState(NM_MAIN_NETWORK_HANDLE, &state, &mode), E_OK);
    EXPECT_EQ(state, NM_STATE_NORMAL_OPERATION);  // 状態機械自体は無関係に維持される
}

// ------------------------------------------------------------
// 再有効化時にタイマーが再起動されること(スタックした古い基準時刻のまま
// 残っていれば、再有効化直後の1周期で即座に見かけ上の満了が起きるはず)。
// ------------------------------------------------------------
TEST_F(Bsw_Nm_CommunicationControlTimeout_Test, MainFunction_OK_TimeoutRestartsOnReEnable)
{
    ASSERT_EQ(Nm_DisableCommunication(NM_MAIN_NETWORK_HANDLE), E_OK);
    FakeMillis_Value += 3UL * NM_TIMEOUT_MS;  // 無効化中に古い基準時刻を大きく経過させる
    Nm_MainFunction();
    ASSERT_FALSE(NetworkTimeoutReported());  // 無効化中は満了しない(前テストと同じ)

    ASSERT_EQ(Nm_EnableCommunication(NM_MAIN_NETWORK_HANDLE), E_OK);

    /* 再起動されていれば、NM_TIMEOUT_MS未満の経過ではまだ満了しない
     * (再起動されず古い基準時刻のままなら、この時点で経過時間は既に
     * 3*NM_TIMEOUT_MS を超えており即座に満了してしまうはず)。 */
    FakeMillis_Value += NM_TIMEOUT_MS - 100UL;
    Nm_MainFunction();
    EXPECT_FALSE(NetworkTimeoutReported());

    /* 再起動後の基準時刻から改めて NM_TIMEOUT_MS 経過すれば、通常どおり満了する
     * (タイマーが再有効化後も引き続き正常に機能していることの確認)。 */
    FakeMillis_Value += 200UL;
    Nm_MainFunction();
    EXPECT_TRUE(NetworkTimeoutReported());
}

// ------------------------------------------------------------
// 自己/simplify(altitude観点)の指摘: 修正が及ぶ3状態のうち
// NORMAL_OPERATION でしか検証していなかったため、REPEAT_MESSAGE/
// READY_SLEEP でも同様にゲートが効くことを直接確認する。
// ------------------------------------------------------------
TEST_F(Bsw_Nm_CommunicationControlTimeout_Test, MainFunction_OK_TimeoutDoesNotFireWhileDisabledInRepeatMessage)
{
    ASSERT_EQ(Nm_RepeatMessageRequest(NM_MAIN_NETWORK_HANDLE), E_OK);  // NORMAL_OPERATION -> REPEAT_MESSAGE
    Nm_StateType state;
    Nm_ModeType  mode;
    ASSERT_EQ(Nm_GetState(NM_MAIN_NETWORK_HANDLE, &state, &mode), E_OK);
    ASSERT_EQ(state, NM_STATE_REPEAT_MESSAGE);
    FakeDetHw_Reset();

    ASSERT_EQ(Nm_DisableCommunication(NM_MAIN_NETWORK_HANDLE), E_OK);
    /* NM_TIMEOUT_MS(3000ms) は NM_REPEAT_MESSAGE_MS(1500ms) より長いため、
     * この経過で Repeat Message State 自体の継続時間(Nm_StateTimerMs、
     * 送信無効化とは無関係に無条件で進む)も同時に超過し、この1回の
     * Nm_MainFunction() 呼び出し内で NORMAL_OPERATION へ自動遷移する
     * （[SWS_CanNm_00170] の Note どおり、Communication Control は Repeat
     * Message State の「長さ」自体には影響しない）。ここで検証したいのは
     * その遷移とは独立な「NM-Timeout Timer 満了判定だけは無効化中スキップ
     * された」という点のみ。 */
    FakeMillis_Value += NM_TIMEOUT_MS + 1UL;
    Nm_MainFunction();

    EXPECT_FALSE(NetworkTimeoutReported());
}

TEST_F(Bsw_Nm_CommunicationControlTimeout_Test, MainFunction_OK_DoesNotEnterPrepareBusSleepWhileDisabledInReadySleep)
{
    ASSERT_EQ(Nm_NetworkRelease(NM_MAIN_NETWORK_HANDLE), E_OK);  // NORMAL_OPERATION -> READY_SLEEP
    Nm_StateType state;
    Nm_ModeType  mode;
    ASSERT_EQ(Nm_GetState(NM_MAIN_NETWORK_HANDLE, &state, &mode), E_OK);
    ASSERT_EQ(state, NM_STATE_READY_SLEEP);

    ASSERT_EQ(Nm_DisableCommunication(NM_MAIN_NETWORK_HANDLE), E_OK);
    FakeMillis_Value += NM_TIMEOUT_MS + 1UL;
    Nm_MainFunction();

    /* READY_SLEEP の満了アクションは DET 報告ではなく
     * Nm_EnterPrepareBusSleep() への遷移([SWS_CanNm_00109])。無効化中は
     * これも起きないはず。 */
    ASSERT_EQ(Nm_GetState(NM_MAIN_NETWORK_HANDLE, &state, &mode), E_OK);
    EXPECT_EQ(state, NM_STATE_READY_SLEEP);
}

}  // namespace
