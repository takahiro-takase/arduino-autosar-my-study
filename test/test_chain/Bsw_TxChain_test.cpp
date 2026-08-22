/**
 * \file    Bsw_TxChain_test.cpp
 * \brief   README.md「Tx 処理（Com → PduR → CanIf → Can の順）」コールチェーンの
 *          単体テスト（GoogleTest / PlatformIO `[env:native_chain]`）。
 *
 * \details 本テストは単一モジュールの検証ではなく、README の以下のコールチェーン
 *          図をそのまま実行して確認するもの：
 *
 *              Com_SendSignal()/Com_SendSignalGroup()   ← ASW から呼ばれる
 *                ┊  (Com_TxPending 経由。次回 Com_MainFunction() まで非同期に待機)
 *              Com_MainFunction()
 *                → PduR_Transmit() → CanIf_Transmit() → Can_Write()
 *                  （SPI 送信完了までここで同期完了）
 *
 *          図中の「┊」（Com_TxPending というキュー経由の非同期の切れ目）を境に、
 *          テストを2つのセグメントへ分けている。キューで切れる箇所を無理に
 *          1つのテストで跨がず、そこで終わりとすることで、それぞれが
 *          「そこまでで何が保証されるか」を単独で・個別に実行可能な形で示す
 *          （`--gtest_filter=Bsw_TxChain_Test.ComSendSignal_*` 等で絞り込み可能）：
 *
 *            セグメント①: Com_SendSignal() が Com_TxPending をセットし、値を
 *                         TX バッファへ正しく pack することを検証し、そこで終わる
 *                         （Com_MainFunction() は呼ばない）。
 *            セグメント②: セグメント①の終端状態（Com_TxPending が立った状態）を
 *                         Arrange で用意し、Com_MainFunction() が
 *                         PduR_Transmit()→CanIf_Transmit()→Can_Write() と同期連鎖し、
 *                         最終的に Can_Hw（フェイク）へ正しい CAN ID/DLC/データで
 *                         送信要求が届くことまで検証する。
 *
 *          本番の `Com_PBCfg.c` 等は TxAckCbk/TxTransformCbk が `Rte_*` 関数を
 *          直接参照するため、リンクすると Rte.c 経由で Dem/WdgM/FiM... まで
 *          巨大な依存グラフを引き込んでしまう。ここではコールチェーンという
 *          「機構」の理解・検証に絞るため、テスト専用の最小 Com/PduR/CanIf
 *          設定を本ファイル内で定義し、Com.c/PduR.c/CanIf.c/Can.c は実体を
 *          リンクする（フェイクは `Can_Hw` 層のみ、
 *          `test/test_chain/Hal_Can_Hw_fake.c` を使う）。中心となる
 *          セグメント①②はコールチェーン全体を貫く IPduId=0（16bit シグナル
 *          1本）のみを使う。IPduId=1/2 は SWS_Com_00495（TMS 遷移時の
 *          無条件即時送信）専用の追加 I-PDU で、Com_MainFunction() より前の
 *          Com_SendSignal()/Com_SendSignalGroup() の境界内で完結する
 *          （PduR/CanIf 側にルーティングは設定していない）。IPduId=0
 *          （kTestTxIPdu）は ComTxModeNumberOfRepetitions（SWS_Com_00305）の
 *          検証も兼ねる（NumberOfRepetitions=2U/RepetitionPeriodMs=50U）。
 *          そのため本ファイルは `Hal_Millis_fake.h` で `millis()` を決定的に
 *          進められるようにしている（`SetUp()` で `FakeMillis_Reset()`）。
 *
 *          [env:native]（test/test_native/）は Can.c 単体を CanIf フェイクで
 *          隔離して検証しており、同じバイナリに CanIf.c の本物を混在させると
 *          シンボル多重定義になる。そのため本テストは env（＝ビルド
 *          ディレクトリ・バイナリ）そのものを分けた `[env:native_chain]`
 *          （このファイルが属する `test/test_chain/`）で実行する
 *          （`pio test -e native_chain`）。
 *
 *          CanIf.c は CanSM_RxIndication()/ControllerBusOff()/
 *          ControllerWakeup() をハードコードで呼ぶため、同じ `[env:native_chain]`
 *          は CanSM.c の実体もリンクしている（README「CAN コントローラの
 *          スリープ制御」のコールチェーンを検証する Bsw_SleepChain_test.cpp /
 *          Bsw_WakeupChain_test.cpp と同一バイナリ。CanSM が呼び返す
 *          ComM/Dem は境界としてフェイクに差し替える、Bsw_ComM_fake.h /
 *          Bsw_Dem_fake.h 冒頭コメント参照）。本ファイルの TX チェーンは
 *          CanSM の状態遷移に一切関与しないため、CanSM_Init() すら呼ばない
 *          （Com_MainFunction() は CanIf_RxIndication() を経由しないため）。
 */
#include <gtest/gtest.h>

extern "C" {
#include "Com.h"
#include "PduR.h"
#include "CanIf.h"
#include "Can.h"
#include "Can_Hw.h"
#include "Hal_Can_Hw_fake.h"
#include "Hal_Det_Hw_fake.h"
#include "Hal_Millis_fake.h"
}

