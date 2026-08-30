/**
 * \file    Bsw_RxTimeoutChain_test.cpp
 * \brief   README.md「Rx 処理」の「デッドライン監視（受信タイムアウト）」
 *          コールチェーンの単体テスト（GoogleTest / PlatformIO `[env:native_chain]`）。
 *
 * \details README の該当コールチェーン図：
 *
 *              [100ms 周期タスク] Os_SchedulerStep() → Com_MainFunctionRx()
 *                → (now - Com_RxLastMs[id]) がしきい値以上なら
 *                    Com_SigTimedOut[s] を立てる（WARN ログ、ここが検知点）
 *                ┊  (Com_SigTimedOut というフラグ経由。次に Com_ReceiveSignal()
 *                ┊   が呼ばれるまで非同期に待機)
 *                ↓
 *              Com_ReceiveSignal()               ← Rte 等から呼ばれる（同期）
 *                → RxDataTimeoutAction に応じて返す値を決定
 *                    SUBSTITUTE : TimeoutSubstitutionValue で置換
 *                    REPLACE    : InitValue で置換
 *                    NONE       : E_NOT_OK（既定、呼び出し元は自分の初期値を使う）
 *
 *          Tx 処理コールチェーン（Bsw_TxChain_test.cpp）の `Com_TxPending` と
 *          構造は同じ「立てる側／読む側が別々のタイミングで動く」非同期境界だが、
 *          向きが逆になっている: TX は「on-demand 呼び出し（Com_SendSignal）が
 *          立てて、周期タスク（Com_MainFunctionTx）が読む」のに対し、こちらは
 *          「周期タスク（Com_MainFunctionRx）が立てて、on-demand 呼び出し
 *          （Com_ReceiveSignal）が読む」。この非同期境界でテストを2つの
 *          セグメントに分け、それぞれ個別に実行可能な `TEST_F` ケースとしている
 *          （`--gtest_filter=Bsw_RxTimeoutChain_Test.ComMainFunction_*` 等で
 *          絞り込み可）。
 *
 *          この非同期境界の検知はどちらも Com.c 内で完結する（PduR/CanIf/Can/
 *          CanSM は経由しない）ため、Tx/Rx チェーンと異なりフェイクは
 *          `millis()`（`test/test_chain/Hal_Millis_fake.c`）のみで足りる。
 *          `Com_RxIndication()` を直接呼んで受信済み状態を作ってから
 *          `FakeMillis_Value` を進める、という手順で「通信していたが途絶えた」
 *          状況を再現する。
 *
 *          `Com_SigTimedOut[]` はテスト専用アクセサ `Com_Test_GetSigTimedOut()`
 *          （`COM_UNIT_TEST` 定義時のみ、`Com.h`/`Com.c` 参照）で直接観測する。
 *          セグメント②は Tx チェーンの `ComMainFunction_OK_...` と同様、
 *          セグメント①の終端状態（フラグが立った状態）を Arrange で
 *          `Com_RxIndication()`→`FakeMillis_Value` 加算→`Com_MainFunctionRx()`と
 *          再現してから、そこに続く `Com_ReceiveSignal()` を検証する。
 *
 *          本番の `Com_PBCfg.c` は Rte 依存の各種コールバックを持ち依存グラフが
 *          巨大なため（Bsw_TxChain_test.cpp 冒頭コメントと同じ理由）、1シグナル・
 *          1 I-PDU のみのテスト専用の最小 Com 設定を本ファイル内で定義する。
 */
#include <gtest/gtest.h>

extern "C" {
#include "Com.h"
#include "Hal_Millis_fake.h"
#include "Hal_Det_Hw_fake.h"
}

