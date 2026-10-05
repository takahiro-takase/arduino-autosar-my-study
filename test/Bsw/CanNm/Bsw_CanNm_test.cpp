/**
 * \file    Bsw_CanNm_test.cpp
 * \brief   CanNm.c（src/Bsw/CanNm/CanNm.c）の単体テスト（GoogleTest / PlatformIO
 *          `[env:native_chain]`）。
 *
 * \details 2026-09、モジュール単位のテストファイルを1モジュール1ファイルへ
 *          集約する方針のもと、`Bsw_CanNm_ChannelValidation_test.cpp` /
 *          `Bsw_CanNm_CommunicationControlTimeout_test.cpp` / 旧
 *          `Bsw_CanNm_test.cpp`（NG系のみ）の3ファイルを本ファイルへ統合した。
 *          各セクションの経緯は元ファイルのコメントをそのまま引き継ぐ。
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

// ============================================================================
// CANNM_E_INVALID_CHANNEL / GetLocalNodeIdentifier / GetNodeIdentifier
// （旧 Bsw_CanNm_ChannelValidation_test.cpp）
// ============================================================================

/**
 * \details 2026-08-30、IF シグネチャは仕様準拠という方針のもと
 *          CanNm_NetworkRequest/NetworkRelease/RepeatMessageRequest/GetState に
 *          NetworkHandleType Channel 引数を追加し、CanSM の
 *          CANSM_E_INVALID_NETWORK_HANDLE と平仄を合わせて
 *          CANNM_E_INVALID_CHANNEL（[SWS_CanNm_00192]）による範囲チェックを
 *          追加した際に新設。/code-review で「新設した検証パスに対する
 *          テストが無い」と指摘され追加した。
 *          CanNm.c 単体（Can/CanIf/CanSM/ComM は不要）で検証できるため、
 *          Bsw_NmStack_SleepCoordination_test.cpp より軽量なフィクスチャで足りる。
 */

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

class Bsw_CanNm_ChannelValidation_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        /* 2026-09 追加の CanNm_RxIndication() -> ComM_Nm_NetworkStartIndication()
         * 呼び出し（[SWS_CanNm_00127]）は ComM が初期化済みだとカスケードする。
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

TEST_F(Bsw_CanNm_ChannelValidation_Test, NetworkRequest_NG_InvalidChannelReturnsErrorAndReportsDet)
{
    Std_ReturnType ret = CanNm_NetworkRequest(kInvalidChannel);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_INVALID_CHANNEL);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);
}

/* NetworkRequest の「有効な Channel」正常系は、CanNm_EnterRepeatMessage() 経由で
 * ComM_Nm_NetworkMode() へカスケードする（本ファイルは CanNm.c 単体の検証が
 * 目的のため ComM_Init() を呼ばない軽量フィクスチャであり、意図的に
 * ComM 側は未初期化のまま。カスケード後の挙動検証は
 * Bsw_NmStack_SleepCoordination_test.cpp の責務）。そのため本ファイルでは
 * NG（Channel 不正時に即座に拒否される）側のみを検証する。 */

TEST_F(Bsw_CanNm_ChannelValidation_Test, NetworkRelease_NG_InvalidChannelReturnsErrorAndReportsDet)
{
    Std_ReturnType ret = CanNm_NetworkRelease(kInvalidChannel);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_INVALID_CHANNEL);
}

TEST_F(Bsw_CanNm_ChannelValidation_Test, RepeatMessageRequest_NG_InvalidChannelReturnsErrorAndReportsDet)
{
    Std_ReturnType ret = CanNm_RepeatMessageRequest(kInvalidChannel);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_INVALID_CHANNEL);
}