namespace
{

// -----------------------------------------------------------------------
// テスト専用の最小 Com/PduR/CanIf 設定（本番の *_PBCfg.c は使わない。
// ファイル冒頭のコメント参照）。
// SignalId=0 (TX, 16bit BigEndian) 1本だけを持つ IPduId=0 の TX I-PDU。
// PduR は SrcPduId=0 を CanIfTxPduId=0 へ直結。CanIf の TxPduId=0 は
// CAN ID=0x100, DLC=2 で Can_Write(Hth=0, ...) を呼ぶ。
// -----------------------------------------------------------------------

const Com_SignalConfigType kTestSignal = {
    /* SignalId */                0U,
    /* Direction */                COM_SIGNAL_DIRECTION_TX,
    /* IPduId */                   0U,
    /* BitPosition */              0U,
    /* BitSize */                  16U,
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
    /* TimeoutNotificationCbk */   NULL,
    /* TxAckCbk */                 NULL,
    /* TxErrCbk */                 NULL
};

const Com_IPduConfigType kTestTxIPdu = {
    /* IPduId */           0U,
    /* DLC */              2U,
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
    /* IpduGroupId */      COM_IPDU_GROUP_NONE,
    /* RxIndicationCbk */  NULL,
    /* TxTransformCbk */   NULL,
    /* TxAckCbk */         NULL,
    /* TxErrCbk */         NULL,
    /* RxAckCbk */         NULL,
    /* NumberOfRepetitions */ 2U,  // ComTxModeNumberOfRepetitions（SWS_Com_00305）検証用
    /* RepetitionPeriodMs */  50U  // ComTxModeRepetitionPeriod
};

// -----------------------------------------------------------------------
// SWS_Com_00495（TMS 遷移時の無条件即時送信）検証用の 2 本目の I-PDU。
// SignalId=1 を持つ Signal Group（IPduId=1）で、唯一のメンバーは
// TmsContributor=1 かつ TransferProperty=PENDING（TMS には寄与するが、
// 単独では通常の送信トリガー Com_GroupTriggerPending を立てない）。
// WarningStatus の FaultLamp/AbsLamp（実運用設定、TmsContributor=1 かつ
// TRIGGERED_ON_CHANGE）とはあえて異なる組み合わせにすることで、「通常の
// 送信トリガー」を経由せずに「TMS 遷移そのもの」だけで送信が引き起こされる
// ことを検証する（Com_Notes.md「TMS 変化時の即時送信について」参照）。
// -----------------------------------------------------------------------
const Com_SignalConfigType kTestTmsPendingSignal = {
    /* SignalId */                1U,
    /* Direction */                COM_SIGNAL_DIRECTION_TX,
    /* IPduId */                   1U,
    /* BitPosition */              0U,
    /* BitSize */                  1U,
    /* Endian */                   COM_BIG_ENDIAN,
    /* InitValue */                0U,
    /* FilterAlgorithm */          COM_FILTER_ALWAYS,  // Signal Group メンバーのため未評価（Com_SendSignal() 参照）
    /* Mask */                     0x01U,
    /* FilterX */                  0U,
    /* FilterMin */                0U,
    /* FilterMax */                0U,
    /* FilterRejectCbk */          NULL,
    /* TmsContributor */           1U,
    /* UpdateBitContributor */     0U,
    /* TransferProperty */         COM_TRANSFER_PROPERTY_PENDING,
    /* RxDataTimeoutAction */      COM_RX_TIMEOUT_ACTION_NONE,
    /* TimeoutSubstitutionValue */ 0U,
    /* DataInvalidAction */        COM_DATA_INVALID_ACTION_NONE,
    /* InvalidValue */             0U,
    /* InvalidNotificationCbk */   NULL,
    /* FirstTimeoutMs */           0U,
    /* TimeoutMs */                0U,
    /* TimeoutNotificationCbk */   NULL,
    /* TxAckCbk */                 NULL,
    /* TxErrCbk */                 NULL
};

// SWS_Com_00468（Signal Group の TxAckCbk はグループ単位で 1 回だけ呼ばれる）
// 検証用のカウンタ付きコールバック。kTestTmsGroupIPdu に設定する。
static uint8_t s_groupTxAckCount = 0U;
static void TestGroupTxAckCbk(void) { s_groupTxAckCount++; }

const Com_IPduConfigType kTestTmsGroupIPdu = {
    /* IPduId */           1U,
    /* DLC */              1U,
    /* PduRId */           1U,   // 本テストは Com_MainFunction()/PduR まで進めないため未使用
    /* FirstTimeoutMs */   0U,
    /* TimeoutMs */        0U,
    /* IsSignalGroup */    1U,
    /* TxModeMode */       COM_TX_MODE_DIRECT,
    /* TxPeriodMs */       0U,
    /* TxModeModeTrue */   COM_TX_MODE_DIRECT,
    /* TxPeriodMsTrue */   0U,
    /* MinDelayMs */       0U,
    /* UpdateBitPosition */ 7U,  // bit0 は kTestTmsPendingSignal が使うため独立したビットにする
    /* IpduGroupId */      COM_IPDU_GROUP_NONE,
    /* RxIndicationCbk */  NULL,
    /* TxTransformCbk */   NULL,
    /* TxAckCbk */         TestGroupTxAckCbk
};

// -----------------------------------------------------------------------
// SWS_Com_00495 の非 Signal Group 側経路（Com_SendSignal() 内の tmsChanged
// 分岐）用。本番の Com_PBCfg.c では非 Signal Group シグナルに
// TmsContributor=1 を設定した例が無く未検証のままだったため
// （/code-review で指摘）、MeterStatus のように複数シグナルが 1 つの
// 非 Signal Group I-PDU を共有する構成を模した最小ケースを追加する。
//
// SignalId=4（TmsContributor=1、InitValue=1）は TMS の条件を初期値から
// 満たしているが、Com_Init() は Com_TmsState[] を一律 0 にするだけで
// Com_RecalcTms() を呼ばない（Com_IpduGroupStart() とは異なる）ため、
// バッファ内容（TMS=true 相当）と Com_TmsState（false のまま）が
// Init 直後から乖離している。SignalId=5（TmsContributor=0、他方の
// シグナル）を送ると、Com_RecalcTms() は「呼ばれたシグナル」ではなく
// 「そのシグナルが属する I-PDU 全体」を毎回スキャンするため、この乖離が
// そこで初めて検出されて tmsChanged=true になる——という経路を使うことで、
// SignalId=5 自身の ComFilterAlgorithm 判定（passesFilter）を独立に
// false にしたまま、OR 経路（SWS_Com_00495）だけで送信要求が立つことを
// 検証できる。
// -----------------------------------------------------------------------
const Com_SignalConfigType kTestNonGroupTmsContributorSignal = {
    /* SignalId */                4U,
    /* Direction */                COM_SIGNAL_DIRECTION_TX,
    /* IPduId */                   2U,
    /* BitPosition */              0U,
    /* BitSize */                  1U,
    /* Endian */                   COM_BIG_ENDIAN,
    /* InitValue */                1U,  // TMS 条件 (value & Mask) != FilterX を起動時から満たす
    /* FilterAlgorithm */          COM_FILTER_ALWAYS,
    /* Mask */                     0x01U,
    /* FilterX */                  0U,
    /* FilterMin */                0U,
    /* FilterMax */                0U,
    /* FilterRejectCbk */          NULL,
    /* TmsContributor */           1U,
    /* UpdateBitContributor */     0U,
    /* TransferProperty */         COM_TRANSFER_PROPERTY_PENDING,
    /* RxDataTimeoutAction */      COM_RX_TIMEOUT_ACTION_NONE,
    /* TimeoutSubstitutionValue */ 0U,
    /* DataInvalidAction */        COM_DATA_INVALID_ACTION_NONE,
    /* InvalidValue */             0U,
    /* InvalidNotificationCbk */   NULL,
    /* FirstTimeoutMs */           0U,
    /* TimeoutMs */                0U,
    /* TimeoutNotificationCbk */   NULL,
    /* TxAckCbk */                 NULL,
    /* TxErrCbk */                 NULL
};

const Com_SignalConfigType kTestNonGroupTmsCalledSignal = {
    /* SignalId */                5U,
    /* Direction */                COM_SIGNAL_DIRECTION_TX,
    /* IPduId */                   2U,
    /* BitPosition */              1U,  // kTestNonGroupTmsContributorSignal の bit0 とは別ビット
    /* BitSize */                  1U,
    /* Endian */                   COM_BIG_ENDIAN,
    /* InitValue */                0U,
    /* FilterAlgorithm */          COM_FILTER_MASKED_NEW_DIFFERS_MASKED_OLD,
    /* Mask */                     0x01U,
    /* FilterX */                  0U,
    /* FilterMin */                0U,
    /* FilterMax */                0U,
    /* FilterRejectCbk */          NULL,
    /* TmsContributor */           0U,  // TMS には寄与しない（呼び出し対象のシグナルのみ）
    /* UpdateBitContributor */     0U,
    /* TransferProperty */         COM_TRANSFER_PROPERTY_PENDING,
    /* RxDataTimeoutAction */      COM_RX_TIMEOUT_ACTION_NONE,
    /* TimeoutSubstitutionValue */ 0U,
    /* DataInvalidAction */        COM_DATA_INVALID_ACTION_NONE,
    /* InvalidValue */             0U,
    /* InvalidNotificationCbk */   NULL,
    /* FirstTimeoutMs */           0U,
    /* TimeoutMs */                0U,
    /* TimeoutNotificationCbk */   NULL,
    /* TxAckCbk */                 NULL,
    /* TxErrCbk */                 NULL
};

const Com_IPduConfigType kTestNonGroupTmsIPdu = {
    /* IPduId */           2U,
    /* DLC */              1U,
    /* PduRId */           2U,   // 本テストは Com_MainFunction()/PduR まで進めないため未使用
    /* FirstTimeoutMs */   0U,
    /* TimeoutMs */        0U,
    /* IsSignalGroup */    0U,
    /* TxModeMode */       COM_TX_MODE_DIRECT,
    /* TxPeriodMs */       0U,
    /* TxModeModeTrue */   COM_TX_MODE_DIRECT,
    /* TxPeriodMsTrue */   0U,
    /* MinDelayMs */       0U,
    /* UpdateBitPosition */ 0xFFU,
    /* IpduGroupId */      COM_IPDU_GROUP_NONE,
    /* RxIndicationCbk */  NULL,
    /* TxTransformCbk */   NULL
};

// -----------------------------------------------------------------------
// SWS_Com_00491（Signal Group の TxErrCbk はグループ単位で 1 回だけ呼ばれる）
// 検証用。Com_IpduGroupStop() が TxErrCbk を発火するのは「送信済み・未確認の
// まま所属 I-PDU Group が停止された」場合のみのため、COM_IPDU_GROUP_NONE
// ではなく実際に停止可能なテスト専用 IpduGroupId を割り当てる
// （kTestStoppableGroupId、本番の Com_Cfg.h の値とは無関係なテストローカル値）。
// -----------------------------------------------------------------------
static const Com_IpduGroupIdType kTestStoppableGroupId = 5U;

static uint8_t s_groupTxErrCount = 0U;
static void TestGroupTxErrCbk(void) { s_groupTxErrCount++; }

const Com_IPduConfigType kTestErrGroupIPdu = {
    /* IPduId */           3U,
    /* DLC */              1U,
    /* PduRId */           3U,   // 本テストは Com_MainFunction()/PduR まで進めないため未使用
    /* FirstTimeoutMs */   0U,
    /* TimeoutMs */        0U,
    /* IsSignalGroup */    1U,
    /* TxModeMode */       COM_TX_MODE_DIRECT,
    /* TxPeriodMs */       0U,
    /* TxModeModeTrue */   COM_TX_MODE_DIRECT,
    /* TxPeriodMsTrue */   0U,
    /* MinDelayMs */       0U,
    /* UpdateBitPosition */ 0xFFU,
    /* IpduGroupId */      kTestStoppableGroupId,
    /* RxIndicationCbk */  NULL,
    /* TxTransformCbk */   NULL,
    /* TxAckCbk */         NULL,
    /* TxErrCbk */         TestGroupTxErrCbk
};

const Com_SignalConfigType kTestSignals[] = {
    kTestSignal, kTestTmsPendingSignal,
    kTestNonGroupTmsContributorSignal, kTestNonGroupTmsCalledSignal
};
const Com_IPduConfigType   kTestTxIPdus[] = {
    kTestTxIPdu, kTestTmsGroupIPdu, kTestNonGroupTmsIPdu, kTestErrGroupIPdu
};

const Com_ConfigType kTestComConfig = {
    /* RxIPdus */       NULL,
    /* RxIPduCount */   0U,
    /* TxIPdus */       kTestTxIPdus,
    /* TxIPduCount */   4U,
    /* Signals */       kTestSignals,
    /* SignalCount */   4U,
    /* GwMappings */    NULL,
    /* GwMappingCount */ 0U
};

const PduR_TxRoutingPathType kTestPduRTxPath = {
    /* SrcPduId */             0U,
    /* CanIfTxPduId */         0U,
    /* ConfDestPduId */        0U,
    /* ConfFct */              NULL,
    /* TransmitOverrideFct */  NULL,
    /* TransmitOverrideId */   0U
};

const PduR_PBConfigType kTestPduRConfig = {
    /* RxPaths */     NULL,
    /* RxPathCount */ 0U,
    /* TxPaths */     &kTestPduRTxPath,
    /* TxPathCount */ 1U
};

const CanIf_TxPduConfigType kTestCanIfTxPdu = {
    /* UpperLayerTxPduId */ 0U,
    /* CanId */             0x100U,
    /* Dlc */               2U,
    /* Hth */               0U,
    /* TxConfirmFct */      NULL
};

const CanIf_ConfigType kTestCanIfConfig = {
    /* TxPduConfig */ &kTestCanIfTxPdu,
    /* TxPduCount */  1U,
    /* RxPduConfig */ NULL,
    /* RxPduCount */  0U
};

class Bsw_TxChain_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();  // Com_Init() が millis() を Com_TxLastSentMs[] へ
                              // 取り込むため、Com_Init() より前にリセットする
                              // （ComTxModeNumberOfRepetitions テストで
                              // FakeMillis_Value を進めて決定的に検証するため）
        FakeCanHw_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制

