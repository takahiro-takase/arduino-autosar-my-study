/**
 * \file    Com_MainFunctionRx_test.cpp
 * \brief   Com_MainFunctionRx() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 Com_test.cpp）。
 *
 *          分割前の共通の説明は、旧 Com_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Com.h"
#include "Fake_Det_Hw.h"
#include "Fake_Millis.h"
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

// COM_TX_IPDU_MAX(=4)/COM_RX_IPDU_MAX(=3) を超える、テーブル破損を模した値
// （Com_Cfg.h の実際の値とは無関係。設定テーブル自体が壊れていた場合の
// 防御チェックを再現するためだけの定数）。
const uint8 kOutOfRangeIPduId = 200U;

// 範囲内だが TX/RX どちらのテーブルにも登録しない ID（「配列範囲内だが
// 登録されていない I-PDU」を再現する）。
const uint8 kUnregisteredIPduId = 1U;

const Com_IPduConfigType kTestTxIPdu = {
    /* IPduId */            0U,
    /* DLC */               1U,
    /* PduRId */            0U,
    /* FirstTimeoutMs */    0U,
    /* TimeoutMs */         0U,
    /* IsSignalGroup */     0U,
    /* TxModeMode */        COM_TX_MODE_DIRECT,
    /* TxPeriodMs */        0U,
    /* TxModeModeTrue */    COM_TX_MODE_DIRECT,
    /* TxPeriodMsTrue */    0U,
    /* MinDelayMs */        0U,
    /* UpdateBitPosition */ 0xFFFFU,
    /* IpduGroupId */       COM_IPDU_GROUP_NONE
};

const Com_IPduConfigType kTestRxIPdu = {
    /* IPduId */            0U,
    /* DLC */               1U,
    /* PduRId */            0U,
    /* FirstTimeoutMs */    0U,
    /* TimeoutMs */         0U,
    /* IsSignalGroup */     0U,
    /* TxModeMode */        COM_TX_MODE_DIRECT,  // RX I-PDU では未使用
    /* TxPeriodMs */        0U,
    /* TxModeModeTrue */    COM_TX_MODE_DIRECT,
    /* TxPeriodMsTrue */    0U,
    /* MinDelayMs */        0U,
    /* UpdateBitPosition */ 0xFFFFU,
    /* IpduGroupId */       COM_IPDU_GROUP_NONE
};

// SignalId=0: 正常な TX シグナル（IPduId=0、登録済み）。
const Com_SignalConfigType kTestTxSignal = {
    /* SignalId */                0U,
    /* Direction */               COM_SIGNAL_DIRECTION_TX,
    /* IPduId */                  0U,
    /* BitPosition */             0U,
    /* BitSize */                 8U,
    /* Endian */                  COM_BIG_ENDIAN,
    /* InitValue */               0U,
    /* FilterAlgorithm */         COM_FILTER_ALWAYS,
    /* Mask */                    0xFFU,
    /* FilterX */                 0U,
    /* FilterMin */               0U,
    /* FilterMax */               0U,
    /* FilterRejectCbk */         NULL,
    /* TmsContributor */          0U,
    /* UpdateBitContributor */    0U,
    /* TransferProperty */        COM_TRANSFER_PROPERTY_PENDING,
    /* RxDataTimeoutAction */     COM_RX_TIMEOUT_ACTION_NONE,
    /* TimeoutSubstitutionValue */ 0U,
    /* DataInvalidAction */       COM_DATA_INVALID_ACTION_NONE,
    /* InvalidValue */            0U,
    /* InvalidNotificationCbk */  NULL,
    /* FirstTimeoutMs */          0U,
    /* TimeoutMs */               0U,
    /* RxTOutCbk */               NULL,
    /* TxAckCbk */                NULL,
    /* TxErrCbk */                NULL,
    /* RxAckCbk */                NULL,
    /* TxTOutCbk */               NULL,
    /* InvalidValueConfigured */  0U
};