TEST_F(Bsw_CanNm_ChannelValidation_Test, GetState_OK_ValidChannelIsAccepted)
{
    CanNm_StateType state;
    CanNm_ModeType  mode;

    Std_ReturnType ret = CanNm_GetState(CANNM_MAIN_NETWORK_HANDLE, &state, &mode);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(Bsw_CanNm_ChannelValidation_Test, GetState_NG_InvalidChannelReturnsErrorAndReportsDet)
{
    CanNm_StateType state;
    CanNm_ModeType  mode;

    Std_ReturnType ret = CanNm_GetState(kInvalidChannel, &state, &mode);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_INVALID_CHANNEL);
}

TEST_F(Bsw_CanNm_ChannelValidation_Test, DisableCommunication_OK_ValidChannelIsAccepted)
{
    Std_ReturnType ret = CanNm_DisableCommunication(CANNM_MAIN_NETWORK_HANDLE);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(Bsw_CanNm_ChannelValidation_Test, DisableCommunication_NG_InvalidChannelReturnsErrorAndReportsDet)
{
    Std_ReturnType ret = CanNm_DisableCommunication(kInvalidChannel);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_INVALID_CHANNEL);
}

TEST_F(Bsw_CanNm_ChannelValidation_Test, EnableCommunication_OK_ValidChannelIsAccepted)
{
    Std_ReturnType ret = CanNm_EnableCommunication(CANNM_MAIN_NETWORK_HANDLE);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(Bsw_CanNm_ChannelValidation_Test, EnableCommunication_NG_InvalidChannelReturnsErrorAndReportsDet)
{
    Std_ReturnType ret = CanNm_EnableCommunication(kInvalidChannel);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_INVALID_CHANNEL);
}

// ------------------------------------------------------------
// CanNm_GetLocalNodeIdentifier()/CanNm_GetNodeIdentifier()
// （[SWS_CanNm_00220]/[SWS_CanNm_00219] 準拠で新設）
// ------------------------------------------------------------

TEST_F(Bsw_CanNm_ChannelValidation_Test, GetLocalNodeIdentifier_OK_ReturnsConfiguredSourceNodeId)
{
    uint8 nodeId = 0U;

    Std_ReturnType ret = CanNm_GetLocalNodeIdentifier(CANNM_MAIN_NETWORK_HANDLE, &nodeId);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(nodeId, CANNM_SOURCE_NODE_ID);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(Bsw_CanNm_ChannelValidation_Test, GetLocalNodeIdentifier_NG_InvalidChannelReturnsErrorAndReportsDet)
{
    uint8 nodeId = 0U;

    Std_ReturnType ret = CanNm_GetLocalNodeIdentifier(kInvalidChannel, &nodeId);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_INVALID_CHANNEL);
}

TEST_F(Bsw_CanNm_ChannelValidation_Test, GetLocalNodeIdentifier_NG_NullPointerReturnsErrorAndReportsDet)
{
    Std_ReturnType ret = CanNm_GetLocalNodeIdentifier(CANNM_MAIN_NETWORK_HANDLE, NULL);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_PARAM_POINTER);
}

TEST_F(Bsw_CanNm_ChannelValidation_Test, GetNodeIdentifier_OK_ReturnsZeroBeforeAnyReception)
{
    uint8 nodeId = 0xFFU;

    Std_ReturnType ret = CanNm_GetNodeIdentifier(CANNM_MAIN_NETWORK_HANDLE, &nodeId);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(nodeId, 0U);
}

TEST_F(Bsw_CanNm_ChannelValidation_Test, GetNodeIdentifier_OK_ReflectsMostRecentlyReceivedFrame)
{
    /* 2026-09 追加の ComM_Nm_NetworkStartIndication() 呼び出し
     * （[SWS_CanNm_00127]）により、Bus-Sleep Mode 中の受信は ComM が
     * 初期化済みだとカスケードしうる（CanSM_RequestComMode()/
     * CanNm_NetworkRequest() 経由）が、SetUp() の ComM_DeInit() により本テスト
     * では ComM は必ず未初期化（＝カスケードせず COMM_E_UNINIT の DET 報告
     * のみで即 return）。カスケード時の挙動検証自体は
     * Bsw_NmStack_SleepCoordination_test.cpp の責務。 */
    uint8 pdu[2] = { 0x00U, 0x2AU };  // CBV=0, sourceNodeId=0x2A
    PduInfoType pduInfo = { pdu, 2U };
    CanNm_RxIndication(0U, &pduInfo);

    uint8 nodeId = 0U;
    Std_ReturnType ret = CanNm_GetNodeIdentifier(CANNM_MAIN_NETWORK_HANDLE, &nodeId);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(nodeId, 0x2AU);
}

