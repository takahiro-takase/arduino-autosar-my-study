/**
 * \file    Bsw_ComStack_SignalGroup_Rx_test.cpp
 * \brief   README.md「Rx 処理」および「デッドライン監視（受信タイムアウト）」
 *          コールチェーンのうち、Signal Group（IsSignalGroup=1）関連・
 *          I-PDU Group 制御（Com_IpduGroupStart/Stop、Com_Enable/
 *          DisableReceptionDM）関連の受信シナリオ専用の単体テスト
 *          （GoogleTest / CMake native_chain_tests）。
 *
 * \details 2026-09、`Bsw_ComStack_Rx_test.cpp`/`Bsw_ComStack_RxTimeout_test.cpp`
 *          のうち Signal Group 関連分と、`rx_ipdu_group` 名前空間（I-PDU
 *          Group 制御の回帰テスト一式）を、Msg内容（Signal/SignalGroup/
 *          E2E/SecOC）× 方向（Tx/Rx）の `Bsw_ComStack_{MsgType}_{Tx|Rx}_
 *          test.cpp` 命名規則へ統合した（ユーザー指示）。非 Signal Group分は
 *          `Bsw_ComStack_Signal_Rx_test.cpp` へ移した。
 *
 *          いずれも Com_RxIndication()/Com_MainFunctionRx() を直接呼ぶのみで
 *          PduR/CanIf/Can/CanSM は経由しないため、フェイクは millis()
 *          （stub/Hal/Fake_Millis.c）のみで足りる。
 *
 *          `rx_ipdu_group` 名前空間の一部テストは非 Signal Group の
 *          IPduId（EngineInfo 相当）も併せて検証しているが、これは
 *          「Com_IpduGroupStart/Stop 等の I-PDU Group 制御が、メンバーの
 *          IsSignalGroup の値によらず一律に効くこと」自体が主張の核心のため、
 *          Signal Group 制御の回帰一式として本ファイルにまとめて置いている。
 */
#include <gtest/gtest.h>

extern "C" {
#include "Com.h"
#include "Fake_Millis.h"
#include "Fake_Det_Hw.h"
}

