/**
 * \file    CanNm_MainFunction_test.cpp
 * \brief   CanNm_MainFunction() の単体テスト
 * \details 2026-10-16、モジュールの単体テストは関数ごとにファイルを分ける方針に改め、旧 CanNm_test.cpp（1 ファイルに 3 つのフィクスチャを統合していた）を関数単位へ分割した。
 *          診断の通信制御（UDS 0x28）で送信を無効化している間は NM-Timeout Timer の満了判定を止め、再有効化でタイマーを再起動する。
 *          NORMAL_OPERATION・REPEAT_MESSAGE・READY_SLEEP のいずれでも無効化中は満了しないこと、送信有効のままなら通常どおり満了することを確認する。
 *          未初期化での呼び出しは CANNM_E_UNINIT を報告する（軽量フィクスチャ）。
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
#include "ComM.h"
#include "CanNm.h"
#include "CanNm_Cfg.h"
#include "Nm.h"
#include "Fake_Can_Hw.h"
#include "Fake_Millis.h"
#include "Fake_Det_Hw.h"
#include "Wrap_Dem.h"
#include "Fake_Bsw_EcuM.h"
#include "Wrap_BswM.h"
#include "Wrap_CanIf.h"
#include "Wrap_Can.h"
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

/**
 * \details [SWS_CanNm_00174]/[SWS_CanNm_00179] 準拠。2026-09 是正の対象:
 *          `CanNm_DisableCommunication()` で NM PDU 送信を無効化しても、
 *          `CanNm_MainFunction()` の NM-Timeout Timer 満了判定は止まらず、
 *          他ノードが存在しない/自ノードの送信も止まっている状況で
 *          `CANNM_E_NETWORK_TIMEOUT` の DET 報告が周期的に空しく繰り返されて
 *          いた不具合。`CanNm_TxEnabled` を満了判定の条件に加え、再有効化時
 *          （`CanNm_EnableCommunication()`）にタイマーを再起動するよう修正した。
 *
 *          `kCommControlTimeoutCanIfConfig`（CANNM_CANIF_TX_PDU_ID のみ有効化、
 *          `NmStack_SleepCoordination_test.cpp` の `kTestCanIfConfigWithNmTx` と
 *          同じパターン）を使う。TxPduCount=0 の空設定だと、CanNm の周期送信
 *          （`CanNm_TransmitPdu()` → `CanIf_Transmit()`）のたびに
 *          `CANIF_E_INVALID_TXPDUID` が DET 報告されてしまい、
 *          `CANNM_E_NETWORK_TIMEOUT` 報告の有無を検証したい DET スパイの
 *          `FakeDetHw_Last*`（直近1件のみ記録）にノイズが混ざるため。
 */

/* CanNm の周期送信(CANNM_CANIF_TX_PDU_ID)が CanIf 層で CANIF_E_INVALID_TXPDUID を
 * 報告してしまうと、CANNM_E_NETWORK_TIMEOUT の DET 報告有無の検証にノイズが
 * 混ざる(NmStack_SleepCoordination_test.cpp の kTestCanIfConfigWithNmTx と同じ
 * 理由・同じパターン)。index 0/1 はダミー。 */
const CanIf_TxPduConfigType kCommControlTimeoutCanIfTxPduConfig[3] = {
    { 0U, 0U, 0U, 0U, NULL },
    { 0U, 0U, 0U, 0U, NULL },
    { /* UpperLayerTxPduId */ CANNM_CANIF_TX_PDU_ID,
      /* CanId */             0x400U,
      /* Dlc */               CANNM_DLC,
      /* Hth */               0U,
      /* TxConfirmFct */      CanNm_TxConfirmation }
};

const CanIf_ConfigType kCommControlTimeoutCanIfConfig = {
    /* TxPduConfig */ kCommControlTimeoutCanIfTxPduConfig,
    /* TxPduCount */  3U,
    /* RxPduConfig */ NULL,
    /* RxPduCount */  0U
};


/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* CanNm_MainFunction() の NM-Timeout Timer（通信制御との関係）の単体テスト用フィクスチャ。
 * SetUp(): 模擬 Can_Hw・Dem・EcuM・BswM と wrap をリセットし、Dem_Init(NULL)・Can_Init()・CanIf_Init(&kCommControlTimeoutCanIfConfig)・CanSM_Init(NULL)・ComM_Init(NULL)（起動時と同じく通信を許可）・CanNm_Init(NULL)・Nm_Init(NULL) の順に呼び、CanNm を NORMAL_OPERATION State まで進める（NmStack_SleepCoordination_test.cpp の ArrangeFullCom() と同じ流儀）。 */