namespace
{

// -----------------------------------------------------------------------
// テスト専用の最小 Com 設定（Bsw_TxChain_test.cpp/Bsw_RxChain_test.cpp と
// 同じ方針）。SignalId=0 (RX, 16bit BigEndian) 1本だけを持つ IPduId=0 の
// RX I-PDU。RxDataTimeoutAction=SUBSTITUTE、TimeoutSubstitutionValue=0xFFFF
// （本番の VehicleSpeed と同じ実運用パターン）。I-PDU 単位のデッドライン監視
// (FirstTimeoutMs/TimeoutMs=0) は無効化し、シグナル単位のみを対象にする
// （Com_ReceiveSignal() は非 Signal Group シグナルに対して Com_SigTimedOut[]
// のみを見るため、この単純化で本チェーンの検証は完結する。Com.c 該当コメント
// 参照）。
//
// IPduId=1 は SWS_Com_00536/00556（Com_CbkRxTOut）のグループ単位経路専用の
// 追加 I-PDU（IsSignalGroup=1、メンバー1本）。IPduId=0 側とは独立に
// I-PDU 単位の FirstTimeoutMs/TimeoutMs を持たせ、Com_MainFunctionRx() の
// I-PDU 単位ループ（本ファイル冒頭のコールチェーン図のうち、シグナル単位
// ループとは別の分岐）を検証する。
// -----------------------------------------------------------------------

// SWS_Com_00536/00556（Com_CbkRxTOut）検証用カウンタ・コールバック。
static uint8_t s_sigRxTOutCount = 0U;
static void TestSigRxTOutCbk(void) { s_sigRxTOutCount++; }

static uint8_t s_groupRxTOutCount = 0U;
static void TestGroupRxTOutCbk(void) { s_groupRxTOutCount++; }

// /code-review で指摘された「Signal Group メンバーの除外は設定規約のみに
// 依存し、コードで担保していない」の回帰テスト用。誤って非 Signal Group
// シグナルのようにコピペ設定してしまった場合を模したカウンタ。
static uint8_t s_misconfiguredGroupMemberRxTOutCount = 0U;
static void TestMisconfiguredGroupMemberRxTOutCbk(void) { s_misconfiguredGroupMemberRxTOutCount++; }

const Com_SignalConfigType kTestRxTimeoutSignal = {
    /* SignalId */                0U,
    /* Direction */                COM_SIGNAL_DIRECTION_RX,
    /* IPduId */                   0U,
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
    /* RxTOutCbk */                TestSigRxTOutCbk,
    /* TxAckCbk */                 NULL,
    /* TxErrCbk */                 NULL
};

const Com_IPduConfigType kTestRxTimeoutIPdu = {
    /* IPduId */           0U,
    /* DLC */              2U,
    /* PduRId */           0U,
    /* FirstTimeoutMs */   0U,  /* I-PDU 単位の監視は無効化（本テストの対象外） */
    /* TimeoutMs */        0U,
    /* IsSignalGroup */    0U,
    /* TxModeMode */       COM_TX_MODE_DIRECT,  /* RX I-PDU では未使用 */
    /* TxPeriodMs */       0U,
    /* TxModeModeTrue */   COM_TX_MODE_DIRECT,
    /* TxPeriodMsTrue */   0U,
    /* MinDelayMs */       0U,
    /* UpdateBitPosition */ 0xFFU,
    /* IpduGroupId */      COM_IPDU_GROUP_NONE,
    /* RxIndicationCbk */  NULL,
    /* TxTransformCbk */   NULL
};

// IPduId=1: グループ単位 Com_CbkRxTOut 検証用（IsSignalGroup=1、メンバー1本）。
const Com_SignalConfigType kTestRxTimeoutGroupSignal = {
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
    /* FirstTimeoutMs */           500U,  /* Signal Group メンバーへの誤設定を意図的に再現
                                            * （本来は未使用=0 にすべき値。Com_Types.h 参照）。
                                            * ipdu->IsSignalGroup のランタイムガードが
                                            * 効いていれば、この誤設定があっても
                                            * TestMisconfiguredGroupMemberRxTOutCbk は
                                            * 発火しないはず（下記回帰テスト参照）。 */
    /* TimeoutMs */                500U,
    /* RxTOutCbk */                TestMisconfiguredGroupMemberRxTOutCbk,
    /* TxAckCbk */                 NULL,
    /* TxErrCbk */                 NULL
};

const Com_IPduConfigType kTestRxTimeoutGroupIPdu = {
    /* IPduId */           1U,
    /* DLC */              1U,
    /* PduRId */           1U,
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

// IPduId=2: [SWS_Com_00872] 段階順の回帰テスト用。RxIpduCalloutCbk が
// 必ず拒否する（＝バッファ・通知はいずれも動かない）設定でも、デッドライン
// 監視タイマ（段階1）だけは reject の前に既にリセットされていることを
// 検証する（/code-review で見つかった「本来タイムアウトしないはずの状況で
// タイムアウトしてしまう」問題の是正）。
static uint8_t s_rejectCalloutInvokeCount = 0U;
static uint8 TestAlwaysRejectCallout(const uint8* SduDataPtr, uint8 SduLength)
{
    (void)SduDataPtr;
    (void)SduLength;
    s_rejectCalloutInvokeCount++;
    return 0U;
}

const Com_IPduConfigType kTestRxTimeoutRejectedIPdu = {
    /* IPduId */           2U,
    /* DLC */              1U,
    /* PduRId */           2U,
    /* FirstTimeoutMs */   1000U,  /* TimeoutMs と意図的に異なる値にして、
                                    * reject されても First→steady 状態
                                    * 遷移が起きることを検証できるようにする
                                    * （下記回帰テスト参照）。 */
    /* TimeoutMs */        500U,
    /* IsSignalGroup */    0U,
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
    /* RxTOutCbk */           NULL,
    /* RxIpduCalloutCbk */    TestAlwaysRejectCallout
};

const Com_SignalConfigType kTestRxTimeoutSignals[] = {
    kTestRxTimeoutSignal, kTestRxTimeoutGroupSignal
};
const Com_IPduConfigType kTestRxTimeoutIPdus[] = {
    kTestRxTimeoutIPdu, kTestRxTimeoutGroupIPdu, kTestRxTimeoutRejectedIPdu
};

const Com_ConfigType kTestComRxTimeoutConfig = {
    /* RxIPdus */       kTestRxTimeoutIPdus,
    /* RxIPduCount */   3U,
    /* TxIPdus */       NULL,
    /* TxIPduCount */   0U,
    /* Signals */       kTestRxTimeoutSignals,
    /* SignalCount */   2U,
    /* GwMappings */    NULL,
    /* GwMappingCount */ 0U
};

class Bsw_RxTimeoutChain_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制

        Com_Init(&kTestComRxTimeoutConfig);
        s_sigRxTOutCount = 0U;
        s_groupRxTOutCount = 0U;
        s_misconfiguredGroupMemberRxTOutCount = 0U;
        s_rejectCalloutInvokeCount = 0U;

        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;  // DeInit() のログを抑制
        Com_DeInit();
    }