        canConfig.filter.filterId = 0x0220U;
        canConfig.filter.mask     = 0x1FFFU;
        canConfig.csPin           = 10U;
        canConfig.intPin          = 2U;
        canConfig.baudrate        = 500000U;
        canConfig.crystalFreq     = CAN_CRYSTAL_16MHZ;

        Can_Init(&canConfig);
        Can_SetControllerMode(0U, CAN_T_START);
        CanIf_Init(&kTestCanIfConfig);
        PduR_Init(&kTestPduRConfig);
        Com_Init(&kTestComConfig);
        s_groupTxAckCount = 0U;
        s_groupTxErrCount = 0U;

        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;  // DeInit() のログを抑制
        Com_DeInit();
        CanIf_DeInit();
    }

    Can_ConfigType canConfig;
};

// ------------------------------------------------------------
// セグメント①: Com_SendSignal() ─ Com_TxPending というキューで切れるまで
// ------------------------------------------------------------
TEST_F(Bsw_TxChain_Test, ComSendSignal_OK_SetsPendingAndPacksBuffer)
{
    /* 準備 (Arrange) */
    uint16_t value = 0x1234U;

    /* 実行 (Act) */
    uint8 ret = Com_SendSignal(0U, &value);

    /* 評価 (Assert) */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(Com_Test_GetTxPending(0U), 1U);
    const uint8* buf = Com_Test_GetTxBuffer(0U);
    ASSERT_NE(buf, nullptr);
    EXPECT_EQ(buf[0], 0x12U);  // BigEndian: bit0(MSB)側が byte[0]
    EXPECT_EQ(buf[1], 0x34U);
}