class CanNm_MainFunction_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeCanHw_Reset();
        WrapCanIf_Reset();  // 他ファイルの故障注入が漏れ伝わらないよう防御的にリセット
        WrapCan_Reset();  // 同上（Can.c 側）
        WrapDem_Reset();
        Dem_Init(NULL);  // Demの内部状態を毎テスト決定的にリセットする（Fake_NvM.cにより常に「初回起動」）
        FakeEcuM_Reset();
        WrapBswM_Reset();
        FakeMillis_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制

        canConfig.filter.filterId = 0x0220U;
        canConfig.filter.mask     = 0x1FFFU;
        canConfig.csPin           = 10U;
        canConfig.intPin          = 2U;
        canConfig.baudrate        = 500000U;
        canConfig.crystalFreq     = CAN_CRYSTAL_16MHZ;

        Can_Init(&canConfig);
        CanIf_Init(&kCommControlTimeoutCanIfConfig);
        CanSM_Init(NULL);
        ComM_Init(NULL);
        ComM_CommunicationAllowed(COMM_CHANNEL_0, TRUE);  // 実 EcuM_Init() と同じく起動時に許可
        CanNm_Init(NULL);
        Nm_Init(NULL);

        // NORMAL_OPERATION State まで進める(NmStack_SleepCoordination_test.cpp の
        // ArrangeFullCom() と同じ流儀)。
        ASSERT_EQ(ComM_RequestComMode(COMM_USER_0, COMM_FULL_COMMUNICATION), E_OK);
        FakeMillis_Value += CANNM_REPEAT_MESSAGE_MS + 100UL;
        CanNm_MainFunction();
        CanNm_StateType state;
        CanNm_ModeType  mode;
        ASSERT_EQ(CanNm_GetState(CANNM_MAIN_NETWORK_HANDLE, &state, &mode), E_OK);
        ASSERT_EQ(state, CANNM_STATE_NORMAL_OPERATION);

        FakeDetHw_Reset();              // ここまでの DET 記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;   // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        CanNm_DeInit();
        ComM_DeInit();
        CanSM_DeInit();
        CanIf_DeInit();
    }

    static bool NetworkTimeoutReported()
    {
        return FakeDetHw_ReportCount > 0U && FakeDetHw_LastModuleId == CANNM_MODULE_ID
               && FakeDetHw_LastApiId == CANNM_API_ID_MAIN_FUNCTION && FakeDetHw_LastErrorId == CANNM_E_NETWORK_TIMEOUT;
    }

    Can_ConfigType canConfig;
};

/* CanNm_MainFunction() の未初期化チェック の単体テスト用フィクスチャ。
 * SetUp(): ComM_DeInit()（RxIndication のカスケードを防ぐ）の後、CanNm_Init(NULL) と Nm_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): CanNm_DeInit() で未初期化へ戻す。 */
class CanNm_MainFunctionUninit_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        /* CanNm_RxIndication() -> ComM_Nm_NetworkStartIndication() の呼び出し
         * （[SWS_CanNm_00127]）は ComM が初期化済みだとカスケードする。
         * 本フィクスチャは意図的に ComM_Init() を呼ばない軽量構成のため、他の
         * テストファイルが ComM を初期化したまま残す可能性
         * （feedback_native_chain_shared_static_hang 参照）を排除するべく、
         * 明示的に ComM_DeInit() で未初期化状態を保証する
         * （未初期化なら DET 報告のみで無害に即 return）。 */
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
    }

    static const NetworkHandleType kInvalidChannel = CANNM_MAIN_NETWORK_HANDLE + 1U;
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

// ------------------------------------------------------------
// 2026-09 是正の本題: 送信無効化中は NM-Timeout Timer の満了判定自体を
// 止めるため、いくら時間が経っても CANNM_E_NETWORK_TIMEOUT は報告されない。
// ------------------------------------------------------------
TEST_F(CanNm_MainFunction_Test, MainFunction_NG_TimeoutFiresNormallyWhenEnabled)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FakeMillis_Value += CANNM_TIMEOUT_MS + 1UL;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanNm_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_TRUE(NetworkTimeoutReported());
}