// SignalId=1: 正常な RX シグナル（IPduId=0、登録済み）。
// Com_InvalidateSignal_NG_NotTxSignal がこの Direction=RX を利用する。
const Com_SignalConfigType kTestRxSignal = {
    /* SignalId */                1U,
    /* Direction */               COM_SIGNAL_DIRECTION_RX,
    /* IPduId */                  0U,
    /* BitPosition */             0U,
    /* BitSize */                 8U,
    /* Endian */                  COM_BIG_ENDIAN,
    /* InitValue */               0U,
    /* FilterAlgorithm */         COM_FILTER_ALWAYS,
    /* Mask */                    0xFFU,
    /* FilterX */                 0U,
    /* FilterMin */               0U,
    /* FilterMax */               0U,
    /* FilterRejectCbk */         NULL,
    /* TmsContributor */          0U,
    /* UpdateBitContributor */    0U,
    /* TransferProperty */        COM_TRANSFER_PROPERTY_PENDING,
    /* RxDataTimeoutAction */     COM_RX_TIMEOUT_ACTION_NONE,
    /* TimeoutSubstitutionValue */ 0U,
    /* DataInvalidAction */       COM_DATA_INVALID_ACTION_NONE,
    /* InvalidValue */            0U,
    /* InvalidNotificationCbk */  NULL,
    /* FirstTimeoutMs */          0U,
    /* TimeoutMs */               0U,
    /* RxTOutCbk */               NULL,
    /* TxAckCbk */                NULL,
    /* TxErrCbk */                NULL,
    /* RxAckCbk */                NULL,
    /* TxTOutCbk */               NULL,
    /* InvalidValueConfigured */  0U
};

// SignalId=2: IPduId が COM_TX_IPDU_MAX を超える、テーブル破損を模した TX シグナル。
const Com_SignalConfigType kCorruptTxSignal = {
    /* SignalId */  2U, COM_SIGNAL_DIRECTION_TX, kOutOfRangeIPduId,
    0U, 8U, COM_BIG_ENDIAN, 0U, COM_FILTER_ALWAYS, 0xFFU, 0U, 0U, 0U, NULL,
    0U, 0U, COM_TRANSFER_PROPERTY_PENDING, COM_RX_TIMEOUT_ACTION_NONE, 0U,
    COM_DATA_INVALID_ACTION_NONE, 0U, NULL, 0U, 0U, NULL, NULL, NULL, NULL, NULL, 0U
};

// SignalId=3: IPduId は範囲内だが TX I-PDU テーブルに未登録の TX シグナル。
const Com_SignalConfigType kUnregisteredTxSignal = {
    /* SignalId */  3U, COM_SIGNAL_DIRECTION_TX, kUnregisteredIPduId,
    0U, 8U, COM_BIG_ENDIAN, 0U, COM_FILTER_ALWAYS, 0xFFU, 0U, 0U, 0U, NULL,
    0U, 0U, COM_TRANSFER_PROPERTY_PENDING, COM_RX_TIMEOUT_ACTION_NONE, 0U,
    COM_DATA_INVALID_ACTION_NONE, 0U, NULL, 0U, 0U, NULL, NULL, NULL, NULL, NULL, 0U
};

// SignalId=4: IPduId が COM_RX_IPDU_MAX を超える、テーブル破損を模した RX シグナル。
const Com_SignalConfigType kCorruptRxSignal = {
    /* SignalId */  4U, COM_SIGNAL_DIRECTION_RX, kOutOfRangeIPduId,
    0U, 8U, COM_BIG_ENDIAN, 0U, COM_FILTER_ALWAYS, 0xFFU, 0U, 0U, 0U, NULL,
    0U, 0U, COM_TRANSFER_PROPERTY_PENDING, COM_RX_TIMEOUT_ACTION_NONE, 0U,
    COM_DATA_INVALID_ACTION_NONE, 0U, NULL, 0U, 0U, NULL, NULL, NULL, NULL, NULL, 0U
};