TEST_F(Bsw_TxChain_Test, ComSendSignal_NG_UnknownSignalId_DoesNotSetPending)
{
    /* 準備 (Arrange) */
    uint16_t value = 0x1234U;

    /* 実行 (Act) */
    uint8 ret = Com_SendSignal(99U, &value);  // 設定に存在しない SignalId

    /* 評価 (Assert) */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(Com_Test_GetTxPending(0U), 0U);
}

// ------------------------------------------------------------
// SWS_Com_00495: TMS 遷移時の無条件即時送信（Com_TmsState[] の変化）が、
// 通常の送信トリガー（ComTransferProperty=TRIGGERED_ON_CHANGE 由来の
// Com_GroupTriggerPending）を経由しなくても Com_TxPending を立てることを
// 検証する。kTestTmsPendingSignal は TmsContributor=1 かつ
// TransferProperty=PENDING のため、これ単体の変化は通常のトリガーには
// ならない（このテストの Arrange 部分自体が、対応前の実装ではここで失敗する
// ことを示す回帰テスト）。
// ------------------------------------------------------------
TEST_F(Bsw_TxChain_Test, TmsTransition_OK_TriggersImmediateSendWithoutGroupTrigger)
{
    /* 準備 (Arrange): TMS 寄与シグナルを 0(false)→1(true) へ変化させる。
     * TransferProperty=PENDING のため、この変化だけでは
     * Com_GroupTriggerPending は立たない。 */
    uint8_t value = 1U;
    Com_SendSignal(1U, &value);

    /* 実行 (Act): シャドウバッファを確定コミットし、TMS を再評価させる */
    Std_ReturnType ret = Com_SendSignalGroup(1U);

    /* 評価 (Assert) */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(Com_Test_GetTxPending(1U), 1U);  // TMS 遷移のみで即時送信要求が立つ

    /* update-bit（bit7）は「値が実際に更新されたか」を示すものであり、
     * TMS 遷移そのもの（PENDING メンバーの変化）とは独立した判断軸のため、
     * このケースではセットされない。
     * ビット番号はネットワーク順（bit0 = byte[0] の MSB=0x80、
     * bit7 = byte[0] の LSB=0x01。Com_PackSignal() 参照）。 */
    const uint8* buf = Com_Test_GetTxBuffer(1U);
    ASSERT_NE(buf, nullptr);
    EXPECT_EQ(buf[0] & 0x80U, 0x80U);  // シグナル値自体（bit0）はコミットされている
    EXPECT_EQ(buf[0] & 0x01U, 0x00U);  // update-bit (bit7) は立たない
}