    /** 「受信していたが、その後途絶えた」状況を作る共通ヘルパー。
     *  Com_RxIndication() で実データを1回受信させ（Com_RxLastMs 更新・
     *  Com_SigTimedOut クリア）、Com_RxUsingFirstTimeout を steady 状態へ
     *  遷移させる。以降は sig->TimeoutMs（本テストでは 500ms）が基準になる。 */
    void ReceiveOnce(uint16_t value)
    {
        uint8 data[2] = { (uint8)(value >> 8), (uint8)(value & 0xFFU) };
        PduInfoType pdu = { data, 2U };
        Com_RxIndication(0U, &pdu);
    }

    /** ReceiveOnce() のグループ I-PDU（IPduId=1）版。 */
    void ReceiveOnceGroup(void)
    {
        uint8 data[1] = { 0x00U };
        PduInfoType pdu = { data, 1U };
        Com_RxIndication(1U, &pdu);
    }

    /** ReceiveOnce() の RxIpduCalloutCbk が必ず拒否する I-PDU（IPduId=2）版。 */
    void ReceiveOnceRejected(void)
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
        Com_RxIndication(1U, &pdu);
    }
};

// ------------------------------------------------------------
// セグメント①: Com_MainFunctionRx() ─ Com_SigTimedOut というフラグで切れるまで
// ------------------------------------------------------------
TEST_F(Bsw_RxTimeoutChain_Test, ComMainFunction_OK_DetectsTimeoutAfterThresholdElapsed)
{
    /* 準備 (Arrange): 一度受信させてから、しきい値(500ms)を超えて時間を進める */
    ReceiveOnce(0x1234U);
    FakeMillis_Value = 600UL;

    /* 実行 (Act) */
    Com_MainFunctionRx();

    /* 評価 (Assert): フラグに加えて SWS_Com_00536/00556 (Com_CbkRxTOut) の
     * シグナル単位コールバックも新規検出の瞬間に1回だけ呼ばれる */
    EXPECT_EQ(Com_Test_GetSigTimedOut(0U), 1U);
    EXPECT_EQ(s_sigRxTOutCount, 1U);
}