TEST_F(Bsw_CanNm_ChannelValidation_Test, GetNodeIdentifier_NG_InvalidChannelReturnsErrorAndReportsDet)
{
    uint8 nodeId = 0U;

    Std_ReturnType ret = CanNm_GetNodeIdentifier(kInvalidChannel, &nodeId);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_INVALID_CHANNEL);
}

TEST_F(Bsw_CanNm_ChannelValidation_Test, GetNodeIdentifier_NG_NullPointerReturnsErrorAndReportsDet)
{
    Std_ReturnType ret = CanNm_GetNodeIdentifier(CANNM_MAIN_NETWORK_HANDLE, NULL);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_PARAM_POINTER);
}

// ============================================================================
// 診断 CommunicationControl(UDS 0x28) 中の NM-Timeout Timer 停止/再起動
// （旧 Bsw_CanNm_CommunicationControlTimeout_test.cpp）
// ============================================================================

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
 *          `Bsw_NmStack_SleepCoordination_test.cpp` の `kTestCanIfConfigWithNmTx` と
 *          同じパターン）を使う。TxPduCount=0 の空設定だと、CanNm の周期送信
 *          （`CanNm_TransmitPdu()` → `CanIf_Transmit()`）のたびに
 *          `CANIF_E_INVALID_TXPDUID` が DET 報告されてしまい、
 *          `CANNM_E_NETWORK_TIMEOUT` 報告の有無を検証したい DET スパイの
 *          `FakeDetHw_Last*`（直近1件のみ記録）にノイズが混ざるため。
 */