TEST_F(CanNm_MainFunctionUninit_Test, CanNm_MainFunction_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanNm_DeInit();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanNm_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_UNINIT);
}

TEST_F(CanNm_MainFunction_Test, MainFunction_NG_TimeoutFiresInRepeatMessageWhenEnabled)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // 送信有効のまま Repeat Message State でタイムアウトを超える
    CanNm_Test_SetState(CANNM_STATE_REPEAT_MESSAGE);
    FakeMillis_Value += CANNM_TIMEOUT_MS + 1UL;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanNm_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_TRUE(NetworkTimeoutReported());
}

TEST_F(CanNm_MainFunction_Test, MainFunction_OK_TimeoutDoesNotFireWhileDisabled)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(CanNm_DisableCommunication(CANNM_MAIN_NETWORK_HANDLE), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    for (uint8 i = 0U; i < 5U; i++)
    {
        FakeMillis_Value += CANNM_TIMEOUT_MS + 1UL;
        CanNm_MainFunction();
    }

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_FALSE(NetworkTimeoutReported());
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);

    CanNm_StateType state;
    CanNm_ModeType  mode;
    ASSERT_EQ(CanNm_GetState(CANNM_MAIN_NETWORK_HANDLE, &state, &mode), E_OK);
    EXPECT_EQ(state, CANNM_STATE_NORMAL_OPERATION);  // 状態機械自体は無関係に維持される
}

// ------------------------------------------------------------
// 再有効化時にタイマーが再起動されること(スタックした古い基準時刻のまま
// 残っていれば、再有効化直後の1周期で即座に見かけ上の満了が起きるはず)。
// ------------------------------------------------------------

TEST_F(CanNm_MainFunction_Test, MainFunction_OK_TimeoutRestartsOnReEnable)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(CanNm_DisableCommunication(CANNM_MAIN_NETWORK_HANDLE), E_OK);
    FakeMillis_Value += 3UL * CANNM_TIMEOUT_MS;  // 無効化中に古い基準時刻を大きく経過させる

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanNm_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_FALSE(NetworkTimeoutReported());  // 無効化中は満了しない(前テストと同じ)

    ASSERT_EQ(CanNm_EnableCommunication(CANNM_MAIN_NETWORK_HANDLE), E_OK);

    /* 再起動されていれば、CANNM_TIMEOUT_MS未満の経過ではまだ満了しない
     * (再起動されず古い基準時刻のままなら、この時点で経過時間は既に
     * 3*CANNM_TIMEOUT_MS を超えており即座に満了してしまうはず)。 */
    FakeMillis_Value += CANNM_TIMEOUT_MS - 100UL;
    CanNm_MainFunction();
    EXPECT_FALSE(NetworkTimeoutReported());

    /* 再起動後の基準時刻から改めて CANNM_TIMEOUT_MS 経過すれば、通常どおり満了する
     * (タイマーが再有効化後も引き続き正常に機能していることの確認)。 */
    FakeMillis_Value += 200UL;
    CanNm_MainFunction();
    EXPECT_TRUE(NetworkTimeoutReported());
}

// ------------------------------------------------------------
// 自己/simplify(altitude観点)の指摘: 修正が及ぶ3状態のうち
// NORMAL_OPERATION でしか検証していなかったため、REPEAT_MESSAGE/
// READY_SLEEP でも同様にゲートが効くことを直接確認する。
// ------------------------------------------------------------

TEST_F(CanNm_MainFunction_Test, MainFunction_OK_TimeoutDoesNotFireWhileDisabledInRepeatMessage)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(CanNm_RepeatMessageRequest(CANNM_MAIN_NETWORK_HANDLE), E_OK);  // NORMAL_OPERATION -> REPEAT_MESSAGE
    CanNm_StateType state;
    CanNm_ModeType  mode;
    ASSERT_EQ(CanNm_GetState(CANNM_MAIN_NETWORK_HANDLE, &state, &mode), E_OK);
    ASSERT_EQ(state, CANNM_STATE_REPEAT_MESSAGE);
    FakeDetHw_Reset();

    ASSERT_EQ(CanNm_DisableCommunication(CANNM_MAIN_NETWORK_HANDLE), E_OK);
    /* CANNM_TIMEOUT_MS(3000ms) は CANNM_REPEAT_MESSAGE_MS(1500ms) より長いため、
     * この経過で Repeat Message State 自体の継続時間(CanNm_StateTimerMs、
     * 送信無効化とは無関係に無条件で進む)も同時に超過し、この1回の
     * CanNm_MainFunction() 呼び出し内で NORMAL_OPERATION へ自動遷移する
     * （[SWS_CanNm_00170] の Note どおり、Communication Control は Repeat
     * Message State の「長さ」自体には影響しない）。ここで検証したいのは
     * その遷移とは独立な「NM-Timeout Timer 満了判定だけは無効化中スキップ
     * された」という点のみ。 */
    FakeMillis_Value += CANNM_TIMEOUT_MS + 1UL;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanNm_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_FALSE(NetworkTimeoutReported());
}