TEST_F(Bsw_TxChain_Test, TmsUnchanged_OK_DoesNotTriggerSendWithoutGroupTrigger)
{
    /* 準備 (Arrange): 初期状態（TMS=false）から変化させない
     * （0 のままシグナルグループをコミットする） */
    uint8_t value = 0U;
    Com_SendSignal(1U, &value);

    /* 実行 (Act) */
    Std_ReturnType ret = Com_SendSignalGroup(1U);

    /* 評価 (Assert): TMS が変化せず、通常トリガーも立たないため送信要求は立たない */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(Com_Test_GetTxPending(1U), 0U);
}

// ------------------------------------------------------------
// SWS_Com_00495: 非 Signal Group 側（Com_SendSignal() 内の tmsChanged 分岐）
// の回帰テスト。上の2件は Signal Group 側（Com_SendSignalGroup()）のみを
// 検証しており、非 Signal Group 側は /code-review で「本番設定に
// TmsContributor=1 の非 Signal Group シグナルが無く未検証」と指摘された
// ギャップだった（コードコメント上は「現状の設定ではこの経路は通らない」と
// 正直に開示されていたが、テストでは未確認だった）。
//
// kTestNonGroupTmsContributorSignal（SignalId=4）は InitValue の時点で既に
// TMS 条件を満たしているが、Com_Init() は Com_TmsState[] を一律 0 にする
// だけで Com_RecalcTms() を呼ばないため、バッファ内容と Com_TmsState が
// Init 直後から乖離している。この乖離は、同じ I-PDU を共有する別の
// シグナル（SignalId=5、TMS には寄与しない）を送った瞬間に
// Com_RecalcTms()（呼ばれたシグナルではなく I-PDU 全体をスキャンする）に
// よって検出される。SignalId=5 自身の ComFilterAlgorithm 判定
// （passesFilter）は独立に false にしてあるため、送信要求が立つとすれば
// それは OR 経路（tmsChanged）だけによるものだと確実に切り分けられる。
// ------------------------------------------------------------
TEST_F(Bsw_TxChain_Test, NonGroupTmsTransition_OK_TriggersImmediateSendEvenWhenCalledSignalFilterFails)
{
    /* 準備 (Arrange): 追加の準備は不要。SetUp() 内の Com_Init() の時点で
     * 既に上記の乖離状態（バッファ上は TMS=true 相当、Com_TmsState は
     * false のまま）が成立している。 */

    /* 実行 (Act): SignalId=5 へ InitValue と同じ値を送る
     * （自身の ComFilterAlgorithm=MASKED_NEW_DIFFERS_MASKED_OLD により
     * passesFilter は false になる）。 */
    uint8_t value = 0U;
    Com_SendSignal(5U, &value);

    /* 評価 (Assert): SignalId=5 自身は「送信不要」と判定されたにも
     * かかわらず、SignalId=4 由来の TMS 遷移検出（tmsChanged）により
     * 送信要求が立つ。 */
    EXPECT_EQ(Com_Test_GetTxPending(2U), 1U);
}