namespace
{

// -----------------------------------------------------------------------
// SWS_Com_00555 の Signal Group 側経路（Com_RxIndication() 内、シグナル単位
// デッドライン監視リセットループへ入る前のグループ単位分岐）用。メンバーが
// 2本でもグループ単位で1回だけ呼ばれることを検証する。
// -----------------------------------------------------------------------
static uint8_t s_groupRxAckCount = 0U;
static void TestGroupRxAckCbk(void) { s_groupRxAckCount++; }

const Com_SignalConfigType kTestRxGroupSignalA = {
    /* SignalId */                1U,
    /* Direction */                COM_SIGNAL_DIRECTION_RX,
    /* IPduId */                   1U,
    /* BitPosition */              0U,
    /* BitSize */                  1U,
    /* Endian */                   COM_BIG_ENDIAN,
    /* InitValue */                0U,
    /* FilterAlgorithm */          COM_FILTER_ALWAYS,
    /* Mask */                     0U,
    /* FilterX */                  0U,
    /* FilterMin */                0U,
    /* FilterMax */                0U,
    /* FilterRejectCbk */          NULL,
    /* TmsContributor */           0U,
    /* UpdateBitContributor */     0U,
    /* TransferProperty */         COM_TRANSFER_PROPERTY_PENDING,
    /* RxDataTimeoutAction */      COM_RX_TIMEOUT_ACTION_NONE,
    /* TimeoutSubstitutionValue */ 0U,
    /* DataInvalidAction */        COM_DATA_INVALID_ACTION_NONE,
    /* InvalidValue */             0U,
    /* InvalidNotificationCbk */   NULL,
    /* FirstTimeoutMs */           0U,
    /* TimeoutMs */                0U,
    /* RxTOutCbk */                NULL,
    /* TxAckCbk */                 NULL,
    /* TxErrCbk */                 NULL,
    /* RxAckCbk */                 NULL  // Signal Group メンバーには設定しない（呼ばれないことの裏付け）
};

const Com_SignalConfigType kTestRxGroupSignalB = {
    /* SignalId */                2U,
    /* Direction */                COM_SIGNAL_DIRECTION_RX,
    /* IPduId */                   1U,
    /* BitPosition */              1U,
    /* BitSize */                  1U,
    /* Endian */                   COM_BIG_ENDIAN,
    /* InitValue */                0U,
    /* FilterAlgorithm */          COM_FILTER_ALWAYS,
    /* Mask */                     0U,
    /* FilterX */                  0U,
    /* FilterMin */                0U,
    /* FilterMax */                0U,
    /* FilterRejectCbk */          NULL,
    /* TmsContributor */           0U,
    /* UpdateBitContributor */     0U,
    /* TransferProperty */         COM_TRANSFER_PROPERTY_PENDING,
    /* RxDataTimeoutAction */      COM_RX_TIMEOUT_ACTION_NONE,
    /* TimeoutSubstitutionValue */ 0U,
    /* DataInvalidAction */        COM_DATA_INVALID_ACTION_NONE,
    /* InvalidValue */             0U,
    /* InvalidNotificationCbk */   NULL,
    /* FirstTimeoutMs */           0U,
    /* TimeoutMs */                0U,
    /* RxTOutCbk */                NULL,
    /* TxAckCbk */                 NULL,
    /* TxErrCbk */                 NULL,
    /* RxAckCbk */                 NULL
};

const Com_IPduConfigType kTestRxGroupIPdu = {
    /* IPduId */           1U,
    /* DLC */              1U,
    /* PduRId */           1U,  // 本テストは Com_MainFunctionRx()/Tx()/CanIf/PduR まで進めないため未使用
    /* FirstTimeoutMs */   0U,
    /* TimeoutMs */        0U,
    /* IsSignalGroup */    1U,
    /* TxModeMode */       COM_TX_MODE_DIRECT,  /* RX I-PDU では未使用 */
    /* TxPeriodMs */       0U,
    /* TxModeModeTrue */   COM_TX_MODE_DIRECT,
    /* TxPeriodMsTrue */   0U,
    /* MinDelayMs */       0U,
    /* UpdateBitPosition */ 0xFFU,
    /* IpduGroupId */      COM_IPDU_GROUP_NONE,
    /* RxIndicationCbk */  NULL,
    /* TxTransformCbk */   NULL,
    /* TxAckCbk */         NULL,
    /* TxErrCbk */         NULL,
    /* RxAckCbk */         TestGroupRxAckCbk
};

// SWS_Com_00536/00556（Com_CbkRxTOut）検証用カウンタ・コールバック。
static uint8_t s_groupRxTOutCount = 0U;
static void TestGroupRxTOutCbk(void) { s_groupRxTOutCount++; }

// /code-review で指摘された「Signal Group メンバーの除外は設定規約のみに
// 依存し、コードで担保していない」の回帰テスト用。誤って非 Signal Group
// シグナルのようにコピペ設定してしまった場合を模したカウンタ。
static uint8_t s_misconfiguredGroupMemberRxTOutCount = 0U;
static void TestMisconfiguredGroupMemberRxTOutCbk(void) { s_misconfiguredGroupMemberRxTOutCount++; }

// IPduId=2: グループ単位 Com_CbkRxTOut 検証用（IsSignalGroup=1、メンバー1本）。
const Com_SignalConfigType kTestRxTimeoutGroupSignal = {
    /* SignalId */                3U,
    /* Direction */                COM_SIGNAL_DIRECTION_RX,
    /* IPduId */                   2U,
    /* BitPosition */              0U,
    /* BitSize */                  1U,
    /* Endian */                   COM_BIG_ENDIAN,
    /* InitValue */                0U,
    /* FilterAlgorithm */          COM_FILTER_ALWAYS,
    /* Mask */                     0U,
    /* FilterX */                  0U,
    /* FilterMin */                0U,
    /* FilterMax */                0U,
    /* FilterRejectCbk */          NULL,
    /* TmsContributor */           0U,
    /* UpdateBitContributor */     0U,
    /* TransferProperty */         COM_TRANSFER_PROPERTY_PENDING,
    /* RxDataTimeoutAction */      COM_RX_TIMEOUT_ACTION_NONE,
    /* TimeoutSubstitutionValue */ 0U,
    /* DataInvalidAction */        COM_DATA_INVALID_ACTION_NONE,
    /* InvalidValue */             0U,
    /* InvalidNotificationCbk */   NULL,
    /* FirstTimeoutMs */           500U,  /* Signal Group メンバーへの誤設定を意図的に再現
                                            * （本来は未使用=0 にすべき値）。 */
    /* TimeoutMs */                500U,
    /* RxTOutCbk */                TestMisconfiguredGroupMemberRxTOutCbk,
    /* TxAckCbk */                 NULL,
    /* TxErrCbk */                 NULL
};

const Com_IPduConfigType kTestRxTimeoutGroupIPdu = {
    /* IPduId */           2U,
    /* DLC */              1U,
    /* PduRId */           2U,
    /* FirstTimeoutMs */   500U,
    /* TimeoutMs */        500U,
    /* IsSignalGroup */    1U,
    /* TxModeMode */       COM_TX_MODE_DIRECT,  /* RX I-PDU では未使用 */
    /* TxPeriodMs */       0U,
    /* TxModeModeTrue */   COM_TX_MODE_DIRECT,
    /* TxPeriodMsTrue */   0U,
    /* MinDelayMs */       0U,
    /* UpdateBitPosition */ 0xFFU,
    /* IpduGroupId */      COM_IPDU_GROUP_NONE,
    /* RxIndicationCbk */  NULL,
    /* TxTransformCbk */   NULL,
    /* TxAckCbk */         NULL,
    /* TxErrCbk */         NULL,
    /* RxAckCbk */         NULL,
    /* NumberOfRepetitions */ 0U,
    /* RepetitionPeriodMs */  0U,
    /* TxFirstTimeoutMs */    0U,
    /* TxTimeoutMs */         0U,
    /* TxTOutCbk */           NULL,
    /* RxTOutCbk */           TestGroupRxTOutCbk
};

const Com_SignalConfigType kTestRxSignals[] = { kTestRxGroupSignalA, kTestRxGroupSignalB, kTestRxTimeoutGroupSignal };
const Com_IPduConfigType kTestRxIPdus[] = { kTestRxGroupIPdu, kTestRxTimeoutGroupIPdu };

const Com_ConfigType kTestComConfig = {
    /* RxIPdus */       kTestRxIPdus,
    /* RxIPduCount */   2U,
    /* TxIPdus */       NULL,
    /* TxIPduCount */   0U,
    /* Signals */       kTestRxSignals,
    /* SignalCount */   3U,
    /* GwMappings */    NULL,
    /* GwMappingCount */ 0U
};

class Bsw_ComStack_SignalGroup_Rx_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制

