/**
 * \file    Bsw_TxChain_test.cpp
 * \brief   README.md「Tx 処理（Com → PduR → CanIf → Can の順）」コールチェーンの
 *          単体テスト（GoogleTest / PlatformIO `[env:native_chain]`）。
 *
 * \details 本テストは単一モジュールの検証ではなく、README の以下のコールチェーン
 *          図をそのまま実行して確認するもの：
 *
 *              Com_SendSignal()/Com_SendSignalGroup()   ← ASW から呼ばれる
 *                ┊  (Com_TxPending 経由。次回 Com_MainFunctionTx() まで非同期に待機)
 *              Com_MainFunctionTx()
 *                → PduR_ComTransmit() → CanIf_Transmit() → Can_Write()
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
 *                         （Com_MainFunctionTx() は呼ばない）。
 *            セグメント②: セグメント①の終端状態（Com_TxPending が立った状態）を
 *                         Arrange で用意し、Com_MainFunctionTx() が
 *                         PduR_ComTransmit()→CanIf_Transmit()→Can_Write() と同期連鎖し、
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
 *          無条件即時送信）専用の追加 I-PDU で、Com_MainFunctionTx() より前の
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
 *          （Com_MainFunctionTx() は CanIf_RxIndication() を経由しないため）。
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

// SWS_Com_00878（TX 送信デッドライン監視、Com_CbkTxTOut）検証用のカウンタ
// 付きコールバック。kTestSignal（非 Signal Group、IPduId=0）に設定する
// （Com_InvokeTxNotification() は非 Signal Group の I-PDU ではシグナル単位の
// TxTOutCbk を配送する。Com_IPduConfigType.TxTOutCbk は Signal Group 専用）。
static uint8_t s_txTOutCount = 0U;
static void TestTxTOutCbk(void) { s_txTOutCount++; }

// Com_TxIpduCallout（SWS_Com_00346、TX I-PDU 単位のフィルタリングフック）
// 検証用。kTestTxIPdu（IPduId=0）に設定する。s_txCalloutAccept で戻り値を
// 切り替えられるトグル式（Bsw_RxChain_test.cpp の TestRxIpduCallout と対称）。
static uint8_t s_txCalloutAccept      = 1U;
static uint8_t s_txCalloutInvokeCount = 0U;
static uint8_t s_txCalloutLastByte0   = 0U;
static boolean TestTxIpduCallout(const uint8* SduDataPtr, uint8 SduLength)
{
    s_txCalloutInvokeCount++;
    s_txCalloutLastByte0 = (SduLength >= 1U) ? SduDataPtr[0] : 0xFFU;
    return s_txCalloutAccept != 0U;
}

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
    /* InvalidValue */             0xBEEFU, // Com_InvalidateSignal（SWS_Com_00099）検証用
    /* InvalidNotificationCbk */   NULL,
    /* FirstTimeoutMs */           0U,
    /* TimeoutMs */                0U,
    /* RxTOutCbk */                NULL,
    /* TxAckCbk */                 NULL,
    /* TxErrCbk */                 NULL,
    /* RxAckCbk */                 NULL,
    /* TxTOutCbk */                TestTxTOutCbk,
    /* InvalidValueConfigured */   1U
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
    /* NumberOfRepetitions */ 2U,   // ComTxModeNumberOfRepetitions（SWS_Com_00305）検証用
    /* RepetitionPeriodMs */  50U,  // ComTxModeRepetitionPeriod
    /* TxFirstTimeoutMs */    1000U, // Com_CbkTxTOut（SWS_Com_00878）検証用
    /* TxTimeoutMs */         500U,
    /* TxTOutCbk */           NULL, // 非 Signal Group のため未使用。
                              // 実際のコールバックは kTestSignal.TxTOutCbk 側
    /* RxTOutCbk */           NULL, // Signal Group 専用のため未使用
    /* RxIpduCalloutCbk */    NULL, // RX 専用のため未使用
    /* TxIpduCalloutCbk */    TestTxIpduCallout // SWS_Com_00346 検証用
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
    /* InvalidValue */             1U, // Com_InvalidateSignalGroup（SWS_Com_00557）検証用
    /* InvalidNotificationCbk */   NULL,
    /* FirstTimeoutMs */           0U,
    /* TimeoutMs */                0U,
    /* RxTOutCbk */                NULL,
    /* TxAckCbk */                 NULL,
    /* TxErrCbk */                 NULL,
    /* RxAckCbk */                 NULL,
    /* TxTOutCbk */                NULL,
    /* InvalidValueConfigured */   1U
};

// SWS_Com_00468（Signal Group の TxAckCbk はグループ単位で 1 回だけ呼ばれる）
// 検証用のカウンタ付きコールバック。kTestTmsGroupIPdu に設定する。
static uint8_t s_groupTxAckCount = 0U;
static void TestGroupTxAckCbk(void) { s_groupTxAckCount++; }

const Com_IPduConfigType kTestTmsGroupIPdu = {
    /* IPduId */           1U,
    /* DLC */              1U,
    /* PduRId */           1U,   // 本テストは Com_MainFunctionTx()/PduR まで進めないため未使用
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
    /* RxTOutCbk */                NULL,
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
    /* RxTOutCbk */                NULL,
    /* TxAckCbk */                 NULL,
    /* TxErrCbk */                 NULL
};