/* CanNm の周期送信(CANNM_CANIF_TX_PDU_ID)が CanIf 層で CANIF_E_INVALID_TXPDUID を
 * 報告してしまうと、CANNM_E_NETWORK_TIMEOUT の DET 報告有無の検証にノイズが
 * 混ざる(Bsw_NmStack_SleepCoordination_test.cpp の kTestCanIfConfigWithNmTx と同じ
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

class Bsw_CanNm_CommunicationControlTimeout_Test : public ::testing::Test
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

        // NORMAL_OPERATION State まで進める(Bsw_NmStack_SleepCoordination_test.cpp の
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

// ------------------------------------------------------------
// 2026-09 是正の本題: 送信無効化中は NM-Timeout Timer の満了判定自体を
// 止めるため、いくら時間が経っても CANNM_E_NETWORK_TIMEOUT は報告されない。
// ------------------------------------------------------------
TEST_F(Bsw_CanNm_CommunicationControlTimeout_Test, MainFunction_OK_TimeoutDoesNotFireWhileDisabled)
{
    ASSERT_EQ(CanNm_DisableCommunication(CANNM_MAIN_NETWORK_HANDLE), E_OK);

    for (uint8 i = 0U; i < 5U; i++)
    {
        FakeMillis_Value += CANNM_TIMEOUT_MS + 1UL;
        CanNm_MainFunction();
    }

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
TEST_F(Bsw_CanNm_CommunicationControlTimeout_Test, MainFunction_OK_TimeoutRestartsOnReEnable)
{
    ASSERT_EQ(CanNm_DisableCommunication(CANNM_MAIN_NETWORK_HANDLE), E_OK);
    FakeMillis_Value += 3UL * CANNM_TIMEOUT_MS;  // 無効化中に古い基準時刻を大きく経過させる
    CanNm_MainFunction();
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
TEST_F(Bsw_CanNm_CommunicationControlTimeout_Test, MainFunction_OK_TimeoutDoesNotFireWhileDisabledInRepeatMessage)
{
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
    CanNm_MainFunction();

    EXPECT_FALSE(NetworkTimeoutReported());
}

TEST_F(Bsw_CanNm_CommunicationControlTimeout_Test, MainFunction_OK_DoesNotEnterPrepareBusSleepWhileDisabledInReadySleep)
{
    ASSERT_EQ(CanNm_NetworkRelease(CANNM_MAIN_NETWORK_HANDLE), E_OK);  // NORMAL_OPERATION -> READY_SLEEP
    CanNm_StateType state;
    CanNm_ModeType  mode;
    ASSERT_EQ(CanNm_GetState(CANNM_MAIN_NETWORK_HANDLE, &state, &mode), E_OK);
    ASSERT_EQ(state, CANNM_STATE_READY_SLEEP);

    ASSERT_EQ(CanNm_DisableCommunication(CANNM_MAIN_NETWORK_HANDLE), E_OK);
    FakeMillis_Value += CANNM_TIMEOUT_MS + 1UL;
    CanNm_MainFunction();

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
TEST_F(Bsw_CanNm_CommunicationControlTimeout_Test, MainFunction_NG_TimeoutFiresNormallyWhenEnabled)
{
    FakeMillis_Value += CANNM_TIMEOUT_MS + 1UL;

    CanNm_MainFunction();

    EXPECT_TRUE(NetworkTimeoutReported());
}

// ============================================================================
// その他公開APIの NG系（旧 Bsw_CanNm_test.cpp、Uninit系中心）
// ============================================================================

/**
 * \details 上記2セクションが未カバーだった `Det_ReportError()` 呼び出し箇所
 *          （各公開APIの CANNM_E_UNINIT、CanNm_GetVersionInfo の
 *          CANNM_E_PARAM_POINTER、CanNm_RxIndication の CANNM_E_UNINIT/
 *          CANNM_E_PARAM_POINTER）をまとめる。各ケースで、報告される ErrorId
 *          が仕様どおり正しい値になっていることを `Fake_Det_Hw.h`
 *          （`FakeDetHw_LastErrorId`）で検証する。
 *
 *          CanNm には `CanNm_DeInit()` が存在するため、Mcu/PduR/CanTp と異なり
 *          「未初期化状態」の検証にテスト専用のリセット関数は不要（各テストが
 *          明示的に `CanNm_DeInit()` を呼んでから検証する）。
 *
 *          上記セクションと同じ理由で、`CanNm_RxIndication()` の
 *          `ComM_Nm_NetworkStartIndication()` へのカスケード
 *          （[SWS_CanNm_00127]）を避けるため ComM は明示的に未初期化のまま
 *          にする（`feedback_native_chain_shared_static_hang` 参照）。
 */

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

class Bsw_CanNm_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        ComM_DeInit();                  // RxIndication のカスケード防止（ファイル冒頭コメント参照）
        CanNm_Init(NULL);
        FakeDetHw_Reset();               // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;   // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        CanNm_DeInit();
    }
};

// ------------------------------------------------------------
// CanNm_DeInit()
// ------------------------------------------------------------

TEST_F(Bsw_CanNm_Test, CanNm_DeInit_NG_Uninit)
{
    CanNm_DeInit();  // 1回目: SetUp() の Init を正常に解除する
    FakeDetHw_Reset();

    CanNm_DeInit();  // 2回目: 既に未初期化のため NG

    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_UNINIT);
}

// ------------------------------------------------------------
// CanNm_NetworkRequest()/CanNm_NetworkRelease()/
// CanNm_DisableCommunication()/CanNm_EnableCommunication()/
// CanNm_RepeatMessageRequest()/CanNm_GetState()（いずれも Uninit チェックのみ）
// ------------------------------------------------------------