        Com_Init(&kTestComConfig);
        s_groupRxAckCount = 0U;
        s_groupRxTOutCount = 0U;
        s_misconfiguredGroupMemberRxTOutCount = 0U;

        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;  // DeInit() のログを抑制
        Com_DeInit();
    }

    /** IPduId=2（グループ）を受信させ、猶予カウンタ経由でしきい値超過に
     *  必要な準備をする共通ヘルパー。 */
    void ReceiveOnceGroup(void)
    {
        uint8 data[1] = { 0x00U };
        PduInfoType pdu = { data, 1U };
        Com_RxIndication(2U, &pdu);
    }

    /** ReceiveOnceGroup() の短小フレーム版（SduLength=0 < DLC=1）。
     *  [SWS_Com_00575] によりグループ全体が不採用になる。 */
    void ReceiveOnceGroupShort(void)
    {
        uint8 data[1] = { 0x00U };
        PduInfoType pdu = { data, 0U };
        Com_RxIndication(2U, &pdu);
    }
};

// ------------------------------------------------------------
// SWS_Com_00555（Com_CbkRxAck、Signal Group 側経路）
// ------------------------------------------------------------
TEST_F(Bsw_ComStack_SignalGroup_Rx_Test, ComRxIndication_OK_GroupAck_FiresOnceRegardlessOfMemberCount)
{
    /* 準備 (Arrange): IPduId=1（Signal Group、メンバー2本）をフル DLC で受信 */
    uint8 buf[1] = { 0x03U };
    PduInfoType pduInfo = { buf, 1U };

    /* 実行 (Act) */
    Com_RxIndication(1U, &pduInfo);

    /* 評価 (Assert): メンバー数（2）に関わらず、グループ単位で厳密に1回 */
    EXPECT_EQ(s_groupRxAckCount, 1U);
}


TEST_F(Bsw_ComStack_SignalGroup_Rx_Test, ComRxIndication_NG_GroupAck_DoesNotFireOnShortFrameDiscard)
{
    /* 準備 (Arrange): IPduId=1 の DLC(1) 未満の受信長
     * （[SWS_Com_00575] によりグループ全体が不採用になる） */
    uint8 buf[1] = { 0x00U };
    PduInfoType pduInfo = { buf, 0U };

    /* 実行 (Act) */
    Com_RxIndication(1U, &pduInfo);

    /* 評価 (Assert): バッファへ格納されていないため RxAckCbk も呼ばれない */
    EXPECT_EQ(s_groupRxAckCount, 0U);
}


// ------------------------------------------------------------
// SWS_Com_00536/00556（Com_CbkRxTOut、グループ単位経路）
// ------------------------------------------------------------
TEST_F(Bsw_ComStack_SignalGroup_Rx_Test, ComMainFunction_NG_GroupShortFrameDiscardStillResetsDeadlineTimer)
{
    /* 準備 (Arrange): t=300ms で IPduId=1（グループ）へ短小フレームが届き
     * [SWS_Com_00575] によりグループ全体が不採用になる。もしタイマが
     * リセットされていなければ、Com_Init() 時点(t=0)を起点に t=600ms で
     * 500ms しきい値を超えてタイムアウトしてしまう。 */
    FakeMillis_Value = 300UL;
    ReceiveOnceGroupShort();

    /* 実行 (Act) */
    FakeMillis_Value = 600UL;
    Com_MainFunctionRx();

    /* 評価 (Assert): [SWS_Com_00738]（無効なシグナルグループ受信時も
     * タイマは再始動される）どおり、拒否された t=300ms を起点にまだ
     * 300ms しか経過しておらず、タイムアウトしない。 */
    EXPECT_EQ(Com_IsRxTimedOut(2U), 0U);
    EXPECT_EQ(s_groupRxTOutCount, 0U);
}


TEST_F(Bsw_ComStack_SignalGroup_Rx_Test, ComMainFunction_OK_GroupRxTOutFiresAfterThresholdElapsed)
{
    /* 準備 (Arrange) */
    ReceiveOnceGroup();
    FakeMillis_Value = 600UL;

    /* 実行 (Act) */
    Com_MainFunctionRx();

    /* 評価 (Assert): グループ単位のコールバックが発火する。IPduId=0 側の
     * シグナル単位監視は本テストでは検証対象外（同じ 500ms しきい値かつ
     * 一度も受信させていないため Com_Init() 起点で同時に満了し、
     * s_sigRxTOutCount 側も独立に発火しうる——これは IPduId=0/1 が別々の
     * I-PDU である以上正しい挙動であり、本テストの主張ではない） */
    EXPECT_EQ(Com_IsRxTimedOut(2U), 1U);
    EXPECT_EQ(s_groupRxTOutCount, 1U);
}


TEST_F(Bsw_ComStack_SignalGroup_Rx_Test, ComMainFunction_NG_GroupBeforeThreshold_DoesNotFire)
{
    /* 準備 (Arrange) */
    ReceiveOnceGroup();
    FakeMillis_Value = 400UL;

    /* 実行 (Act) */
    Com_MainFunctionRx();

    /* 評価 (Assert) */
    EXPECT_EQ(Com_IsRxTimedOut(2U), 0U);
    EXPECT_EQ(s_groupRxTOutCount, 0U);
}