TEST_F(Bsw_RxTimeoutChain_Test, ComMainFunction_NG_BeforeThreshold_LeavesSigTimedOutClear)
{
    /* 準備 (Arrange): 一度受信させるが、しきい値(500ms)未満しか時間を進めない */
    ReceiveOnce(0x1234U);
    FakeMillis_Value = 400UL;

    /* 実行 (Act) */
    Com_MainFunctionRx();

    /* 評価 (Assert): まだ検知しない。コールバックも呼ばれない */
    EXPECT_EQ(Com_Test_GetSigTimedOut(0U), 0U);
    EXPECT_EQ(s_sigRxTOutCount, 0U);
}

TEST_F(Bsw_RxTimeoutChain_Test, ComMainFunction_OK_SigRxTOutCbkFiresOnlyOnceAcrossRepeatedCalls)
{
    /* 準備 (Arrange): しきい値超過を検出させたあと、時間をさらに進めて
     * Com_MainFunctionRx() を再度呼ぶ（エッジトリガのため2回目は発火しない） */
    ReceiveOnce(0x1234U);
    FakeMillis_Value = 600UL;
    Com_MainFunctionRx();
    ASSERT_EQ(s_sigRxTOutCount, 1U);

    /* 実行 (Act) */
    FakeMillis_Value = 700UL;
    Com_MainFunctionRx();

    /* 評価 (Assert): 新規検出時のみ発火するため回数は増えない */
    EXPECT_EQ(s_sigRxTOutCount, 1U);
}

// ------------------------------------------------------------
// [SWS_Com_00872] 段階順の回帰テスト（/code-review 指摘の是正確認）。
// RxIpduCalloutCbk に拒否されても、デッドライン監視タイマ（段階1）は
// callout（段階2）より先にリセットされているはず。
// ------------------------------------------------------------
TEST_F(Bsw_RxTimeoutChain_Test, ComMainFunction_NG_RejectedFrameStillResetsDeadlineTimer)
{
    /* 準備 (Arrange): t=300ms でフレーム到着→callout に拒否される。
     * もしタイマがリセットされていなければ、Com_Init() 時点(t=0)を
     * 起点に t=600ms で 500ms しきい値を超えてタイムアウトしてしまう。
     * タイマが正しくリセットされていれば、拒否された t=300ms を起点に
     * t=600ms 時点ではまだ 300ms しか経過しておらず、タイムアウトしない。 */
    FakeMillis_Value = 300UL;
    ReceiveOnceRejected();
    ASSERT_EQ(s_rejectCalloutInvokeCount, 1U);  // 拒否経路を通ったことの裏付け

    /* 実行 (Act) */
    FakeMillis_Value = 600UL;
    Com_MainFunctionRx();

    /* 評価 (Assert): 実 AUTOSAR ならタイムアウトしない状況
     * （バスは正常、ペイロードが拒否されただけ）で、実際にタイムアウト
     * しないことを確認する。 */
    EXPECT_EQ(Com_IsRxTimedOut(2U), 0U);
}