// ------------------------------------------------------------
// SWS_Com_00468: Signal Group の TxAckCbk はグループ単位で 1 回だけ呼ばれる
// （Rte_COMCbkTAck_<sg> 相当）ことの回帰テスト。当初の実装はメンバーシグナル
// 単位で走査していたため、WarningStatus のような 3 メンバー構成では最大 3 回
// 呼ばれてしまう簡略化だった。kTestTmsGroupIPdu（IPduId=1、Signal Group、
// TxAckCbk=TestGroupTxAckCbk）に対して Com_TxConfirmation() を直接呼び、
// カウンタが厳密に 1 であることを確認する（本テストは Com_MainFunction()/
// PduR を経由しないため、Com_TxConfirmation() を直接呼ぶ形で検証する）。
// ------------------------------------------------------------
TEST_F(Bsw_TxChain_Test, TxConfirmation_OK_CallsSignalGroupAckCbkExactlyOnce)
{
    /* 準備 (Arrange): 不要（SetUp() で s_groupTxAckCount は 0 にリセット済み） */

    /* 実行 (Act): IPduId=1（Signal Group）の送信成功を通知する */
    Com_TxConfirmation(1U, E_OK);

    /* 評価 (Assert): メンバー数（このテストでは 1）に関わらず、
     * グループ単位で厳密に 1 回だけ呼ばれる */
    EXPECT_EQ(s_groupTxAckCount, 1U);
}

TEST_F(Bsw_TxChain_Test, TxConfirmation_NG_NonGroupIPduDoesNotCallGroupAckCbk)
{
    /* 準備 (Arrange): 不要。IPduId=0 は非 Signal Group（kTestTxIPdu） */

    /* 実行 (Act) */
    Com_TxConfirmation(0U, E_OK);

    /* 評価 (Assert): 無関係な Signal Group（IPduId=1）の TxAckCbk は呼ばれない */
    EXPECT_EQ(s_groupTxAckCount, 0U);
}

// ------------------------------------------------------------
// SWS_Com_00491: Signal Group の TxErrCbk はグループ単位で 1 回だけ呼ばれる
// （Rte_COMCbkTErr_<sg> 相当）ことの回帰テスト。TxAckCbk と全く同じ理由で
// 当初はメンバーシグナル単位の走査だった。本番の Com_PBCfg.c では
// WarningStatus（唯一の Signal Group）が IpduGroupId=COM_IPDU_GROUP_NONE
// （常時有効）のため Com_IpduGroupStop() の対象にならず、実機では発動しない
// （docs/modules/Com_Notes.md 参照）。このユニットテストのみが検証手段となる。
// ------------------------------------------------------------
TEST_F(Bsw_TxChain_Test, IpduGroupStop_OK_CallsSignalGroupErrCbkExactlyOnceWhenUnconfirmed)
{
    /* 準備 (Arrange): 「PduR へは渡した（実送信済み）が Com_TxConfirmation()
     * がまだ届いていない」状態を直接作る（Com_MainFunction()/PduR を経由
     * しないための test-only setter、Com.h 参照）。 */
    Com_Test_SetTxConfPending(3U, 1U);

    /* 実行 (Act): kTestErrGroupIPdu（IPduId=3）が所属する I-PDU Group を
     * 未確認のまま停止する。 */
    Com_IpduGroupStop(kTestStoppableGroupId);

    /* 評価 (Assert): メンバー数に関わらず、グループ単位で厳密に 1 回だけ
     * 呼ばれる。 */
    EXPECT_EQ(s_groupTxErrCount, 1U);
}

TEST_F(Bsw_TxChain_Test, IpduGroupStop_NG_DoesNotCallErrCbkWhenAlreadyConfirmed)
{
    /* 準備 (Arrange): 不要。「送信済み・未確認」状態を一切作らない
     * （Com_TxConfPending は Com_Init() で 0 のまま）。 */

    /* 実行 (Act) */
    Com_IpduGroupStop(kTestStoppableGroupId);

    /* 評価 (Assert): 未確認の送信が無いため TxErrCbk は呼ばれない。 */
    EXPECT_EQ(s_groupTxErrCount, 0U);
}