TEST_F(Bsw_ComStack_SignalGroup_Rx_Test, ComMainFunction_NG_MisconfiguredGroupMemberDoesNotDoubleFire)
{
    /* 準備 (Arrange): kTestRxTimeoutGroupSignal は Signal Group メンバー
     * （IPduId=1、IsSignalGroup=1）でありながら、非 Signal Group シグナルの
     * ようにコピペ設定されてしまった状態（FirstTimeoutMs/TimeoutMs/RxTOutCbk
     * が設定済み）を模している。ipdu->IsSignalGroup のランタイムガードが
     * 無ければ、シグナル単位ループでもこのメンバーの RxTOutCbk が誤って
     * 発火してしまう（/code-review で指摘された二重発火シナリオ）。 */
    ReceiveOnceGroup();
    FakeMillis_Value = 600UL;

    /* 実行 (Act) */
    Com_MainFunctionRx();

    /* 評価 (Assert): グループ単位のコールバックは正しく1回発火するが、
     * 誤設定されたメンバー側のシグナル単位コールバックは発火しない */
    EXPECT_EQ(s_groupRxTOutCount, 1U);
    EXPECT_EQ(s_misconfiguredGroupMemberRxTOutCount, 0U);
}



// -----------------------------------------------------------------------
// Com_IpduGroupStart/Stop の RX 側検証（本番 EngineInfo/AbsInfo が
// COM_IPDU_GROUP_NONE から実グループ COM_IPDU_GROUP_SENSOR_RX へ移行した
// ことに伴う回帰）。専用の最小 Com 設定・フィクスチャを別に用意する
// （上の kTestComConfig とは独立した COM_RX_IPDU_MAX の枠を使う）。
// -----------------------------------------------------------------------
namespace rx_ipdu_group
{

static uint8_t s_groupedRxTOutCount = 0U;
static void TestGroupedRxTOutCbk(void) { s_groupedRxTOutCount++; }

// 本番の COM_IPDU_GROUP_SENSOR_RX（Com_Cfg.h、Com.h 経由で可視）を直接
// 使う（独自のローカル定数を別途定義すると、値がたまたま一致しているだけの
// 見せかけの回帰テストになり、本番側の値が変わっても追従できず静かに
// ズレてしまうため。/code-review で指摘）。
const Com_IPduConfigType kTestRxGroupedIPdu = {
    /* IPduId */           0U,
    /* DLC */              1U,
    /* PduRId */           0U,
    /* FirstTimeoutMs */   500U,
    /* TimeoutMs */        500U,
    /* IsSignalGroup */    1U,  // I-PDU 単位の RxTOutCbk は Signal Group 専用
    /* TxModeMode */       COM_TX_MODE_DIRECT,  /* RX I-PDU では未使用 */
    /* TxPeriodMs */       0U,
    /* TxModeModeTrue */   COM_TX_MODE_DIRECT,
    /* TxPeriodMsTrue */   0U,
    /* MinDelayMs */       0U,
    /* UpdateBitPosition */ 0xFFU,
    /* IpduGroupId */      COM_IPDU_GROUP_SENSOR_RX,
    /* RxIndicationCbk */  NULL,
    /* TxTransformCbk */   NULL,
    /* TxAckCbk */         NULL,
    /* TxErrCbk */         NULL,
    /* RxAckCbk */         NULL,
    /* NumberOfRepetitions */ 0U,
    /* RepetitionPeriodMs */  0U,
    /* TxFirstTimeoutMs */    0U,
    /* TxTimeoutMs */         0U,
    /* TxTOutCbk */           NULL,
    /* RxTOutCbk */           TestGroupedRxTOutCbk
    // RxTOutCbk より後ろ（RxIpduCalloutCbk/TxIpduCalloutCbk）は未使用のため
    // 省略（C の集成体初期化で NULL 埋め）。
};

// IPduId=1: 本番 EngineInfo と同じ形（非 Signal Group、シグナル単位の
// RxDataTimeoutAction=SUBSTITUTE + RxTOutCbk）を再現する。上の
// kTestRxGroupedIPdu（IPduId=0、Signal Group、I-PDU 単位 RxTOutCbk）だけでは
// AbsInfo 寄りの経路しか検証できておらず、Com_MainFunctionRx() のシグナル単位
// ループ・Com_ReceiveSignal() の SUBSTITUTE 分岐がグループ停止中に正しく
// 抑制されることを検証できていなかった（/code-review で指摘）。「I-PDU
// Group 制御が非 Signal Group メンバーにも一律に効くこと」の裏付けのため、
// このファイル（SignalGroup 側）に残している（ファイル冒頭コメント参照）。
static uint8_t s_nonGroupRxTOutCount = 0U;
static void TestNonGroupRxTOutCbk(void) { s_nonGroupRxTOutCount++; }

const Com_SignalConfigType kTestRxGroupedNonGroupSignal = {
    /* SignalId */                0U,
    /* Direction */                COM_SIGNAL_DIRECTION_RX,
    /* IPduId */                   1U,
    /* BitPosition */              0U,
    /* BitSize */                  16U,
    /* Endian */                   COM_BIG_ENDIAN,
    /* InitValue */                0xAAAAU,
    /* FilterAlgorithm */          COM_FILTER_ALWAYS,
    /* Mask */                     0U,
    /* FilterX */                  0U,
    /* FilterMin */                0U,
    /* FilterMax */                0U,
    /* FilterRejectCbk */          NULL,
    /* TmsContributor */           0U,
    /* UpdateBitContributor */     0U,
    /* TransferProperty */         COM_TRANSFER_PROPERTY_PENDING,
    /* RxDataTimeoutAction */      COM_RX_TIMEOUT_ACTION_SUBSTITUTE,
    /* TimeoutSubstitutionValue */ 0xFFFFU,
    /* DataInvalidAction */        COM_DATA_INVALID_ACTION_NONE,
    /* InvalidValue */             0U,
    /* InvalidNotificationCbk */   NULL,
    /* FirstTimeoutMs */           500U,
    /* TimeoutMs */                500U,
    /* RxTOutCbk */                TestNonGroupRxTOutCbk,
    /* TxAckCbk */                 NULL,
    /* TxErrCbk */                 NULL
};

const Com_IPduConfigType kTestRxGroupedNonGroupIPdu = {
    /* IPduId */           1U,
    /* DLC */              2U,
    /* PduRId */           1U,
    /* FirstTimeoutMs */   0U,  /* I-PDU 単位の監視は無効化（シグナル単位のみ対象） */
    /* TimeoutMs */        0U,
    /* IsSignalGroup */    0U,  // 本番 EngineInfo と同じ非 Signal Group
    /* TxModeMode */       COM_TX_MODE_DIRECT,  /* RX I-PDU では未使用 */
    /* TxPeriodMs */       0U,
    /* TxModeModeTrue */   COM_TX_MODE_DIRECT,
    /* TxPeriodMsTrue */   0U,
    /* MinDelayMs */       0U,
    /* UpdateBitPosition */ 0xFFU,
    /* IpduGroupId */      COM_IPDU_GROUP_SENSOR_RX
};

// IPduId=2(RX)・IPduId=0(TX) は Com_EnableReceptionDM/Com_DisableReceptionDM
// （[SWS_Com_00534]）専用: 本番の COM_IPDU_GROUP_NONE が実際に RX/TX 混在
// グループであること（Com_PBCfg.c 参照: RX 1本・TX 3本が同じ
// COM_IPDU_GROUP_NONE に属する）を再現し、「TX I-PDU を含むグループへの
// 呼び出しは要求全体を無視しなければならない」ことを検証する。
const Com_IPduConfigType kTestRxMixedGroupIPdu = {
    /* IPduId */           2U,
    /* DLC */              1U,
    /* PduRId */           2U,
    /* FirstTimeoutMs */   0U,
    /* TimeoutMs */        0U,
    /* IsSignalGroup */    0U,
    /* TxModeMode */       COM_TX_MODE_DIRECT,  /* RX I-PDU では未使用 */
    /* TxPeriodMs */       0U,
    /* TxModeModeTrue */   COM_TX_MODE_DIRECT,
    /* TxPeriodMsTrue */   0U,
    /* MinDelayMs */       0U,
    /* UpdateBitPosition */ 0xFFU,
    /* IpduGroupId */      COM_IPDU_GROUP_NONE
};

const Com_IPduConfigType kTestTxMixedGroupIPdu = {
    /* IPduId */           0U,
    /* DLC */              1U,
    /* PduRId */           0U,
    /* FirstTimeoutMs */   0U,
    /* TimeoutMs */        0U,
    /* IsSignalGroup */    0U,
    /* TxModeMode */       COM_TX_MODE_DIRECT,
    /* TxPeriodMs */       0U,
    /* TxModeModeTrue */   COM_TX_MODE_DIRECT,
    /* TxPeriodMsTrue */   0U,
    /* MinDelayMs */       0U,
    /* UpdateBitPosition */ 0xFFU,
    /* IpduGroupId */      COM_IPDU_GROUP_NONE
};

const Com_IPduConfigType kTestRxIpduGroupIPdus[] = {
    kTestRxGroupedIPdu, kTestRxGroupedNonGroupIPdu, kTestRxMixedGroupIPdu
};
const Com_IPduConfigType kTestTxIpduGroupIPdus[] = { kTestTxMixedGroupIPdu };
const Com_SignalConfigType kTestRxIpduGroupSignals[] = { kTestRxGroupedNonGroupSignal };

const Com_ConfigType kTestRxIpduGroupConfig = {
    /* RxIPdus */       kTestRxIpduGroupIPdus,
    /* RxIPduCount */   3U,
    /* TxIPdus */       kTestTxIpduGroupIPdus,
    /* TxIPduCount */   1U,
    /* Signals */       kTestRxIpduGroupSignals,
    /* SignalCount */   1U,
    /* GwMappings */    NULL,
    /* GwMappingCount */ 0U
};

class Bsw_ComStack_SignalGroup_RxIpduGroup_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        FakeDetHw_LogSuppressed = 1U;
        Com_Init(&kTestRxIpduGroupConfig);
        s_groupedRxTOutCount = 0U;
        s_nonGroupRxTOutCount = 0U;
        FakeDetHw_LogSuppressed = 0U;
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Com_DeInit();
    }

    /** IPduId=1（非 Signal Group）へ実データを1回受信させるヘルパー。 */
    void ReceiveOnceNonGroup(uint16_t value)
    {
        uint8 data[2] = { (uint8)(value >> 8), (uint8)(value & 0xFFU) };
        PduInfoType pdu = { data, 2U };
        Com_RxIndication(1U, &pdu);
    }
};