TEST_F(CanNm_MainFunction_Test, MainFunction_OK_DoesNotEnterPrepareBusSleepWhileDisabledInReadySleep)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(CanNm_NetworkRelease(CANNM_MAIN_NETWORK_HANDLE), E_OK);  // NORMAL_OPERATION -> READY_SLEEP
    CanNm_StateType state;
    CanNm_ModeType  mode;
    ASSERT_EQ(CanNm_GetState(CANNM_MAIN_NETWORK_HANDLE, &state, &mode), E_OK);
    ASSERT_EQ(state, CANNM_STATE_READY_SLEEP);

    ASSERT_EQ(CanNm_DisableCommunication(CANNM_MAIN_NETWORK_HANDLE), E_OK);
    FakeMillis_Value += CANNM_TIMEOUT_MS + 1UL;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanNm_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    /* READY_SLEEP の満了アクションは DET 報告ではなく
     * CanNm_EnterPrepareBusSleep() への遷移([SWS_CanNm_00109])。無効化中は
     * これも起きないはず。 */
    ASSERT_EQ(CanNm_GetState(CANNM_MAIN_NETWORK_HANDLE, &state, &mode), E_OK);
    EXPECT_EQ(state, CANNM_STATE_READY_SLEEP);
}

// ------------------------------------------------------------
// 制御群: 送信有効のまま(既定)放置すれば、通常どおり NM-Timeout Timer は
// 満了して CANNM_E_NETWORK_TIMEOUT が報告される(是正前から変わらない挙動)。
// ------------------------------------------------------------

TEST_F(CanNm_MainFunction_Test, MainFunction_OK_BusSleepStateDoesNothing)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanNm_Test_SetState(CANNM_STATE_BUS_SLEEP);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    FakeMillis_Value += CANNM_TIMEOUT_MS + CANNM_REPEAT_MESSAGE_MS;
    CanNm_MainFunction();
    CanNm_StateType state = CANNM_STATE_NORMAL_OPERATION;
    CanNm_ModeType  mode  = CANNM_MODE_NETWORK;
    const Std_ReturnType ret = CanNm_GetState(CANNM_MAIN_NETWORK_HANDLE, &state, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_EQ(ret, E_OK);
    EXPECT_EQ(state, CANNM_STATE_BUS_SLEEP);   // 時間が経っても、状態は変わらない
}

TEST_F(CanNm_MainFunction_Test, MainFunction_OK_RepeatMessageEndsInReadySleepWhenNetworkIsNotRequested)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(CanNm_NetworkRelease(CANNM_MAIN_NETWORK_HANDLE), E_OK);   // 通信の要求を取り下げる
    CanNm_Test_SetState(CANNM_STATE_REPEAT_MESSAGE);
    FakeMillis_Value += CANNM_REPEAT_MESSAGE_MS;   // 繰り返し送信の期間が経過する

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanNm_MainFunction();
    CanNm_StateType state = CANNM_STATE_NORMAL_OPERATION;
    CanNm_ModeType  mode  = CANNM_MODE_NETWORK;
    const Std_ReturnType ret = CanNm_GetState(CANNM_MAIN_NETWORK_HANDLE, &state, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_EQ(ret, E_OK);
    EXPECT_EQ(state, CANNM_STATE_READY_SLEEP);   // 要求が無いので、通常運転ではなく READY_SLEEP へ
}

TEST_F(CanNm_MainFunction_Test, MainFunction_OK_UnknownStateIsIgnored)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanNm_Test_SetState(static_cast<CanNm_StateType>(0x7F));   // 定義にない値（破損を想定）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanNm_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);   // 何もしない（DET も報告しない）
}

}  // namespace