// SignalId=5: IPduId は範囲内だが RX I-PDU テーブルに未登録の RX シグナル。
const Com_SignalConfigType kUnregisteredRxSignal = {
    /* SignalId */  5U, COM_SIGNAL_DIRECTION_RX, kUnregisteredIPduId,
    0U, 8U, COM_BIG_ENDIAN, 0U, COM_FILTER_ALWAYS, 0xFFU, 0U, 0U, 0U, NULL,
    0U, 0U, COM_TRANSFER_PROPERTY_PENDING, COM_RX_TIMEOUT_ACTION_NONE, 0U,
    COM_DATA_INVALID_ACTION_NONE, 0U, NULL, 0U, 0U, NULL, NULL, NULL, NULL, NULL, 0U
};

// 受信デッドライン（100 ms）を持つ RX I-PDU（IPduId 0、DLC 1）と、そのシグナルの周りに、監視の対象にならないシグナルを並べた設定を、
// 各テストが組み立てる。設定テーブルは Com が参照し続けるため、テスト本体のローカル変数ではなく静的な領域に置く。
Com_IPduConfigType    gDlIPdu;
Com_SignalConfigType  gDlSignals[3];
Com_ConfigType        gDlConfig;

uint32 gGroupTimeoutCount;
void OnGroupRxTimeout() { gGroupTimeoutCount++; }

const Com_SignalIdType kDlRxSignalId      = 100U;
const Com_SignalIdType kDlTxSignalId      = 101U;
const Com_SignalIdType kDlCorruptSignalId = 102U;
const unsigned long    kDlTimeoutMs       = 100UL;

// 先頭に TX シグナル、2 番目に配列の範囲を超える IPduId の RX シグナル（どちらも監視しない）、3 番目に監視対象の RX シグナルを置く。
void PrepareDeadlineConfig()
{
    gDlIPdu = kTestRxIPdu;
    gDlIPdu.TimeoutMs = (uint16)kDlTimeoutMs;

    gDlSignals[0] = kTestTxSignal;
    gDlSignals[0].SignalId  = kDlTxSignalId;
    gDlSignals[0].TimeoutMs = (uint16)kDlTimeoutMs;   // 方向が TX のため、受信の監視の対象にならない
    gDlSignals[1] = kTestRxSignal;
    gDlSignals[1].SignalId  = kDlCorruptSignalId;
    gDlSignals[1].IPduId    = 200U;                    // 配列の範囲を超える（テーブルの破損を想定）
    gDlSignals[1].TimeoutMs = (uint16)kDlTimeoutMs;
    gDlSignals[2] = kTestRxSignal;
    gDlSignals[2].SignalId  = kDlRxSignalId;
    gDlSignals[2].TimeoutMs = (uint16)kDlTimeoutMs;
}

// 受信デッドラインは、最初のフレームを受信した後から監視する。1 バイトのフレームを、PduRId 0 の I-PDU で受信したことにする。
void ReceiveFirstFrame()
{
    uint8 frame[1] = { 0x01U };
    PduInfoType info = { frame, 1U };
    Com_RxIndication(0U, &info);
}

void InitWithDeadlineConfig()
{
    gDlConfig.RxIPdus        = &gDlIPdu;
    gDlConfig.RxIPduCount    = 1U;
    gDlConfig.TxIPdus        = &kTestTxIPdu;
    gDlConfig.TxIPduCount    = 1U;
    gDlConfig.Signals        = gDlSignals;
    gDlConfig.SignalCount    = 3U;
    gDlConfig.GwMappings     = NULL;
    gDlConfig.GwMappingCount = 0U;

    FakeDetHw_LogSuppressed = 1U;
    Com_DeInit();
    Com_Init(&gDlConfig);
    FakeDetHw_Reset();
    FakeDetHw_LogSuppressed = 0U;
}