// ------------------------------------------------------------
// Com_IpduGroupStart/Stop（[SWS_Com_00444]/[SWS_Com_00685] 等）
// ------------------------------------------------------------
TEST_F(Bsw_ComStack_SignalGroup_RxIpduGroup_Test, ComMainFunction_NG_StoppedGroupedIPduNeverTimesOutRegardlessOfElapsed)
{
    /* 準備 (Arrange): Com_IpduGroupStart() を一度も呼ばない
     * （[SWS_Com_00444]: I-PDU Group は既定で停止状態） */

    /* 実行 (Act): TimeoutMs(500ms) を大幅に超えて経過させる */
    FakeMillis_Value += 5000U;
    Com_MainFunctionRx();
    Com_MainFunctionTx();

    /* 評価 (Assert): 停止中はデッドライン監視自体が評価されないため
     * （[SWS_Com_00685]）、いくら経過しても RxTOutCbk は発火しない
     * （本番の Bus-Sleep 中に EngineInfo/AbsInfo の RX タイムアウトが
     * 誤って発火しないことの裏付け）。 */
    EXPECT_EQ(s_groupedRxTOutCount, 0U);
}


TEST_F(Bsw_ComStack_SignalGroup_RxIpduGroup_Test, ComIpduGroupStart_OK_GroupedIPduBeginsMonitoringAfterExplicitStart)
{
    /* 準備 (Arrange): 明示的に開始する（BswM の FULL_COM 遷移ルート相当） */
    Com_IpduGroupStart(COM_IPDU_GROUP_SENSOR_RX, 0U);

    /* 実行 (Act): TimeoutMs(500ms) 経過 */
    FakeMillis_Value += 500U;
    Com_MainFunctionRx();
    Com_MainFunctionTx();

    /* 評価 (Assert): 開始後は通常どおり監視が働き、タイムアウトが発火する */
    EXPECT_EQ(s_groupedRxTOutCount, 1U);
}