TEST_F(Bsw_RxTimeoutChain_Test, ComMainFunction_OK_RejectedFrameStillTransitionsToSteadyTimeout)
{
    /* 準備 (Arrange): IPduId=2 の初回受信（t=300ms）が callout に拒否される。
     * [SWS_Com_00715]（Com_RxIndication 呼び出し自体でタイマ再始動）と
     * [SWS_Com_00738]（シグナル値を考慮しない）により、拒否されても
     * First→steady の状態遷移自体は起きるはず。しきい値を FirstTimeoutMs
     * (1000ms) ではなく steady の TimeoutMs (500ms) に切り替えさせて
     * 検証する。 */
    FakeMillis_Value = 300UL;
    ReceiveOnceRejected();
    ASSERT_EQ(s_rejectCalloutInvokeCount, 1U);

    /* t=750ms（拒否からの経過 450ms）: steady(500ms) 未満のためまだ
     * タイムアウトしない */
    FakeMillis_Value = 750UL;
    Com_MainFunctionRx();
    ASSERT_EQ(Com_IsRxTimedOut(2U), 0U);

    /* 実行 (Act): t=850ms（拒否からの経過 550ms） */
    FakeMillis_Value = 850UL;
    Com_MainFunctionRx();

    /* 評価 (Assert): steady の 500ms は超えているためタイムアウトする。
     * もし拒否によって First(1000ms) のまま据え置かれていたら、
     * 550ms < 1000ms でタイムアウトしないはずなので、この違いで
     * 状態遷移の有無を判別できる。 */
    EXPECT_EQ(Com_IsRxTimedOut(2U), 1U);
}

TEST_F(Bsw_RxTimeoutChain_Test, ComMainFunction_NG_GroupShortFrameDiscardStillResetsDeadlineTimer)
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
    EXPECT_EQ(Com_IsRxTimedOut(1U), 0U);
    EXPECT_EQ(s_groupRxTOutCount, 0U);
}

// ------------------------------------------------------------
// SWS_Com_00536/00556（Com_CbkRxTOut）のグループ単位経路
// （Com_MainFunctionRx() の I-PDU 単位ループ、IPduId=1）
// ------------------------------------------------------------
TEST_F(Bsw_RxTimeoutChain_Test, ComMainFunction_OK_GroupRxTOutFiresAfterThresholdElapsed)
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
    EXPECT_EQ(Com_IsRxTimedOut(1U), 1U);
    EXPECT_EQ(s_groupRxTOutCount, 1U);
}

TEST_F(Bsw_RxTimeoutChain_Test, ComMainFunction_NG_GroupBeforeThreshold_DoesNotFire)
{
    /* 準備 (Arrange) */
    ReceiveOnceGroup();
    FakeMillis_Value = 400UL;

    /* 実行 (Act) */
    Com_MainFunctionRx();

    /* 評価 (Assert) */
    EXPECT_EQ(Com_IsRxTimedOut(1U), 0U);
    EXPECT_EQ(s_groupRxTOutCount, 0U);
}

TEST_F(Bsw_RxTimeoutChain_Test, ComMainFunction_NG_MisconfiguredGroupMemberDoesNotDoubleFire)
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

// ------------------------------------------------------------
// セグメント②: Com_ReceiveSignal() ─ フラグの続きから RxDataTimeoutAction 適用まで
// ------------------------------------------------------------
TEST_F(Bsw_RxTimeoutChain_Test, ComReceiveSignal_OK_SubstitutesValueAfterTimeout)
{
    /* 準備 (Arrange): セグメント①の終端状態（Com_SigTimedOut が立った状態）を用意する */
    ReceiveOnce(0x1234U);
    FakeMillis_Value = 600UL;
    Com_MainFunctionRx();
    ASSERT_EQ(Com_Test_GetSigTimedOut(0U), 1U);

    /* 実行 (Act) */
    uint16_t value = 0U;
    uint8 ret = Com_ReceiveSignal(0U, &value);

    /* 評価 (Assert): 実受信値(0x1234)ではなく TimeoutSubstitutionValue が返る */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(value, 0xFFFFU);
}