// ------------------------------------------------------------
// セグメント②: Com_MainFunction() ─ キューの続きから Can_Hw まで
// ------------------------------------------------------------
TEST_F(Bsw_TxChain_Test, ComMainFunction_OK_DrivesChainToCanHwSend)
{
    /* 準備 (Arrange): セグメント①の終端状態（Com_TxPending が立った状態）を用意する */
    uint16_t value = 0x1234U;
    Com_SendSignal(0U, &value);
    ASSERT_EQ(Com_Test_GetTxPending(0U), 1U);

    /* 実行 (Act) */
    Com_MainFunction();

    /* 評価 (Assert) */
    EXPECT_EQ(Com_Test_GetTxPending(0U), 0U);  // 送信要求が消費された
    EXPECT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x100U);   // CanIf_TxPduConfigType.CanId
    EXPECT_EQ(FakeCanHw_LastSendDlc, 2U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x12U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x34U);
}

TEST_F(Bsw_TxChain_Test, ComMainFunction_NG_NothingPending_DoesNotReachCanHw)
{
    /* 準備 (Arrange): Com_SendSignal() を呼ばない（Com_TxPending が立っていない） */

    /* 実行 (Act) */
    Com_MainFunction();

    /* 評価 (Assert) */
    EXPECT_EQ(FakeCanHw_SendCount, 0U);
}

// ------------------------------------------------------------
// SWS_Com_00305: ComTxModeNumberOfRepetitions（変化時送信の冗長再送）。
// kTestTxIPdu（IPduId=0）は NumberOfRepetitions=2U/RepetitionPeriodMs=50U を
// 持つため、初回送信 + 2 回の再送 = 計3回送信されて止まることを検証する。
// 残り回数の減算は Com_TxConfirmation() 到達時ではなく Com_MainFunction() の
// dispatch 時点で行う設計のため（Com.c のコメント参照。TX 確認は
// Can_MainFunction_Write() という別タスク経由で非同期に届き、
// Com_MainFunction() 単独では待てないため）、これらのテストは
// Com_TxConfirmation() を一切呼ばない。
// ------------------------------------------------------------
TEST_F(Bsw_TxChain_Test, RepetitionSequence_OK_FiresConfiguredNumberOfRepeatsThenStops)
{
    /* 準備 (Arrange) */
    uint16_t value = 0x1234U;
    Com_SendSignal(0U, &value);

    /* 実行 (Act) + 評価 (Assert): 初回送信 */
    Com_MainFunction();
    EXPECT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(Com_Test_GetTxRepeatsRemaining(0U), 2U);  // 初回はまだ減らない

    /* 1 回目の再送（RepetitionPeriodMs=50 経過後） */
    FakeMillis_Value += 50U;
    Com_MainFunction();
    EXPECT_EQ(FakeCanHw_SendCount, 2U);
    EXPECT_EQ(Com_Test_GetTxRepeatsRemaining(0U), 1U);

    /* 2 回目の再送（NumberOfRepetitions=2 を使い切る） */
    FakeMillis_Value += 50U;
    Com_MainFunction();
    EXPECT_EQ(FakeCanHw_SendCount, 3U);
    EXPECT_EQ(Com_Test_GetTxRepeatsRemaining(0U), 0U);

    /* 再送を使い切った後は、さらに周期が経過しても送信されない */
    FakeMillis_Value += 50U;
    Com_MainFunction();
    EXPECT_EQ(FakeCanHw_SendCount, 3U);
}

// /code-review で指摘されたオフバイワンの回帰テスト: Com_Init() 時点の
// Com_TxLastSentMs（=0）から、RepetitionPeriodMs(50) を優に超える時間が
// 経過してから初めて変化イベントが来ると（実機でも、起動直後よりだいぶ
// 後になって初めて変化が来れば必ず起こるごく普通の状況）、その「初回」
// 送信の時点で偶然 repeatDue も真になり得る。これを再送1回分として
// 誤カウントすると、計3回ではなく2回で止まってしまう不具合があった
// （上のテストは FakeMillis_Reset() 直後に送信するため elapsed=0 の
// ケースしか通らず、この不具合を検出できていなかった）。
TEST_F(Bsw_TxChain_Test, RepetitionSequence_OK_InitialSendDoesNotConsumeRepeatBudgetEvenWhenElapsedAlreadyExceedsPeriod)
{
    /* 準備 (Arrange): RepetitionPeriodMs(50) を優に超える時間が経過した
     * 状態を作ってから、初めて送信要求を出す。 */
    FakeMillis_Value = 10000U;
    uint16_t value = 0x1234U;
    Com_SendSignal(0U, &value);

    /* 実行 (Act) + 評価 (Assert): 初回送信では残り回数が減らない
     * （elapsed が RepetitionPeriodMs を超えていても、changeDue 由来の
     * 送信は再送としてカウントしない）。計3回まで正常に続くことは
     * RepetitionSequence_OK_FiresConfiguredNumberOfRepeatsThenStops が
     * 既に検証しているため、ここでは初回分の回帰確認に絞る。 */
    Com_MainFunction();
    EXPECT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(Com_Test_GetTxRepeatsRemaining(0U), 2U);

    /* 以降も正常に再送が続くことだけ 1 回分だけ確認する */
    FakeMillis_Value += 50U;
    Com_MainFunction();
    EXPECT_EQ(FakeCanHw_SendCount, 2U);
    EXPECT_EQ(Com_Test_GetTxRepeatsRemaining(0U), 1U);
}