TEST_F(Bsw_ComStack_SignalGroup_RxIpduGroup_Test, ComIpduGroupStop_OK_StoppingAgainSuppressesTimeoutEvenAfterElapsed)
{
    /* 準備 (Arrange): 一度開始してから、しきい値に達する前に停止する
     * （本番の Bus-Sleep 遷移相当: FULL_COM → NO_COMMUNICATION）。 */
    Com_IpduGroupStart(COM_IPDU_GROUP_SENSOR_RX, 0U);
    FakeMillis_Value += 100U;
    Com_MainFunctionRx();
    Com_MainFunctionTx();
    ASSERT_EQ(s_groupedRxTOutCount, 0U);  // まだしきい値未満

    Com_IpduGroupStop(COM_IPDU_GROUP_SENSOR_RX);

    /* 実行 (Act): 停止中にしきい値を大幅に超えて経過させる */
    FakeMillis_Value += 5000U;
    Com_MainFunctionRx();
    Com_MainFunctionTx();

    /* 評価 (Assert): 停止中は評価されないため発火しない
     * （意図的なスリープのたびに誤って RX timeout が記録されていた
     * 問題が解消されていることの直接的な裏付け）。 */
    EXPECT_EQ(s_groupedRxTOutCount, 0U);
}


TEST_F(Bsw_ComStack_SignalGroup_RxIpduGroup_Test, ComIpduGroupStop_OK_NonGroupSignalFreezesInsteadOfSubstitutingWhileStopped)
{
    /* 準備 (Arrange): 開始→実受信→しきい値未満で停止
     * （本番の Bus-Sleep 遷移相当: FULL_COM → NO_COMMUNICATION）。 */
    Com_IpduGroupStart(COM_IPDU_GROUP_SENSOR_RX, 0U);
    ReceiveOnceNonGroup(0x1234U);
    FakeMillis_Value += 100U;
    Com_MainFunctionRx();
    Com_MainFunctionTx();
    ASSERT_EQ(s_nonGroupRxTOutCount, 0U);  // まだしきい値未満

    Com_IpduGroupStop(COM_IPDU_GROUP_SENSOR_RX);

    /* 実行 (Act): 停止中にしきい値を大幅に超えて経過させる */
    FakeMillis_Value += 5000U;
    Com_MainFunctionRx();
    Com_MainFunctionTx();

    /* 評価 (Assert): 停止中はシグナル単位ループも評価されないため
     * RxTOutCbk は発火しない。Com_ReceiveSignal() も SUBSTITUTE
     * （0xFFFF）へ切り替わらず、停止直前の実受信値（0x1234）が
     * 凍結されたまま返り続ける。これが本来の目的（Bus-Sleep 中に
     * 「通信異常」として誤って上位層へ伝わらないようにする）だが、
     * 裏を返せば「本当に通信異常が起きても、再開までは検知されない」
     * ことも意味する（意図的な停止期間中は当然の仕様）。戻り値は
     * [SWS_Com_00684]/[SWS_Com_00685]/Table 3 のとおり
     * COM_SERVICE_NOT_AVAILABLE（2026-09-20 是正。以前は
     * COM_SERVICE_NOT_AVAILABLE 定数が存在せず E_OK のままだった）。 */
    EXPECT_EQ(s_nonGroupRxTOutCount, 0U);
    uint16_t value = 0U;
    uint8 ret = Com_ReceiveSignal(0U, &value);
    EXPECT_EQ(ret, COM_SERVICE_NOT_AVAILABLE);
    EXPECT_EQ(value, 0x1234U);  // SUBSTITUTE(0xFFFF) にはならない
}