const Com_IPduConfigType kTestNonGroupTmsIPdu = {
    /* IPduId */           2U,
    /* DLC */              1U,
    /* PduRId */           2U,   // 本テストは Com_MainFunctionTx()/PduR まで進めないため未使用
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

// SWS_Com_00878（TX 送信デッドライン監視）のグループ単位発火・
// Com_IpduGroupStop() との二重発火防止を検証するためのカウンタ付き
// コールバック。kTestErrGroupIPdu（Signal Group、停止可能グループ）に設定する。
static uint8_t s_groupTxTOutCount = 0U;
static void TestGroupTxTOutCbk(void) { s_groupTxTOutCount++; }

const Com_IPduConfigType kTestErrGroupIPdu = {
    /* IPduId */           3U,
    /* DLC */              1U,
    /* PduRId */           3U,   // kTestPduRConfig に対応する経路を登録していないため
                                 // 未登録（PduR_ComTransmit は経路なしで安全に E_NOT_OK を
                                 // 返す。Com_TriggerIPDUSend の MDT 検証用に実際に
                                 // ディスパッチさせるが、実 CAN 送信までは進めない）
    /* FirstTimeoutMs */   0U,
    /* TimeoutMs */        0U,
    /* IsSignalGroup */    1U,
    /* TxModeMode */       COM_TX_MODE_DIRECT,
    /* TxPeriodMs */       0U,
    /* TxModeModeTrue */   COM_TX_MODE_DIRECT,
    /* TxPeriodMsTrue */   0U,
    /* MinDelayMs */       100U,  // Com_TriggerIPDUSend の MDT 尊重（SWS_Com_00388）検証用
    /* UpdateBitPosition */ 0xFFU,
    /* IpduGroupId */      kTestStoppableGroupId,
    /* RxIndicationCbk */  NULL,
    /* TxTransformCbk */   NULL,
    /* TxAckCbk */         NULL,
    /* TxErrCbk */         TestGroupTxErrCbk,
    /* RxAckCbk */         NULL,
    /* NumberOfRepetitions */ 0U,
    /* RepetitionPeriodMs */  0U,
    /* TxFirstTimeoutMs */    100U,  // Com_CbkTxTOut（SWS_Com_00878）検証用
    /* TxTimeoutMs */          50U,
    /* TxTOutCbk */           TestGroupTxTOutCbk
};

// -----------------------------------------------------------------------
// Com_InvalidateSignal/Com_InvalidateSignalGroup（SWS_Com_00099/SWS_Com_00557
// 等、2026-08 追加）の Com_InvalidateSignalGroup 側 all-or-nothing 検証用。
// kTestErrGroupIPdu（IPduId=3、Signal Group）に新規メンバーを 2 本追加する
// （既存メンバーなしの I-PDU だったため、新規 I-PDU を増やさずに済む——
// COM_TX_IPDU_MAX は本ファイルの 4 本で既に上限のため、テスト用にこれ以上
// I-PDU を増やしてはいけない。feedback_test_chain_ipdu_id_ceiling 参照）。
// SignalId=6 は InvalidValueConfigured=1（設定済み）、SignalId=7 はあえて
// 未設定のままにし、「1本でも未設定なら全体を E_NOT_OK とする」ことの検証に使う。
// -----------------------------------------------------------------------
const Com_SignalConfigType kTestInvalidateGroupMemberA = {
    /* SignalId */                6U,
    /* Direction */                COM_SIGNAL_DIRECTION_TX,
    /* IPduId */                   3U,
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
    /* InvalidValue */             1U,
    /* InvalidNotificationCbk */   NULL,
    /* FirstTimeoutMs */           0U,
    /* TimeoutMs */                0U,
    /* RxTOutCbk */                NULL,
    /* TxAckCbk */                 NULL,
    /* TxErrCbk */                 NULL,
    /* RxAckCbk */                 NULL,
    /* TxTOutCbk */                NULL,
    /* InvalidValueConfigured */   1U
};

const Com_SignalConfigType kTestInvalidateGroupMemberB = {
    /* SignalId */                7U,
    /* Direction */                COM_SIGNAL_DIRECTION_TX,
    /* IPduId */                   3U,
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
    /* RxAckCbk */                 NULL,
    /* TxTOutCbk */                NULL,
    /* InvalidValueConfigured */   0U  // あえて未設定のまま（all-or-nothing 検証用）
};

// Com_InvalidateSignal() の Direction==TX ガード（/code-review 指摘、
// Com_InvalidateSignalGroup() 側は元々メンバー走査で Direction==TX を
// 見ていたことに対する非対称の是正）検証用。InvalidValueConfigured=1 を
// 誤って設定された RX シグナルという想定で、Direction チェックのみで
// Com_SendSignal() に到達せず拒否されることを確認する。
const Com_SignalConfigType kTestInvalidateRxSignal = {
    /* SignalId */                8U,
    /* Direction */                COM_SIGNAL_DIRECTION_RX,
    /* IPduId */                   0U,
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
    /* RxAckCbk */                 NULL,
    /* TxTOutCbk */                NULL,
    /* InvalidValueConfigured */   1U  // 誤設定を想定（Direction チェックが先に効くことの検証）
};

const Com_SignalConfigType kTestSignals[] = {
    kTestSignal, kTestTmsPendingSignal,
    kTestNonGroupTmsContributorSignal, kTestNonGroupTmsCalledSignal,
    kTestInvalidateGroupMemberA, kTestInvalidateGroupMemberB,
    kTestInvalidateRxSignal
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
    /* SignalCount */   7U,
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
        CanIf_Init(&kTestCanIfConfig);
        /* CanIf_Init() より前に Can_SetControllerMode() を直接呼ぶと、CanIf が
         * 追跡するコントローラ状態（CanIf_ControllerMode[]、CanIf.c 参照）が
         * Init() で CAN_CS_STOPPED に巻き戻され、実際の Can 側の状態
         * （CAN_CS_STARTED）と食い違ったままになる。本テストは CanSM を
         * 経由しないためこの食い違い自体は実害が無いが、CanIf_SetControllerMode()
         * 経由に統一しておく方が事故が起きない（/code-review 指摘）。 */
        CanIf_SetControllerMode(0U, CAN_CS_STARTED);
        /* 本テストは CanSM を経由しない（CanSM_Init() を呼ばない）ため、
         * CanIf_Init() 直後の既定値 CANIF_OFFLINE のままでは CanIf_Transmit()
         * が常に E_NOT_OK になってしまう。CanSM が FULL_COM 確立時に行う
         * CanIf_SetPduMode(CANIF_ONLINE) を代わりにここで行う
         * （2026-08 追加、CanIf.c 参照）。 */
        CanIf_SetPduMode(0U, CANIF_ONLINE);
        PduR_Init(&kTestPduRConfig);
        Com_Init(&kTestComConfig);
        s_groupTxAckCount = 0U;
        s_groupTxErrCount = 0U;
        s_txTOutCount      = 0U;
        s_groupTxTOutCount = 0U;
        s_txCalloutAccept      = 1U;
        s_txCalloutInvokeCount = 0U;
        s_txCalloutLastByte0   = 0U;

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
// Com_SendSignalGroupArray（SWS_Com_00851/00852、Com_ReceiveSignalGroupArray
// の送信側対）。kTestTmsGroupIPdu（IPduId=1、DLC=1、TMSシグナル=bit0、
// update-bit=bit7）を流用する。Com_SendSignal()/Com_SendSignalGroup() を
// 経由しない一括書き込みでも、TMS 再評価（Com_TxBuffer から直接読む）・
// update-bit セット・送信要求が正しく動くことを検証する。
// ------------------------------------------------------------
TEST_F(Bsw_TxChain_Test, SendSignalGroupArray_OK_WritesBufferTriggersSendAndSetsUpdateBit)
{
    /* 準備 (Arrange): TMS ビット(bit0=0x80)を立てた生バイト列 */
    uint8_t raw = 0x80U;

    /* 実行 (Act) */
    Std_ReturnType ret = Com_SendSignalGroupArray(1U, &raw);

    /* 評価 (Assert): 個々の Com_SendSignal() を経由しなくても
     * Com_RecalcTms() が Com_TxBuffer を直接読むため TMS 遷移が検出され、
     * 無条件で送信要求・update-bit セットが行われる
     * （Com_SendSignalGroup() は Com_GroupTriggerPending が立っていないと
     * update-bit をセットしないが、本関数は常にセットする——ドキュメント
     * コメント参照）。 */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(Com_Test_GetTxPending(1U), 1U);
    const uint8* buf = Com_Test_GetTxBuffer(1U);
    ASSERT_NE(buf, nullptr);
    EXPECT_EQ(buf[0], 0x81U);  // bit0(TMS, 書き込んだ値) + bit7(update-bit, 自動セット)
}

TEST_F(Bsw_TxChain_Test, SendSignalGroupArray_OK_AlwaysTriggersEvenWithoutChange)
{
    /* 準備 (Arrange): Init 直後の値（0x00、TMS=false のまま）と全く同じ
     * 内容を書き込む。上の TmsUnchanged_OK_DoesNotTriggerSendWithoutGroupTrigger
     * （通常経路 Com_SendSignal()+Com_SendSignalGroup()）では、この
     * 「変化なし」ケースは送信要求を立てない。 */
    uint8_t raw = 0x00U;

    /* 実行 (Act) */
    Std_ReturnType ret = Com_SendSignalGroupArray(1U, &raw);

    /* 評価 (Assert): 本関数は個々のシグナルの変化検知を経由しないため、
     * 値が変化していなくても常に送信要求が立つ（ドキュメントコメント
     * 「呼ばれるたびに常に『新しいデータがある』ものとして扱う」の
     * とおり。通常経路との対比が本テストの主張）。 */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(Com_Test_GetTxPending(1U), 1U);
}

TEST_F(Bsw_TxChain_Test, SendSignalGroupArray_NG_NullDataPtrReturnsError)
{
    /* 実行 (Act) + 評価 (Assert) */
    EXPECT_EQ(Com_SendSignalGroupArray(1U, NULL), E_NOT_OK);
    EXPECT_EQ(Com_Test_GetTxPending(1U), 0U);  // 何も変化しない
}

TEST_F(Bsw_TxChain_Test, SendSignalGroupArray_NG_NonSignalGroupIPduReturnsError)
{
    /* 準備 (Arrange): kTestTxIPdu（IPduId=0）は IsSignalGroup=0 */
    uint8_t raw[2] = { 0x12U, 0x34U };

    /* 実行 (Act) + 評価 (Assert) */
    EXPECT_EQ(Com_SendSignalGroupArray(0U, raw), E_NOT_OK);
    EXPECT_EQ(Com_Test_GetTxPending(0U), 0U);
}

// /code-review で指摘された状態不整合（シャドウバッファ・
// Com_GroupTriggerPending・Com_FilterLastValue が同期されず、通常経路と
// 混在させると黙って巻き戻る）の是正確認。この修正が無ければ、本テストは
// 「Com_SendSignalGroup() が Com_Init() 直後の古いシャドウ内容(0x00)で
// せっかくコミットした 0x81 を上書きしてしまう」形で失敗していたはず。
TEST_F(Bsw_TxChain_Test, SendSignalGroupArray_OK_SyncsShadowBufferPreventingStaleOverwrite)
{
    /* 準備 (Arrange): 配列APIで一括コミット（個々の Com_SendSignal() は
     * 一切呼ばない） */
    uint8_t raw = 0x80U;  // TMS ビット(bit0)のみ
    ASSERT_EQ(Com_SendSignalGroupArray(1U, &raw), E_OK);
    const uint8* bufAfterArray = Com_Test_GetTxBuffer(1U);
    ASSERT_NE(bufAfterArray, nullptr);
    ASSERT_EQ(bufAfterArray[0], 0x81U);  // TMS ビット + update-bit(自動セット)

    /* 実行 (Act): 通常経路のコミット関数を、個別の Com_SendSignal() を
     * 挟まずそのまま呼ぶ（呼び出し側が API を混在させた状況を再現）。 */
    Std_ReturnType ret = Com_SendSignalGroup(1U);

    /* 評価 (Assert): シャドウバッファが Com_SendSignalGroupArray() 内で
     * 既に同期済みのため、Com_SendSignalGroup() は同じ内容をそのまま
     * 再コミットするだけになり、TMS ビットの値（bit0）が保持される。 */
    EXPECT_EQ(ret, E_OK);
    const uint8* bufAfterGroup = Com_Test_GetTxBuffer(1U);
    ASSERT_NE(bufAfterGroup, nullptr);
    EXPECT_EQ(bufAfterGroup[0] & 0x80U, 0x80U);
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
// カウンタが厳密に 1 であることを確認する（本テストは Com_MainFunctionTx()/
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
     * がまだ届いていない」状態を直接作る（Com_MainFunctionTx()/PduR を経由
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
// セグメント②: Com_MainFunctionTx() ─ キューの続きから Can_Hw まで
// ------------------------------------------------------------
TEST_F(Bsw_TxChain_Test, ComMainFunction_OK_DrivesChainToCanHwSend)
{
    /* 準備 (Arrange): セグメント①の終端状態（Com_TxPending が立った状態）を用意する */
    uint16_t value = 0x1234U;
    Com_SendSignal(0U, &value);
    ASSERT_EQ(Com_Test_GetTxPending(0U), 1U);

    /* 実行 (Act) */
    Com_MainFunctionTx();

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
    Com_MainFunctionTx();

    /* 評価 (Assert) */
    EXPECT_EQ(FakeCanHw_SendCount, 0U);
}

// ------------------------------------------------------------
// Com_TxIpduCallout（SWS_Com_00346、TX I-PDU 単位のフィルタリングフック）。
// Bsw_RxChain_test.cpp の Com_RxIpduCallout テストと対になる、送信側の検証。
// kTestTxIPdu（IPduId=0）に TestTxIpduCallout を設定済み。Com_DoTransmit()
// 内で TxTransformCbk 適用後・PduR_ComTransmit() 呼び出し直前に呼ばれることを、
// Can_Hw まで到達するかどうかで確認する。
// ------------------------------------------------------------
TEST_F(Bsw_TxChain_Test, ComMainFunction_OK_AcceptedByTxIpduCalloutTransmitsNormally)
{
    /* 準備 (Arrange): s_txCalloutAccept は SetUp() で 1（既定）にリセット済み */
    uint16_t value = 0x1234U;
    Com_SendSignal(0U, &value);

    /* 実行 (Act) */
    Com_MainFunctionTx();

    /* 評価 (Assert): callout は送信直前の最終バイト列で 1 回呼ばれ、
     * 通常どおり Can_Hw まで到達する。実際に PduR へ渡したため
     * Com_TxConfPending もセットされる。 */
    EXPECT_EQ(s_txCalloutInvokeCount, 1U);
    EXPECT_EQ(s_txCalloutLastByte0, 0x12U);
    EXPECT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(Com_Test_GetTxConfPending(0U), 1U);
}

TEST_F(Bsw_TxChain_Test, ComMainFunction_NG_RejectedByTxIpduCalloutDiscardsTransmission)
{
    /* 準備 (Arrange) */
    s_txCalloutAccept = 0U;
    uint16_t value = 0x1234U;
    Com_SendSignal(0U, &value);

    /* 実行 (Act) */
    Com_MainFunctionTx();

    /* 評価 (Assert): [SWS_Com_00346] false のため PduR_ComTransmit() 以降
     * （CanIf/Can/Can_Hw）に一切到達しない。実際には送信していないため
     * Com_TxConfPending もセットされない（TX 送信デッドライン監視タイマも
     * 起動しない）。 */
    EXPECT_EQ(s_txCalloutInvokeCount, 1U);
    EXPECT_EQ(FakeCanHw_SendCount, 0U);
    EXPECT_EQ(Com_Test_GetTxConfPending(0U), 0U);
}

// ------------------------------------------------------------
// SWS_Com_00305: ComTxModeNumberOfRepetitions（変化時送信の冗長再送）。
// kTestTxIPdu（IPduId=0）は NumberOfRepetitions=2U/RepetitionPeriodMs=50U を
// 持つため、初回送信 + 2 回の再送 = 計3回送信されて止まることを検証する。
// 残り回数の減算は Com_TxConfirmation() 到達時ではなく Com_MainFunctionTx() の
// dispatch 時点で行う設計のため（Com.c のコメント参照。TX 確認は
// Can_MainFunction_Write() という別タスク経由で非同期に届き、
// Com_MainFunctionTx() 単独では待てないため）、これらのテストは
// Com_TxConfirmation() を一切呼ばない。
// ------------------------------------------------------------
TEST_F(Bsw_TxChain_Test, RepetitionSequence_OK_FiresConfiguredNumberOfRepeatsThenStops)
{
    /* 準備 (Arrange) */
    uint16_t value = 0x1234U;
    Com_SendSignal(0U, &value);

    /* 実行 (Act) + 評価 (Assert): 初回送信 */
    Com_MainFunctionTx();
    EXPECT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(Com_Test_GetTxRepeatsRemaining(0U), 2U);  // 初回はまだ減らない

    /* 1 回目の再送（RepetitionPeriodMs=50 経過後） */
    FakeMillis_Value += 50U;
    Com_MainFunctionTx();
    EXPECT_EQ(FakeCanHw_SendCount, 2U);
    EXPECT_EQ(Com_Test_GetTxRepeatsRemaining(0U), 1U);

    /* 2 回目の再送（NumberOfRepetitions=2 を使い切る） */
    FakeMillis_Value += 50U;
    Com_MainFunctionTx();
    EXPECT_EQ(FakeCanHw_SendCount, 3U);
    EXPECT_EQ(Com_Test_GetTxRepeatsRemaining(0U), 0U);

    /* 再送を使い切った後は、さらに周期が経過しても送信されない */
    FakeMillis_Value += 50U;
    Com_MainFunctionTx();
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
    Com_MainFunctionTx();
    EXPECT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(Com_Test_GetTxRepeatsRemaining(0U), 2U);

    /* 以降も正常に再送が続くことだけ 1 回分だけ確認する */
    FakeMillis_Value += 50U;
    Com_MainFunctionTx();
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
    Com_MainFunctionTx();
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    ASSERT_EQ(Com_Test_GetTxRepeatsRemaining(0U), 2U);
    Com_SetCommunicationEnabled(1U, 0U);  // RxEnabled=1, TxEnabled=0

    /* 実行 (Act): 抑制中に RepetitionPeriodMs を複数回分経過させる
     * （repeatDue 自体は周期的に真になり得るが、Com_TxEnabled==0 のため
     * Com_DoTransmit() には到達しない） */
    FakeMillis_Value += 50U;
    Com_MainFunctionTx();
    FakeMillis_Value += 50U;
    Com_MainFunctionTx();

    /* 評価 (Assert): 送信は1本も増えておらず、残り回数も空費されていない */
    EXPECT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(Com_Test_GetTxRepeatsRemaining(0U), 2U);

    /* 抑制解除後は、通常どおり残っていた再送が送信される */
    Com_SetCommunicationEnabled(1U, 1U);
    FakeMillis_Value += 50U;
    Com_MainFunctionTx();
    EXPECT_EQ(FakeCanHw_SendCount, 2U);
    EXPECT_EQ(Com_Test_GetTxRepeatsRemaining(0U), 1U);
}

TEST_F(Bsw_TxChain_Test, RepetitionSequence_NG_DoesNotFireBeforePeriodElapsed)
{
    /* 準備 (Arrange): 初回送信を済ませておく */
    uint16_t value = 0x1234U;
    Com_SendSignal(0U, &value);
    Com_MainFunctionTx();
    ASSERT_EQ(FakeCanHw_SendCount, 1U);

    /* 実行 (Act): RepetitionPeriodMs(50) 未満しか経過していない */
    FakeMillis_Value += 49U;
    Com_MainFunctionTx();

    /* 評価 (Assert): 再送されない。残り回数も減らない */
    EXPECT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(Com_Test_GetTxRepeatsRemaining(0U), 2U);
}

TEST_F(Bsw_TxChain_Test, RepetitionSequence_OK_NewSendSignalRestartsSequence)
{
    /* 準備 (Arrange): 初回送信 + 1 回の再送を消費させる */
    uint16_t value = 0x1234U;
    Com_SendSignal(0U, &value);
    Com_MainFunctionTx();
    FakeMillis_Value += 50U;
    Com_MainFunctionTx();
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

// ------------------------------------------------------------
// SWS_Com_00878/00879/00880/00304/00554: TX 送信デッドライン監視
// （Com_CbkTxTOut）。kTestTxIPdu（IPduId=0）は TxFirstTimeoutMs=1000U/
// TxTimeoutMs=500U を持つ。実際のディスパッチ（Com_MainFunctionTx() 経由）が
// タイマをアームするため、Com_TxConfirmation() を直接呼ぶ場合を除き
// Com_MainFunctionTx() を通して検証する。
// ------------------------------------------------------------
TEST_F(Bsw_TxChain_Test, TxTOut_OK_FiresAfterFirstTimeoutWhenArmedAndUnconfirmed)
{
    /* 準備 (Arrange): 送信し、確認を一切与えない（アームしたまま放置） */
    uint16_t value = 0x1234U;
    Com_SendSignal(0U, &value);
    Com_MainFunctionTx();  // t=0: 実送信、Com_TxConfPendingSinceMs[0]=0 でアーム
    ASSERT_EQ(Com_Test_GetTxConfPending(0U), 1U);

    /* 実行 (Act) + 評価 (Assert): TxFirstTimeoutMs(1000) 未満ではまだ発火しない */
    FakeMillis_Value += 999U;
    Com_MainFunctionTx();
    EXPECT_EQ(Com_Test_GetTxTimedOut(0U), 0U);
    EXPECT_EQ(s_txTOutCount, 0U);

    /* TxFirstTimeoutMs(1000) 超過で発火する */
    FakeMillis_Value += 2U;
    Com_MainFunctionTx();
    EXPECT_EQ(Com_Test_GetTxTimedOut(0U), 1U);
    EXPECT_EQ(s_txTOutCount, 1U);
}

TEST_F(Bsw_TxChain_Test, TxTOut_OK_RepeatsDoNotRestartOrExtendDeadline)
{
    /* 準備 (Arrange): 初回送信 + ComTxModeNumberOfRepetitions による再送
     * （t=50/100、計3回送信）が進行する間、デッドラインタイマは最初の
     * アーム時刻（t=0）を基準にしたままであることを確認する
     * （[SWS_Com_00878] "unless already running"）。 */
    uint16_t value = 0x1234U;
    Com_SendSignal(0U, &value);
    Com_MainFunctionTx();  // t=0: 初回送信、アーム
    FakeMillis_Value += 50U;
    Com_MainFunctionTx();  // t=50: 再送1回目（Com_TxConfPending は既に1のまま）
    FakeMillis_Value += 50U;
    Com_MainFunctionTx();  // t=100: 再送2回目（NumberOfRepetitions を使い切る）
    FakeMillis_Value += 50U;
    Com_MainFunctionTx();  // t=150: 再送なし

    /* 実行 (Act): t=0 基準で TxFirstTimeoutMs(1000) を超過させる
     * （t=150 + 851 = 1001。再送のたびにタイマが延命されていれば
     * t=100+1000=1100 まで発火しないはずだが、そうならないことを確認する） */
    FakeMillis_Value += 851U;
    Com_MainFunctionTx();

    /* 評価 (Assert) */
    EXPECT_EQ(Com_Test_GetTxTimedOut(0U), 1U);
    EXPECT_EQ(s_txTOutCount, 1U);
}

TEST_F(Bsw_TxChain_Test, TxTOut_OK_ConfirmationBeforeDeadlineCancelsIt)
{
    /* 準備 (Arrange): 送信後、TxFirstTimeoutMs(1000) 未満のうちに確認する */
    uint16_t value = 0x1234U;
    Com_SendSignal(0U, &value);
    Com_MainFunctionTx();  // t=0: 送信、アーム
    FakeMillis_Value += 400U;
    Com_TxConfirmation(0U, E_OK);  // t=400: 確認到達、タイマ解除
    ASSERT_EQ(Com_Test_GetTxConfPending(0U), 0U);

    /* 実行 (Act): TxFirstTimeoutMs を優に超える時間が経過しても、
     * 既に確認済み（Com_TxConfPending==0）のため監視対象外のまま */
    FakeMillis_Value += 700U;
    Com_MainFunctionTx();

    /* 評価 (Assert) */
    EXPECT_EQ(Com_Test_GetTxTimedOut(0U), 0U);
    EXPECT_EQ(s_txTOutCount, 0U);
}

TEST_F(Bsw_TxChain_Test, TxTOut_OK_UsesSteadyTimeoutAfterFirstConfirmedCycle)
{
    /* 準備 (Arrange): 1 サイクル分、送信→確認を完了させる
     * （Com_TxUsingFirstTimeout を false へ倒す） */
    uint16_t value = 0x1234U;
    Com_SendSignal(0U, &value);
    Com_MainFunctionTx();
    Com_TxConfirmation(0U, E_OK);
    ASSERT_EQ(Com_Test_GetTxConfPending(0U), 0U);

    /* 実行 (Act): 新たな送信要求で再アームする（steady TxTimeoutMs=500 を
     * 使うはずで、TxFirstTimeoutMs=1000 は使わない） */
    uint16_t value2 = 0x5678U;
    Com_SendSignal(0U, &value2);
    Com_MainFunctionTx();
    ASSERT_EQ(Com_Test_GetTxConfPending(0U), 1U);

    FakeMillis_Value += 500U;
    Com_MainFunctionTx();

    /* 評価 (Assert): TxFirstTimeoutMs(1000) ではなく TxTimeoutMs(500) で
     * 発火している */
    EXPECT_EQ(Com_Test_GetTxTimedOut(0U), 1U);
    EXPECT_EQ(s_txTOutCount, 1U);
}

TEST_F(Bsw_TxChain_Test, TxTOut_OK_GroupLevelFiresWhenStartedAndOverdue)
{
    /* 準備 (Arrange): kTestErrGroupIPdu（IPduId=3、Signal Group、
     * TxFirstTimeoutMs=100U）を起動し、test-only setter で
     * 「送信済み・未確認」状態を直接注入する（実際に Com_MainFunctionTx()/PduR
     * を経由させる配線は用意していないため）。 */
    Com_IpduGroupStart(kTestStoppableGroupId, 0U);
    Com_Test_SetTxConfPending(3U, 1U);
    Com_Test_SetTxConfPendingSinceMs(3U, FakeMillis_Value);

    /* 実行 (Act): TxFirstTimeoutMs(100) を超過させる */
    FakeMillis_Value += 101U;
    Com_MainFunctionTx();

    /* 評価 (Assert): グループ単位で発火する。TxErrCbk とは無関係 */
    EXPECT_EQ(s_groupTxTOutCount, 1U);
    EXPECT_EQ(s_groupTxErrCount, 0U);
}

TEST_F(Bsw_TxChain_Test, IpduGroupStop_OK_PreventsTxTOutDoubleFireWithTxErrCbk)
{
    /* 準備 (Arrange): TxTOut_OK_GroupLevelFiresWhenStartedAndOverdue と同じ
     * 「送信済み・未確認のまま閾値超過」状態を作るが、Com_MainFunctionTx() で
     * 評価される前に Com_IpduGroupStop() を先に呼ぶ。 */
    Com_IpduGroupStart(kTestStoppableGroupId, 0U);
    Com_Test_SetTxConfPending(3U, 1U);
    Com_Test_SetTxConfPendingSinceMs(3U, FakeMillis_Value);
    FakeMillis_Value += 101U;

    /* 実行 (Act) */
    Com_IpduGroupStop(kTestStoppableGroupId);  // TxErrCbk が発火、Started=0 に
    Com_MainFunctionTx();  // Com_TxIPduStarted[3]==0 のため監視ループ自体が対象外

    /* 評価 (Assert): TxErrCbk は発火するが、TxTOutCbk とは二重発火しない */
    EXPECT_EQ(s_groupTxErrCount, 1U);
    EXPECT_EQ(s_groupTxTOutCount, 0U);
}

// ------------------------------------------------------------
// Com_InvalidateSignal（SWS_Com_00099/SWS_Com_00642/SWS_Com_00643、2026-08
// 追加）。kTestSignal（非 Signal Group、IPduId=0、16bit BigEndian、
// InvalidValue=0xBEEF・InvalidValueConfigured=1）を流用する。
// ------------------------------------------------------------
TEST_F(Bsw_TxChain_Test, InvalidateSignal_OK_WritesConfiguredInvalidValueToBuffer)
{
    /* 実行 (Act) */
    uint8 ret = Com_InvalidateSignal(0U);

    /* 評価 (Assert): [SWS_Com_00642] 内部で Com_SendSignal() が呼ばれ、
     * ComSignalDataInvalidValue (0xBEEF) がそのまま TX バッファへ反映される。 */
    EXPECT_EQ(ret, E_OK);
    const uint8* buf = Com_Test_GetTxBuffer(0U);
    ASSERT_NE(buf, nullptr);
    EXPECT_EQ(buf[0], 0xBEU);
    EXPECT_EQ(buf[1], 0xEFU);
}

TEST_F(Bsw_TxChain_Test, InvalidateSignal_NG_UnconfiguredInvalidValueReturnsErrorWithoutWriting)
{
    /* 準備 (Arrange): kTestNonGroupTmsCalledSignal（SignalId=5、IPduId=2）は
     * InvalidValueConfigured が既定の 0（未設定）のまま。 */

    /* 実行 (Act) */
    uint8 ret = Com_InvalidateSignal(5U);

    /* 評価 (Assert): [SWS_Com_00643] ComSignalDataInvalidValue 未設定のため
     * E_NOT_OK。副作用（バッファ書き込み）も一切起きない。 */
    EXPECT_EQ(ret, E_NOT_OK);
    const uint8* buf = Com_Test_GetTxBuffer(2U);
    ASSERT_NE(buf, nullptr);
    // bit0 は kTestNonGroupTmsContributorSignal（SignalId=4、InitValue=1）が
    // Com_Init() 時点で既にパック済み（0x80）。SignalId=5（bit1）側は
    // 今回の呼び出しが失敗したので変化しない。
    EXPECT_EQ(buf[0], 0x80U);
}

TEST_F(Bsw_TxChain_Test, InvalidateSignal_NG_UnknownSignalIdReturnsError)
{
    /* 実行 (Act) + 評価 (Assert) */
    EXPECT_EQ(Com_InvalidateSignal(255U), E_NOT_OK);
}

TEST_F(Bsw_TxChain_Test, InvalidateSignal_NG_RxSignalReturnsErrorWithoutReachingSendSignal)
{
    /* 準備 (Arrange): kTestInvalidateRxSignal（SignalId=8、Direction=RX）は
     * InvalidValueConfigured=1（誤設定された想定）だが、RX/TX の IPduId が
     * 数値空間を共有するため、Direction チェックが無いと Com_SendSignal()
     * 側で偶然一致する TX I-PDU を静かに書き換えかねない（/code-review 指摘）。 */

    /* 実行 (Act) + 評価 (Assert): Direction チェックのみで拒否される */
    EXPECT_EQ(Com_InvalidateSignal(8U), E_NOT_OK);
}

// ------------------------------------------------------------
// Com_InvalidateSignalGroup（SWS_Com_00557/SWS_Com_00645、2026-08 追加）。
// ------------------------------------------------------------
TEST_F(Bsw_TxChain_Test, InvalidateSignalGroup_OK_WritesMemberInvalidValueAndCommitsToBuffer)
{
    /* 準備 (Arrange): kTestTmsGroupIPdu（IPduId=1）の唯一のメンバー
     * kTestTmsPendingSignal（SignalId=1、BitPosition=0、BigEndian 1bit＝
     * 0x80）は InvalidValue=1・InvalidValueConfigured=1。 */

    /* 実行 (Act) */
    uint8 ret = Com_InvalidateSignalGroup(1U);

    /* 評価 (Assert): [SWS_Com_00645] 内部で Com_SendSignal()（シャドウ
     * バッファへ書き込み）→ Com_SendSignalGroup()（実バッファへコミット）
     * の順に呼ばれ、メンバーの InvalidValue が実バッファへ反映される。 */
    EXPECT_EQ(ret, E_OK);
    const uint8* buf = Com_Test_GetTxBuffer(1U);
    ASSERT_NE(buf, nullptr);
    EXPECT_EQ(buf[0] & 0x80U, 0x80U);
}

TEST_F(Bsw_TxChain_Test, InvalidateSignalGroup_NG_AnyMemberUnconfiguredReturnsErrorWithoutPartialCommit)
{
    /* 準備 (Arrange): kTestErrGroupIPdu（IPduId=3）に、InvalidValueConfigured=1
     * の SignalId=6 と、あえて未設定のままの SignalId=7 の 2 メンバーを設定
     * 済み（ファイル冒頭のシグナル定義参照）。 */

    /* 実行 (Act) */
    uint8 ret = Com_InvalidateSignalGroup(3U);

    /* 評価 (Assert): [SWS_Com_00557] "no ComSignalDataInvalidValue is
     * configured for any of the group signals" に該当するため、1本でも
     * 未設定なら all-or-nothing で全体を E_NOT_OK とし、設定済みの
     * SignalId=6 側も含めて一切バッファへ書き込まない。 */
    EXPECT_EQ(ret, E_NOT_OK);
    const uint8* buf = Com_Test_GetTxBuffer(3U);
    ASSERT_NE(buf, nullptr);
    EXPECT_EQ(buf[0], 0x00U);
}

// ------------------------------------------------------------
// Com_TriggerIPDUSend（SWS_Com_00861/SWS_Com_00388/SWS_Com_00492、2026-08
// 追加）。DIRECT/MIXED 分岐の検証。PERIODIC モードの分岐は、本ファイルの
// TX I-PDU 4 本（COM_TX_IPDU_MAX と同数、これ以上増やせない）がいずれも
// DIRECT/MIXED のため、ファイル末尾の独立した名前空間
// `tx_trigger_periodic`（専用の最小 Com_ConfigType、IPduId=0 を再利用）で
// 別途検証する（/code-review 指摘: 当初は「コード構造上の対称性」のみを
// 根拠にテストを省略していたが、実際にテストを追加した）。
// ------------------------------------------------------------
TEST_F(Bsw_TxChain_Test, TriggerIPDUSend_OK_ForcesDispatchWithoutValueChange)
{
    /* 準備 (Arrange): kTestTxIPdu（IPduId=0、DIRECT）へ一切 Com_SendSignal()
     * を呼ばない（値の変化なし、Com_TxPending は立てない）。 */

    /* 実行 (Act) */
    uint8 ret = Com_TriggerIPDUSend(0U);
    ASSERT_EQ(ret, E_OK);
    Com_MainFunctionTx();

    /* 評価 (Assert): 値の変化が一切無くても送信される（[SWS_Com_00861]）。
     * バッファ内容自体は InitValue のまま（トリガーは中身を変えない）。 */
    EXPECT_EQ(Com_Test_GetTxTriggerPending(0U), 0U);
    EXPECT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x100U);
    EXPECT_EQ(FakeCanHw_LastSendDlc, 2U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x00U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x00U);
}

TEST_F(Bsw_TxChain_Test, TriggerIPDUSend_NG_UnknownPduIdReturnsError)
{
    /* 実行 (Act) + 評価 (Assert) */
    EXPECT_EQ(Com_TriggerIPDUSend(99U), E_NOT_OK);
}

TEST_F(Bsw_TxChain_Test, TriggerIPDUSend_NG_StoppedIpduReturnsErrorWithoutTriggering)
{
    /* 準備 (Arrange): kTestErrGroupIPdu（IPduId=3）は IpduGroupId=
     * kTestStoppableGroupId を持つため、Com_Init() 直後は既定で停止状態
     * （[SWS_Com_00444]/[SWS_Com_00840]）。Com_IpduGroupStart() を一度も
     * 呼ばないことで「起動されたことがない」状態を明示的に表す
     * （念のため Com_IpduGroupStop() も呼び、Start 後に Stop された場合と
     * 同じ経路であることも合わせて確認する）。 */
    Com_IpduGroupStop(kTestStoppableGroupId);

    /* 実行 (Act) + 評価 (Assert): [SWS_Com_00861] stopped I-PDU は E_NOT_OK。
     * トリガー自体も記録されない（後で started になっても自動実行されない）。 */
    EXPECT_EQ(Com_TriggerIPDUSend(3U), E_NOT_OK);
    EXPECT_EQ(Com_Test_GetTxTriggerPending(3U), 0U);
}

TEST_F(Bsw_TxChain_Test, TriggerIPDUSend_OK_RespectsMinDelayTimeAndDispatchesOnceElapsed)
{
    /* 準備 (Arrange): kTestErrGroupIPdu（IPduId=3）は IpduGroupId=
     * kTestStoppableGroupId を持つため、Com_Init() 直後は既定で停止状態
     * （[SWS_Com_00444]/[SWS_Com_00840]、Com_Init() の実装コメント参照）。
     * 明示的に起動してから使う（TxTOut_OK_GroupLevelFiresWhenStartedAndOverdue
     * と同じ手順）。MinDelayMs=100U。Com_IpduGroupStart() が
     * Com_TxLastSentMs[3] を現在時刻にリセットするため、経過時間はここから
     * 0 スタートになる。 */
    Com_IpduGroupStart(kTestStoppableGroupId, 0U);

    /* 実行 (Act 1): トリガーは受け付けるが、MDT 未経過のため今回は送信しない
     * （[SWS_Com_00388] "shall postpone transmissions if necessary"）。 */
    ASSERT_EQ(Com_TriggerIPDUSend(3U), E_OK);
    Com_MainFunctionTx();

    /* 評価 (Assert 1): トリガーは破棄されず、次回以降のために保持される */
    EXPECT_EQ(Com_Test_GetTxTriggerPending(3U), 1U);

    /* 実行 (Act 2): MDT 経過後に再度 Com_MainFunctionTx() を呼ぶ */
    FakeMillis_Value += 100U;
    Com_MainFunctionTx();

    /* 評価 (Assert 2): MDT 経過により消費される（実際の PduR ルートは
     * 未登録のため CAN 送信までは進まないが、ディスパッチ自体は試行される
     * ことをフラグのクリアで確認する） */
    EXPECT_EQ(Com_Test_GetTxTriggerPending(3U), 0U);
}

TEST_F(Bsw_TxChain_Test, TriggerIPDUSend_OK_DoesNotConsumeNumberOfRepetitionsBudget)
{
    /* 準備 (Arrange): kTestTxIPdu（IPduId=0、NumberOfRepetitions=2U）の
     * 残り再送回数を明示的にセットしておく。 */
    Com_Test_SetTxRepeatsRemaining(0U, 2U);

    /* 実行 (Act) */
    ASSERT_EQ(Com_TriggerIPDUSend(0U), E_OK);
    Com_MainFunctionTx();

    /* 評価 (Assert): [SWS_Com_00388] "shall not take into account ...
     * ComTxModeNumberOfRepetitions" のとおり、残り回数は変化しない
     * （通常の repeatDue によるデクリメントとは独立した OR 項のため）。 */
    EXPECT_EQ(FakeCanHw_SendCount, 1U);  // トリガー自体は送信を引き起こす
    EXPECT_EQ(Com_Test_GetTxRepeatsRemaining(0U), 2U);
}

// ------------------------------------------------------------
// Com_SwitchIpduTxMode（SWS_Com_00881/SWS_Com_00239/SWS_Com_00244、2026-08
// 追加）。kTestTmsGroupIPdu（IPduId=1、Signal Group、既定 Com_TmsState=0）
// を流用する。
// ------------------------------------------------------------
TEST_F(Bsw_TxChain_Test, SwitchIpduTxMode_OK_FlipsStateAndTriggersImmediateSend)
{
    /* 準備 (Arrange): Com_Init() 直後は Com_TmsState[1]==0（既定 false）。 */
    ASSERT_EQ(Com_Test_GetTmsState(1U), 0U);

    /* 実行 (Act) */
    Com_SwitchIpduTxMode(1U, 1U);

    /* 評価 (Assert): [SWS_Com_00881] 状態が切り替わり、[SWS_Com_00239]/
     * [SWS_Com_00495] と同じ経路（Com_RequestTxOnChange()）で次回
     * Com_MainFunctionTx() 向けの送信要求が立つ。 */
    EXPECT_EQ(Com_Test_GetTmsState(1U), 1U);
    EXPECT_EQ(Com_Test_GetTxPending(1U), 1U);
}

TEST_F(Bsw_TxChain_Test, SwitchIpduTxMode_NG_NoEffectWhenModeAlreadyActive)
{
    /* 準備 (Arrange): 既定状態(false)と同じ Mode=0 を明示的に要求する。 */

    /* 実行 (Act) */
    Com_SwitchIpduTxMode(1U, 0U);

    /* 評価 (Assert): spec 原文 "the call will have no effect"。送信要求も
     * 立たない（状態が変化していないため Com_RequestTxOnChange() は
     * 呼ばれない）。 */
    EXPECT_EQ(Com_Test_GetTmsState(1U), 0U);
    EXPECT_EQ(Com_Test_GetTxPending(1U), 0U);
}

TEST_F(Bsw_TxChain_Test, SwitchIpduTxMode_OK_TogglingBackTriggersAnotherSend)
{
    /* 準備 (Arrange): 一旦 true へ切り替え、Com_MainFunctionTx() で
     * 送信要求を消費させておく。 */
    Com_SwitchIpduTxMode(1U, 1U);
    Com_MainFunctionTx();
    ASSERT_EQ(Com_Test_GetTxPending(1U), 0U);

    /* 実行 (Act): false へ戻す（再び実際の変化） */
    Com_SwitchIpduTxMode(1U, 0U);

    /* 評価 (Assert): 戻すのも「変化」であるため、再度送信要求が立つ */
    EXPECT_EQ(Com_Test_GetTmsState(1U), 0U);
    EXPECT_EQ(Com_Test_GetTxPending(1U), 1U);
}

TEST_F(Bsw_TxChain_Test, SwitchIpduTxMode_NG_UnknownPduIdHasNoEffect)
{
    /* 実行 (Act) + 評価 (Assert): 戻り値が無い（void）API のため、
     * クラッシュしないこと・既存の状態に影響しないことを確認する。 */
    Com_SwitchIpduTxMode(99U, 1U);
    EXPECT_EQ(Com_Test_GetTmsState(1U), 0U);
    EXPECT_EQ(Com_Test_GetTxPending(1U), 0U);
}

// ------------------------------------------------------------
// Com_TriggerIPDUSend の COM_TX_MODE_PERIODIC 分岐専用の独立したフィクスチャ。
// Bsw_TxChain_Test（上記）は TX I-PDU 4 本（COM_TX_IPDU_MAX と同数）を
// 既に使い切っており、新たに PERIODIC I-PDU を追加できない
// （feedback_test_chain_ipdu_id_ceiling: COM_RX/TX_IPDU_MAX は
// native_chain バイナリ全体で共有される固定サイズ配列であり、超過は
// 範囲外書き込みによる無関係なテストの原因不明なハングを引き起こす）。
// そのため Bsw_RxTimeoutChain_test.cpp の `rx_ipdu_group` 名前空間と同じ
// 手法（専用の最小 Com_ConfigType、IPduId=0 を再利用した独立した
// Com_Init() サイクル）で分離する。
//
// PduR_Init()/CanIf_Init() はあえて呼ばない: このフィクスチャの関心は
// 「Com_TxTriggerPending が PERIODIC I-PDU でも period 判定と独立した OR 項
// として効くか」のみであり、それは Com_MainFunctionTx() 内で
// PduR_ComTransmit() を呼ぶ前に確定する（Com_TxTriggerPending[id]=0 の
// クリアは due=true になった時点で無条件に行われる、Com.c 参照）。
// PduR_ConfigPtr が NULL のままでも PduR_ComTransmit() は安全に E_NOT_OK を
// 返すため（PduR.c 参照）、実際の CAN 送信まで配線しなくても検証できる。
//
// このパターンの SetUp()/TearDown() 自体は本ファイル内で 2 回目の登場のため
// （tx_switch_periodic 名前空間も同じ構成を使う）、共通基底クラスへ切り出す
// （/code-review 指摘、rule of three）。派生側は対象の Com_ConfigType への
// ポインタを返す GetComConfig() だけを実装する。
// ------------------------------------------------------------
class IsolatedComTxFixtureBase : public ::testing::Test
{
protected:
    virtual const Com_ConfigType* GetComConfig() const = 0;

    void SetUp() override
    {
        FakeMillis_Reset();
        FakeCanHw_Reset();
        FakeDetHw_LogSuppressed = 1U;
        Com_Init(GetComConfig());
        FakeDetHw_LogSuppressed = 0U;
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Com_DeInit();
    }
};

namespace tx_trigger_periodic
{

const Com_IPduConfigType kTestPeriodicIPdu = {
    /* IPduId */           0U,
    /* DLC */              1U,
    /* PduRId */           0U,   // PduR_Init() を呼ばないため未登録のまま
                                 // （上記名前空間コメント参照）
    /* FirstTimeoutMs */   0U,
    /* TimeoutMs */        0U,
    /* IsSignalGroup */    0U,
    /* TxModeMode */       COM_TX_MODE_PERIODIC,
    /* TxPeriodMs */       1000U,
    /* TxModeModeTrue */   COM_TX_MODE_PERIODIC,
    /* TxPeriodMsTrue */   1000U,
    /* MinDelayMs */       50U,  // Com_TriggerIPDUSend の MDT 尊重（SWS_Com_00388）検証用
    /* UpdateBitPosition */ 0xFFU,
    /* IpduGroupId */      COM_IPDU_GROUP_NONE
};

const Com_IPduConfigType kTestTxIPdus[] = { kTestPeriodicIPdu };

const Com_ConfigType kTestComConfig = {
    /* RxIPdus */       NULL,
    /* RxIPduCount */   0U,
    /* TxIPdus */       kTestTxIPdus,
    /* TxIPduCount */   1U,
    /* Signals */       NULL,
    /* SignalCount */   0U,
    /* GwMappings */    NULL,
    /* GwMappingCount */ 0U
};

class Bsw_TxTriggerPeriodicChain_Test : public IsolatedComTxFixtureBase
{
protected:
    const Com_ConfigType* GetComConfig() const override { return &kTestComConfig; }
};

TEST_F(Bsw_TxTriggerPeriodicChain_Test, TriggerIPDUSend_OK_FiresBetweenPeriodsOnceMdtElapses)
{
    /* 準備 (Arrange): kTestPeriodicIPdu は IpduGroupId=COM_IPDU_GROUP_NONE の
     * ため Com_Init() 直後から起動済み。TxPeriodMs=1000U だが、経過時間は
     * まだ 0 のため自然な周期発火は起こらない。 */

    /* 実行 (Act 1): トリガー直後、MDT(50ms)未経過ではまだ消費されない */
    ASSERT_EQ(Com_TriggerIPDUSend(0U), E_OK);
    Com_MainFunctionTx();
    EXPECT_EQ(Com_Test_GetTxTriggerPending(0U), 1U);

    /* 実行 (Act 2): MDT 経過後は、TxPeriodMs(1000ms) にまだ遠く及ばなくても
     * トリガーにより送信が試行される（[SWS_Com_00861]/[SWS_Com_00388]:
     * PERIODIC I-PDU でも TxModeMode によらず効く。Com_TxTriggerPending の
     * 宣言コメント参照）。 */
    FakeMillis_Value += 50U;
    Com_MainFunctionTx();
    EXPECT_EQ(Com_Test_GetTxTriggerPending(0U), 0U);
}

TEST_F(Bsw_TxTriggerPeriodicChain_Test, ComMainFunction_NG_DoesNotFireBeforePeriodElapsedWithoutTrigger)
{
    /* 準備 (Arrange): トリガーを一切呼ばない（回帰確認: 本変更が既存の
     * PERIODIC 判定そのものを壊していないこと）。 */

    /* 実行 (Act): TxPeriodMs(1000ms) 未満だけ経過させる */
    FakeMillis_Value += 999U;
    Com_MainFunctionTx();

    /* 評価 (Assert): トリガーが無い限り、period 未経過では送信されない */
    EXPECT_EQ(Com_Test_GetTxTriggerPending(0U), 0U);
    EXPECT_EQ(FakeCanHw_SendCount, 0U);
}

}  // namespace tx_trigger_periodic

// ------------------------------------------------------------
// Com_SwitchIpduTxMode が遷移後の実効 TxModeMode を PERIODIC にする場合の
// 周期タイマ再始動（[SWS_Com_00244]）専用の独立したフィクスチャ。
// tx_trigger_periodic の kTestPeriodicIPdu は既定状態（TMS=false）自体が
// PERIODIC であり、他の2件のテストがそれに依存しているため流用できない
// （このシナリオが必要とするのは「既定は DIRECT で、TMS=true になった
// 瞬間に初めて PERIODIC へ切り替わる」逆方向の構成）。COM_TX_IPDU_MAX の
// 制約により本ファイルの他のフィクスチャとは独立した最小 Com_ConfigType を
// 別途用意する（tx_trigger_periodic と同じ手法。共通の SetUp()/TearDown() は
// IsolatedComTxFixtureBase を継承して再利用する）。
// ------------------------------------------------------------
namespace tx_switch_periodic
{

const Com_IPduConfigType kTestTmsPeriodicIPdu = {
    /* IPduId */           0U,
    /* DLC */              1U,
    /* PduRId */           0U,   // PduR_Init() を呼ばないため未登録のまま
    /* FirstTimeoutMs */   0U,
    /* TimeoutMs */        0U,
    /* IsSignalGroup */    0U,
    /* TxModeMode */       COM_TX_MODE_DIRECT,    // TMS=false（既定）
    /* TxPeriodMs */       0U,
    /* TxModeModeTrue */   COM_TX_MODE_PERIODIC,  // TMS=true で PERIODIC へ
    /* TxPeriodMsTrue */   1000U,
    /* MinDelayMs */       0U,
    /* UpdateBitPosition */ 0xFFU,
    /* IpduGroupId */      COM_IPDU_GROUP_NONE
};

const Com_IPduConfigType kTestTxIPdus[] = { kTestTmsPeriodicIPdu };

const Com_ConfigType kTestComConfig = {
    /* RxIPdus */       NULL,
    /* RxIPduCount */   0U,
    /* TxIPdus */       kTestTxIPdus,
    /* TxIPduCount */   1U,
    /* Signals */       NULL,
    /* SignalCount */   0U,
    /* GwMappings */    NULL,
    /* GwMappingCount */ 0U
};

class Bsw_TxSwitchPeriodicChain_Test : public IsolatedComTxFixtureBase
{
protected:
    const Com_ConfigType* GetComConfig() const override { return &kTestComConfig; }
};

TEST_F(Bsw_TxSwitchPeriodicChain_Test, SwitchIpduTxMode_OK_RestartsPeriodicTimerOnTransitionIntoPeriodic)
{
    /* 準備 (Arrange): Com_Init() から 700ms 経過させてから切り替える
     * （「タイマが Init 時点のままか、切り替え時点で再始動されたか」を
     * 後段で区別できるようにするため）。 */
    FakeMillis_Value += 700U;

    /* 実行 (Act 1): TMS を true へ切り替える。実効 TxModeMode は
     * DIRECT→PERIODIC へ変化するため、Com_RequestTxOnChange() 経由の
     * 即時送信は発生しない（PERIODIC の設計どおり）。 */
    Com_SwitchIpduTxMode(0U, 1U);
    EXPECT_EQ(Com_Test_GetTmsState(0U), 1U);
    EXPECT_EQ(FakeCanHw_SendCount, 0U);  // PERIODIC への遷移自体は即時送信しない

    /* 実行 (Act 2): 切り替え時点から 350ms だけ経過させる（Init 時点からは
     * 1050ms、TxPeriodMsTrue(1000ms) 以上）。 */
    FakeMillis_Value += 350U;
    Com_MainFunctionTx();

    /* 評価 (Assert): [SWS_Com_00244] 周期タイマが切り替え時点で再始動されて
     * いれば、切り替えからまだ 350ms しか経っていないため送信されない。
     * 再始動されていなければ（是正前のバグ）、Com_TxLastSentMs が
     * Com_Init() 時点のまま残り、経過 1050ms >= 1000ms と誤判定されて
     * 送信されてしまう。 */
    EXPECT_EQ(FakeCanHw_SendCount, 0U);
}

}  // namespace tx_switch_periodic

}  // namespace