// CommunicationControl (UDS 0x28) による送信抑制中は、残り再送回数を
// 空費させない（/code-review で指摘: 抑制中も Com_TxLastSentMs が更新され
// 続けるため、抑制解除を待たずに repeatDue が周期的に真になり得るが、
// Com_DoTransmit() 自体は呼ばれない。ここで残り回数まで減らしてしまうと、
// 抑制解除後に本来送るべき再送が1本も残っていない、という事態になりかねない）。
TEST_F(Bsw_TxChain_Test, RepetitionSequence_OK_DoesNotConsumeBudgetWhileCommunicationControlDisabled)
{
    /* 準備 (Arrange): 初回送信を済ませたうえで送信を抑制する */
    uint16_t value = 0x1234U;
    Com_SendSignal(0U, &value);
    Com_MainFunction();
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    ASSERT_EQ(Com_Test_GetTxRepeatsRemaining(0U), 2U);
    Com_SetCommunicationEnabled(1U, 0U);  // RxEnabled=1, TxEnabled=0

    /* 実行 (Act): 抑制中に RepetitionPeriodMs を複数回分経過させる
     * （repeatDue 自体は周期的に真になり得るが、Com_TxEnabled==0 のため
     * Com_DoTransmit() には到達しない） */
    FakeMillis_Value += 50U;
    Com_MainFunction();
    FakeMillis_Value += 50U;
    Com_MainFunction();

    /* 評価 (Assert): 送信は1本も増えておらず、残り回数も空費されていない */
    EXPECT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(Com_Test_GetTxRepeatsRemaining(0U), 2U);

    /* 抑制解除後は、通常どおり残っていた再送が送信される */
    Com_SetCommunicationEnabled(1U, 1U);
    FakeMillis_Value += 50U;
    Com_MainFunction();
    EXPECT_EQ(FakeCanHw_SendCount, 2U);
    EXPECT_EQ(Com_Test_GetTxRepeatsRemaining(0U), 1U);
}

TEST_F(Bsw_TxChain_Test, RepetitionSequence_NG_DoesNotFireBeforePeriodElapsed)
{
    /* 準備 (Arrange): 初回送信を済ませておく */
    uint16_t value = 0x1234U;
    Com_SendSignal(0U, &value);
    Com_MainFunction();
    ASSERT_EQ(FakeCanHw_SendCount, 1U);

    /* 実行 (Act): RepetitionPeriodMs(50) 未満しか経過していない */
    FakeMillis_Value += 49U;
    Com_MainFunction();

    /* 評価 (Assert): 再送されない。残り回数も減らない */
    EXPECT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(Com_Test_GetTxRepeatsRemaining(0U), 2U);
}

TEST_F(Bsw_TxChain_Test, RepetitionSequence_OK_NewSendSignalRestartsSequence)
{
    /* 準備 (Arrange): 初回送信 + 1 回の再送を消費させる */
    uint16_t value = 0x1234U;
    Com_SendSignal(0U, &value);
    Com_MainFunction();
    FakeMillis_Value += 50U;
    Com_MainFunction();
    ASSERT_EQ(Com_Test_GetTxRepeatsRemaining(0U), 1U);

    /* 実行 (Act): 新たな送信要求（[SWS_Com_00279]、kTestSignal は
     * FilterAlgorithm=ALWAYS のため値の異同を問わず要求が通る） */
    uint16_t newValue = 0x5678U;
    Com_SendSignal(0U, &newValue);

    /* 評価 (Assert): 残り回数が NumberOfRepetitions=2 へ戻る
     * （進行中の再送シーケンスをキャンセルして再スタート） */
    EXPECT_EQ(Com_Test_GetTxRepeatsRemaining(0U), 2U);
}

TEST_F(Bsw_TxChain_Test, IpduGroupStop_OK_ClearsRepeatsRemaining)
{
    /* 準備 (Arrange): 再送シーケンス進行中の状態を、実際に NumberOfRepetitions
     * を設定した停止可能グループの I-PDU を新規に用意しなくても、test-only
     * setter で直接作る（kTestErrGroupIPdu/kTestStoppableGroupId を流用）。 */
    Com_Test_SetTxRepeatsRemaining(3U, 2U);

    /* 実行 (Act): [SWS_Com_00392] I-PDU Group の停止は再送シーケンスも
     * キャンセルする */
    Com_IpduGroupStop(kTestStoppableGroupId);

    /* 評価 (Assert) */
    EXPECT_EQ(Com_Test_GetTxRepeatsRemaining(3U), 0U);
}

}  // namespace