// ------------------------------------------------------------
// COM_SERVICE_NOT_AVAILABLE（[SWS_Com_00461]/[SWS_Com_00857]/Table 3）
// ------------------------------------------------------------
TEST_F(Bsw_ComStack_SignalGroup_RxIpduGroup_Test, ComReceiveSignalGroup_OK_ReturnsServiceNotAvailableWhenGroupStopped)
{
    /* 準備 (Arrange): 開始してから停止する（Com_Init() 直後の未開始状態との
     * 区別のため、明示的に一度開始してから止める）。 */
    Com_IpduGroupStart(COM_IPDU_GROUP_SENSOR_RX, 0U);
    Com_IpduGroupStop(COM_IPDU_GROUP_SENSOR_RX);

    /* 実行 (Act) */
    uint8 ret = Com_ReceiveSignalGroup(0U);

    /* 評価 (Assert): [SWS_Com_00461] 停止中でもシャドウバッファへの
     * コピー自体は行うが、戻り値は COM_SERVICE_NOT_AVAILABLE。 */
    EXPECT_EQ(ret, COM_SERVICE_NOT_AVAILABLE);
}


TEST_F(Bsw_ComStack_SignalGroup_RxIpduGroup_Test, ComReceiveSignalGroupArray_OK_ReturnsServiceNotAvailableWhenGroupStoppedButStillCopies)
{
    /* 準備 (Arrange): 開始して実受信させてから停止する（受信値が
     * 凍結されたまま返ることも合わせて確認する）。 */
    Com_IpduGroupStart(COM_IPDU_GROUP_SENSOR_RX, 0U);
    uint8 data[1] = { 0xABU };
    PduInfoType pdu = { data, 1U };
    Com_RxIndication(0U, &pdu);
    Com_IpduGroupStop(COM_IPDU_GROUP_SENSOR_RX);

    /* 実行 (Act) */
    uint8 out[1] = { 0U };
    uint8 ret = Com_ReceiveSignalGroupArray(0U, out);

    /* 評価 (Assert): [SWS_Com_00857] 停止中でも最後の受信値をそのまま
     * コピーするが、戻り値は COM_SERVICE_NOT_AVAILABLE。 */
    EXPECT_EQ(ret, COM_SERVICE_NOT_AVAILABLE);
    EXPECT_EQ(out[0], 0xABU);
}


TEST_F(Bsw_ComStack_SignalGroup_RxIpduGroup_Test, ComIpduGroupStart_OK_NonGroupSignalTimesOutAndSubstitutesAfterExplicitStart)
{
    /* 準備 (Arrange): 明示的に開始してから実受信させる */
    Com_IpduGroupStart(COM_IPDU_GROUP_SENSOR_RX, 0U);
    ReceiveOnceNonGroup(0x1234U);

    /* 実行 (Act): TimeoutMs(500ms) 経過 */
    FakeMillis_Value += 500U;
    Com_MainFunctionRx();
    Com_MainFunctionTx();

    /* 評価 (Assert): 開始後は通常どおり監視が働き、RxTOutCbk が発火し
     * Com_ReceiveSignal() も SUBSTITUTE 値を返すようになる。 */
    EXPECT_EQ(s_nonGroupRxTOutCount, 1U);
    uint16_t value = 0U;
    uint8 ret = Com_ReceiveSignal(0U, &value);
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(value, 0xFFFFU);
}


// ------------------------------------------------------------
// Com_EnableReceptionDM/Com_DisableReceptionDM（SRS_Com_00192）
// ------------------------------------------------------------
TEST_F(Bsw_ComStack_SignalGroup_RxIpduGroup_Test, ComEnableDisableReceptionDM_OK_TogglesFlagForMatchingGroupIPdusOnly)
{
    /* 評価 (Assert): Com_Init() 直後は既定で有効 */
    EXPECT_EQ(Com_Test_GetRxDmEnabled(0U), 1U);
    EXPECT_EQ(Com_Test_GetRxDmEnabled(1U), 1U);

    /* 実行 (Act) + 評価 (Assert): 無効化すると両方とも 0 になる
     * （IPduId=0/1 いずれも COM_IPDU_GROUP_SENSOR_RX 所属） */
    Com_DisableReceptionDM(COM_IPDU_GROUP_SENSOR_RX);
    EXPECT_EQ(Com_Test_GetRxDmEnabled(0U), 0U);
    EXPECT_EQ(Com_Test_GetRxDmEnabled(1U), 0U);

    /* 実行 (Act) + 評価 (Assert): 再度有効化すると両方とも 1 に戻る */
    Com_EnableReceptionDM(COM_IPDU_GROUP_SENSOR_RX);
    EXPECT_EQ(Com_Test_GetRxDmEnabled(0U), 1U);
    EXPECT_EQ(Com_Test_GetRxDmEnabled(1U), 1U);
}