const Com_SignalConfigType kTestSignals[] = {
    kTestTxSignal, kTestRxSignal, kCorruptTxSignal,
    kUnregisteredTxSignal, kCorruptRxSignal, kUnregisteredRxSignal
};

const Com_ConfigType kTestConfig = {
    &kTestRxIPdu, 1U,
    &kTestTxIPdu, 1U,
    kTestSignals, 6U,
    NULL, 0U
};

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* Com の単体テスト用フィクスチャ。
 * SetUp(): Com_Init(&kTestConfig) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): Com_DeInit() で未初期化へ戻す。 */
class Com_MainFunctionRx_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        Com_Init(&kTestConfig);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Com_DeInit();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Com_MainFunctionRx_Test, Com_MainFunctionRx_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Com_DeInit();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_MainFunctionRx();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, COM_E_UNINIT);
}

TEST_F(Com_MainFunctionRx_Test, Com_MainFunctionRx_OK_OnlyReceiveSignalsOfStartedIPduAreMonitored)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PrepareDeadlineConfig();
    InitWithDeadlineConfig();
    ReceiveFirstFrame();
    FakeMillis_Value += kDlTimeoutMs;   // 受信デッドラインが切れる

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_MainFunctionRx();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Com_Test_GetSigTimedOut(kDlRxSignalId), 1U);        // 監視対象の RX シグナルは、タイムアウトになる
    EXPECT_EQ(Com_Test_GetSigTimedOut(kDlTxSignalId), 0U);        // TX シグナルは対象外
    EXPECT_EQ(Com_Test_GetSigTimedOut(kDlCorruptSignalId), 0U);   // 範囲外の IPduId のシグナルも対象外
}

TEST_F(Com_MainFunctionRx_Test, Com_MainFunctionRx_OK_TimedOutIPduIsNotReportedAgain)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PrepareDeadlineConfig();
    InitWithDeadlineConfig();
    ReceiveFirstFrame();
    FakeMillis_Value += kDlTimeoutMs;
    Com_MainFunctionRx();
    ASSERT_EQ(Com_Test_GetRxTimedOut(0U), 1U);
    FakeMillis_Value += kDlTimeoutMs;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_MainFunctionRx();   // タイムアウト済みの状態で、もう一度判定する

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Com_Test_GetRxTimedOut(0U), 1U);   // タイムアウトのまま（解除されず、状態が壊れもしない）
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(Com_MainFunctionRx_Test, Com_MainFunctionRx_OK_SignalGroupWithoutTimeoutCallbackStillTimesOut)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PrepareDeadlineConfig();
    gDlIPdu.IsSignalGroup = 1U;   // Signal Group で、タイムアウトの通知関数が無い
    gDlIPdu.RxTOutCbk     = NULL;
    InitWithDeadlineConfig();
    ReceiveFirstFrame();
    FakeMillis_Value += kDlTimeoutMs;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_MainFunctionRx();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Com_Test_GetRxTimedOut(0U), 1U);   // 通知関数が無くても、タイムアウトとして扱う
}

TEST_F(Com_MainFunctionRx_Test, Com_MainFunctionRx_OK_SignalGroupTimeoutCallbackIsCalledOnce)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PrepareDeadlineConfig();
    gDlIPdu.IsSignalGroup = 1U;
    gDlIPdu.RxTOutCbk     = OnGroupRxTimeout;
    InitWithDeadlineConfig();
    ReceiveFirstFrame();
    gGroupTimeoutCount = 0U;
    FakeMillis_Value += kDlTimeoutMs;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_MainFunctionRx();
    FakeMillis_Value += kDlTimeoutMs;
    Com_MainFunctionRx();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(gGroupTimeoutCount, 1U);   // タイムアウトの通知は、1 回だけ
}

}  // namespace