TEST_F(Bsw_RxTimeoutChain_Test, ComReceiveSignal_NG_BeforeTimeout_ReturnsLastReceivedValue)
{
    /* 準備 (Arrange): 受信直後、まだタイムアウトしきい値に達していない */
    ReceiveOnce(0x1234U);
    FakeMillis_Value = 400UL;
    Com_MainFunctionRx();
    ASSERT_EQ(Com_Test_GetSigTimedOut(0U), 0U);

    /* 実行 (Act) */
    uint16_t value = 0U;
    uint8 ret = Com_ReceiveSignal(0U, &value);

    /* 評価 (Assert): 実受信値がそのまま返る（置換されない） */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(value, 0x1234U);
}

// -----------------------------------------------------------------------
// Com_IpduGroupStart/Stop の RX 側検証（本番 EngineInfo/AbsInfo が
// COM_IPDU_GROUP_NONE から実グループ COM_IPDU_GROUP_SENSOR_RX へ移行した
// ことに伴う回帰、2026-08）。専用の最小 Com 設定・フィクスチャを別に用意
// する（上の kTestComRxTimeoutConfig は COM_RX_IPDU_MAX いっぱいの3本を
// 既に使い切っており、COM_RX_IPDU_MAX は Com.c が Com_Cfg.h の
// COM_RX_IPDU_COUNT（本番値、native_chain 環境全体で共有される固定サイズ）
// にそのまま連動しているため、4本目を追加すると Com_RxLastMs[] 等の
// 内部状態配列に範囲外アクセスしてしまう。IPduId=0 を再利用するのは
// 別の Com_Init() 呼び出し（このフィクスチャの SetUp()）で毎回状態が
// 作り直されるため安全）。
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
    // 省略（C の集成体初期化で NULL 埋め）。RxTOutCbk 自体は非デフォルト値
    // が必要なため、それより手前の未使用フィールド（TxTransformCbk〜
    // TxTOutCbk）は kTestRxGroupedNonGroupIPdu のように省略できない
    // （位置初期化では途中のフィールドだけ飛ばせないため）。
};

// IPduId=1: 本番 EngineInfo と同じ形（非 Signal Group、シグナル単位の
// RxDataTimeoutAction=SUBSTITUTE + RxTOutCbk）を再現する。上の
// kTestRxGroupedIPdu（IPduId=0、Signal Group、I-PDU 単位 RxTOutCbk）だけでは
// AbsInfo 寄りの経路しか検証できておらず、Com_MainFunctionRx() のシグナル単位
// ループ（Com.c）・Com_ReceiveSignal() の SUBSTITUTE 分岐がグループ停止中に
// 正しく抑制されることを検証できていなかった（/code-review で指摘）。
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

class Bsw_RxIpduGroupChain_Test : public ::testing::Test
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

TEST_F(Bsw_RxIpduGroupChain_Test, ComMainFunction_NG_StoppedGroupedIPduNeverTimesOutRegardlessOfElapsed)
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

TEST_F(Bsw_RxIpduGroupChain_Test, ComIpduGroupStart_OK_GroupedIPduBeginsMonitoringAfterExplicitStart)
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

TEST_F(Bsw_RxIpduGroupChain_Test, ComIpduGroupStop_OK_StoppingAgainSuppressesTimeoutEvenAfterElapsed)
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