TEST_F(Bsw_ComStack_SignalGroup_RxIpduGroup_Test, DisableReceptionDM_OK_SuppressesGroupTimeoutWhileIpduGroupStaysStarted)
{
    /* 準備 (Arrange): 開始してからデッドライン監視のみ無効化する
     * （Com_IpduGroupStop() とは異なり、I-PDU Group 自体は起動済みのまま）。 */
    Com_IpduGroupStart(COM_IPDU_GROUP_SENSOR_RX, 0U);
    Com_DisableReceptionDM(COM_IPDU_GROUP_SENSOR_RX);

    /* 実行 (Act): しきい値(500ms)を大幅に超えて経過させる */
    FakeMillis_Value += 5000U;
    Com_MainFunctionRx();
    Com_MainFunctionTx();

    /* 評価 (Assert): デッドライン監視のみ抑制され、RxTOutCbk は発火しない */
    EXPECT_EQ(s_groupedRxTOutCount, 0U);

    /* 評価 (Assert): Com_IpduGroupStop() と違い、受信処理自体は継続している
     * ことを確認する（IPduId=1、非 Signal Group側で実際に受信させ、
     * Com_ReceiveSignal() が新しい値を返すことで検証）。 */
    ReceiveOnceNonGroup(0x5678U);
    uint16_t value = 0U;
    uint8 ret = Com_ReceiveSignal(0U, &value);
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(value, 0x5678U);
}


TEST_F(Bsw_ComStack_SignalGroup_RxIpduGroup_Test, EnableReceptionDM_OK_ResetsTimerAndResumesDetectionWithoutImmediateFalseTimeout)
{
    /* 準備 (Arrange): 開始→実受信→デッドライン監視を無効化したまま
     * しきい値を大幅に超えて放置する。 */
    Com_IpduGroupStart(COM_IPDU_GROUP_SENSOR_RX, 0U);
    ReceiveOnceNonGroup(0x1234U);
    Com_DisableReceptionDM(COM_IPDU_GROUP_SENSOR_RX);
    FakeMillis_Value += 5000U;
    Com_MainFunctionRx();
    Com_MainFunctionTx();
    ASSERT_EQ(s_nonGroupRxTOutCount, 0U);  // 無効化中は評価されない

    /* 実行 (Act): 再度有効化した直後に Com_MainFunctionRx() を呼ぶ */
    Com_EnableReceptionDM(COM_IPDU_GROUP_SENSOR_RX);
    Com_MainFunctionRx();
    Com_MainFunctionTx();

    /* 評価 (Assert): タイマが再始動されているため、無効化中に「経過して
     * いた」5000ms を理由に即座にタイムアウト判定されない
     * （Com_IpduGroupStart() の [SWS_Com_00787] 項目2と同じ理由）。 */
    EXPECT_EQ(s_nonGroupRxTOutCount, 0U);

    /* 実行 (Act): 再有効化後、改めて TimeoutMs(500ms) 経過させる */
    FakeMillis_Value += 500U;
    Com_MainFunctionRx();
    Com_MainFunctionTx();

    /* 評価 (Assert): 通常どおり監視が再開されていること */
    EXPECT_EQ(s_nonGroupRxTOutCount, 1U);
}


TEST_F(Bsw_ComStack_SignalGroup_RxIpduGroup_Test, DisableReceptionDM_NG_UnknownIpduGroupIdHasNoEffect)
{
    /* 準備 (Arrange): 不要。既定で有効なまま */

    /* 実行 (Act): どの I-PDU にも一致しない IpduGroupId を指定する */
    Com_DisableReceptionDM(static_cast<Com_IpduGroupIdType>(0xAAU));

    /* 評価 (Assert): 何も変化しない */
    EXPECT_EQ(Com_Test_GetRxDmEnabled(0U), 1U);
    EXPECT_EQ(Com_Test_GetRxDmEnabled(1U), 1U);
}


// ------------------------------------------------------------
// [SWS_Com_00534]（RX/TX 混在グループは要求全体を無視する）
// ------------------------------------------------------------
TEST_F(Bsw_ComStack_SignalGroup_RxIpduGroup_Test, DisableReceptionDM_NG_MixedRxTxGroupIsIgnoredEntirely)
{
    /* 準備 (Arrange): 不要。既定で有効なまま */
    ASSERT_EQ(Com_Test_GetRxDmEnabled(2U), 1U);

    /* 実行 (Act): RX/TX 混在グループ（COM_IPDU_GROUP_NONE）を指定する */
    Com_DisableReceptionDM(COM_IPDU_GROUP_NONE);

    /* 評価 (Assert): 要求全体が無視され、RX メンバー（IPduId=2）のフラグも
     * 変化しない。 */
    EXPECT_EQ(Com_Test_GetRxDmEnabled(2U), 1U);
}


TEST_F(Bsw_ComStack_SignalGroup_RxIpduGroup_Test, EnableReceptionDM_NG_MixedRxTxGroupIsIgnoredEntirely)
{
    /* 準備 (Arrange): 不要。既定で有効なため Enable 側だけでは差が出ない。
     * Disable 側が [SWS_Com_00534] のガードにより無視されて 1 のままである
     * こと自体は上のテストで確認済みのため、ここでは Enable 単体を呼んでも
     * DET_LOGW 以外の副作用が無い（クラッシュ・範囲外アクセスしない）ことを
     * 確認する（境界条件としての最小限の呼び出し安全性の検証）。 */

    /* 実行 (Act) + 評価 (Assert) */
    Com_EnableReceptionDM(COM_IPDU_GROUP_NONE);
    EXPECT_EQ(Com_Test_GetRxDmEnabled(2U), 1U);
}


}  // namespace rx_ipdu_group

}  // namespace
