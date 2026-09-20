/**
 * \file    Bsw_ComStack_TxChain_SwitchIpduTxMode_test.cpp
 * \brief   README.md「Tx 処理（Com → PduR → CanIf → Can の順）」コールチェーンの
 *          単体テスト（GoogleTest / CMake native_chain_tests）。SwitchIpduTxMode シナリオ専用。
 *
 * \details 2026-09-20、Bsw_ComStack_TxChain_test.cpp から本シナリオ（OK 1種＋
 *          派生 NG）を切り出した（1 OK シナリオ名につき1ファイルという方針、
 *          ユーザー指示）。Com/PduR/CanIf のテスト専用設定・fixture は元ファイルと
 *          全く同じものを複製している（COM_TX_IPDU_MAX の制約上、シナリオごとに
 *          設定を作り分けるより安全なため）。設定の背景・コールチェーン全体の
 *          説明は元ファイル（分割前）のコメントをそのまま引き継いでいる。
 */
#include <gtest/gtest.h>

extern "C" {
#include "Com.h"
#include "PduR.h"
#include "CanIf.h"
#include "Can.h"
#include "Can_Hw.h"
#include "Fake_Can_Hw.h"
#include "Fake_Det_Hw.h"
#include "Fake_Millis.h"
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
// 切り替えられるトグル式（Bsw_ComStack_RxChain_test.cpp の TestRxIpduCallout と対称）。
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

class Bsw_ComStack_TxChain_SwitchIpduTxMode_Test : public ::testing::Test
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
// Com_SwitchIpduTxMode（SWS_Com_00881/SWS_Com_00239/SWS_Com_00244、2026-08
// 追加）。kTestTmsGroupIPdu（IPduId=1、Signal Group、既定 Com_TmsState=0）
// を流用する。
// ------------------------------------------------------------
TEST_F(Bsw_ComStack_TxChain_SwitchIpduTxMode_Test, SwitchIpduTxMode_OK_FlipsStateAndTriggersImmediateSend)
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

TEST_F(Bsw_ComStack_TxChain_SwitchIpduTxMode_Test, SwitchIpduTxMode_NG_NoEffectWhenModeAlreadyActive)
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

TEST_F(Bsw_ComStack_TxChain_SwitchIpduTxMode_Test, SwitchIpduTxMode_OK_TogglingBackTriggersAnotherSend)
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

TEST_F(Bsw_ComStack_TxChain_SwitchIpduTxMode_Test, SwitchIpduTxMode_NG_UnknownPduIdHasNoEffect)
{
    /* 実行 (Act) + 評価 (Assert): 戻り値が無い（void）API のため、
     * クラッシュしないこと・既存の状態に影響しないことを確認する。 */
    Com_SwitchIpduTxMode(99U, 1U);
    EXPECT_EQ(Com_Test_GetTmsState(1U), 0U);
    EXPECT_EQ(Com_Test_GetTxPending(1U), 0U);
}
// ------------------------------------------------------------
// Com_TriggerIPDUSend の COM_TX_MODE_PERIODIC 分岐専用の独立したフィクスチャ。
// Bsw_ComStack_TxChain_SwitchIpduTxMode_Test（上記）は TX I-PDU 4 本（COM_TX_IPDU_MAX と同数）を
// 既に使い切っており、新たに PERIODIC I-PDU を追加できない
// （feedback_test_chain_ipdu_id_ceiling: COM_RX/TX_IPDU_MAX は
// native_chain バイナリ全体で共有される固定サイズ配列であり、超過は
// 範囲外書き込みによる無関係なテストの原因不明なハングを引き起こす）。
// そのため Bsw_ComStack_RxTimeoutChain_test.cpp の `rx_ipdu_group` 名前空間と同じ
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

class Bsw_ComStack_TxChain_SwitchIpduTxModePeriodic_Test : public IsolatedComTxFixtureBase
{
protected:
    const Com_ConfigType* GetComConfig() const override { return &kTestComConfig; }
};

TEST_F(Bsw_ComStack_TxChain_SwitchIpduTxModePeriodic_Test, SwitchIpduTxMode_OK_RestartsPeriodicTimerOnTransitionIntoPeriodic)
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