// ------------------------------------------------------------
// 上の3件は IPduId=0（Signal Group、I-PDU 単位 RxTOutCbk、AbsInfo 寄りの形）
// のみを検証していた。本番 EngineInfo と同じ形（非 Signal Group、シグナル
// 単位の RxDataTimeoutAction=SUBSTITUTE + RxTOutCbk）である IPduId=1 でも
// 同じ抑制が効くことを確認する（/code-review で指摘されたカバレッジの
// 隙間の是正）。
// ------------------------------------------------------------
TEST_F(Bsw_RxIpduGroupChain_Test, ComIpduGroupStop_OK_NonGroupSignalFreezesInsteadOfSubstitutingWhileStopped)
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
     * ことも意味する（意図的な停止期間中は当然の仕様）。 */
    EXPECT_EQ(s_nonGroupRxTOutCount, 0U);
    uint16_t value = 0U;
    uint8 ret = Com_ReceiveSignal(0U, &value);
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(value, 0x1234U);  // SUBSTITUTE(0xFFFF) にはならない
}

TEST_F(Bsw_RxIpduGroupChain_Test, ComIpduGroupStart_OK_NonGroupSignalTimesOutAndSubstitutesAfterExplicitStart)
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
// Com_EnableReceptionDM/Com_DisableReceptionDM（SRS_Com_00192、2026-08 追加）。
// Com_IpduGroupStart/Stop（上記）とは独立した、より細かい制御軸である
// ことが本節の主張の核心: I-PDU Group 自体は起動済みのまま
// （Com_RxIndication() による受信処理・バッファ更新は継続する）、
// デッドライン監視の評価だけを個別に止められること。
// ------------------------------------------------------------
TEST_F(Bsw_RxIpduGroupChain_Test, ComEnableDisableReceptionDM_OK_TogglesFlagForMatchingGroupIPdusOnly)
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

TEST_F(Bsw_RxIpduGroupChain_Test, DisableReceptionDM_OK_SuppressesGroupTimeoutWhileIpduGroupStaysStarted)
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

TEST_F(Bsw_RxIpduGroupChain_Test, EnableReceptionDM_OK_ResetsTimerAndResumesDetectionWithoutImmediateFalseTimeout)
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

TEST_F(Bsw_RxIpduGroupChain_Test, DisableReceptionDM_NG_UnknownIpduGroupIdHasNoEffect)
{
    /* 準備 (Arrange): 不要。既定で有効なまま */

    /* 実行 (Act): どの I-PDU にも一致しない IpduGroupId を指定する */
    Com_DisableReceptionDM(static_cast<Com_IpduGroupIdType>(0xAAU));

    /* 評価 (Assert): 何も変化しない */
    EXPECT_EQ(Com_Test_GetRxDmEnabled(0U), 1U);
    EXPECT_EQ(Com_Test_GetRxDmEnabled(1U), 1U);
}

// ------------------------------------------------------------
// [SWS_Com_00534]（/code-review で発見）: 対象 I-PDU Group が1本でも
// TX I-PDU を含む場合、要求全体を黙って無視しなければならない。本番の
// COM_IPDU_GROUP_NONE は実際に RX/TX 混在グループ（Com_PBCfg.c 参照）の
// ため、これは仮説上の懸念ではなく実際に到達しうる呼び出しである。
// kTestRxMixedGroupIPdu(RX, IPduId=2)/kTestTxMixedGroupIPdu(TX, IPduId=0)
// が同じ COM_IPDU_GROUP_NONE に属する構成で検証する。
// ------------------------------------------------------------
TEST_F(Bsw_RxIpduGroupChain_Test, DisableReceptionDM_NG_MixedRxTxGroupIsIgnoredEntirely)
{
    /* 準備 (Arrange): 不要。既定で有効なまま */
    ASSERT_EQ(Com_Test_GetRxDmEnabled(2U), 1U);

    /* 実行 (Act): RX/TX 混在グループ（COM_IPDU_GROUP_NONE）を指定する */
    Com_DisableReceptionDM(COM_IPDU_GROUP_NONE);

    /* 評価 (Assert): 要求全体が無視され、RX メンバー（IPduId=2）のフラグも
     * 変化しない。 */
    EXPECT_EQ(Com_Test_GetRxDmEnabled(2U), 1U);
}

TEST_F(Bsw_RxIpduGroupChain_Test, EnableReceptionDM_NG_MixedRxTxGroupIsIgnoredEntirely)
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