TEST_F(Bsw_CanNm_Test, CanNm_NetworkRequest_NG_Uninit)
{
    CanNm_DeInit();

    Std_ReturnType ret = CanNm_NetworkRequest(CANNM_MAIN_NETWORK_HANDLE);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_UNINIT);
}

TEST_F(Bsw_CanNm_Test, CanNm_NetworkRelease_NG_Uninit)
{
    CanNm_DeInit();

    Std_ReturnType ret = CanNm_NetworkRelease(CANNM_MAIN_NETWORK_HANDLE);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_UNINIT);
}

TEST_F(Bsw_CanNm_Test, CanNm_DisableCommunication_NG_Uninit)
{
    CanNm_DeInit();

    Std_ReturnType ret = CanNm_DisableCommunication(CANNM_MAIN_NETWORK_HANDLE);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_UNINIT);
}

TEST_F(Bsw_CanNm_Test, CanNm_EnableCommunication_NG_Uninit)
{
    CanNm_DeInit();

    Std_ReturnType ret = CanNm_EnableCommunication(CANNM_MAIN_NETWORK_HANDLE);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_UNINIT);
}

TEST_F(Bsw_CanNm_Test, CanNm_RepeatMessageRequest_NG_Uninit)
{
    CanNm_DeInit();

    Std_ReturnType ret = CanNm_RepeatMessageRequest(CANNM_MAIN_NETWORK_HANDLE);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_UNINIT);
}

TEST_F(Bsw_CanNm_Test, CanNm_GetState_NG_Uninit)
{
    CanNm_DeInit();
    CanNm_StateType state;
    CanNm_ModeType  mode;

    Std_ReturnType ret = CanNm_GetState(CANNM_MAIN_NETWORK_HANDLE, &state, &mode);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_UNINIT);
}

// ------------------------------------------------------------
// CanNm_GetNodeIdentifier()/CanNm_GetLocalNodeIdentifier()
// （Uninit チェックのみ、Channel/NULL は上記セクションでカバー済み）
// ------------------------------------------------------------

TEST_F(Bsw_CanNm_Test, CanNm_GetNodeIdentifier_NG_Uninit)
{
    CanNm_DeInit();
    uint8 nodeId = 0U;

    Std_ReturnType ret = CanNm_GetNodeIdentifier(CANNM_MAIN_NETWORK_HANDLE, &nodeId);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_UNINIT);
}

TEST_F(Bsw_CanNm_Test, CanNm_GetLocalNodeIdentifier_NG_Uninit)
{
    CanNm_DeInit();
    uint8 nodeId = 0U;

    Std_ReturnType ret = CanNm_GetLocalNodeIdentifier(CANNM_MAIN_NETWORK_HANDLE, &nodeId);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_UNINIT);
}

// ------------------------------------------------------------
// CanNm_GetVersionInfo()
// ------------------------------------------------------------

TEST_F(Bsw_CanNm_Test, CanNm_GetVersionInfo_NG_NullPointer)
{
    CanNm_GetVersionInfo(NULL);

    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// CanNm_RxIndication()
// ------------------------------------------------------------

TEST_F(Bsw_CanNm_Test, CanNm_RxIndication_NG_Uninit)
{
    CanNm_DeInit();
    uint8 pdu[2] = { 0x00U, 0x2AU };
    PduInfoType info = { pdu, 2U };

    CanNm_RxIndication(0U, &info);

    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_UNINIT);
}

TEST_F(Bsw_CanNm_Test, CanNm_RxIndication_NG_NullPduInfoPtr)
{
    CanNm_RxIndication(0U, NULL);

    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// CanNm_MainFunction()
// ------------------------------------------------------------

TEST_F(Bsw_CanNm_Test, CanNm_MainFunction_NG_Uninit)
{
    CanNm_DeInit();

    CanNm_MainFunction();

    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_UNINIT);
}

}  // namespace
