/**
 * \file    Com_MainFunctionTx_test.cpp
 * \brief   Com_MainFunctionTx() の単体テスト
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
#include "Wrap_PduR.h"
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

// 送信確認のタイムアウト通知（シグナル単位）が呼ばれた回数。
uint32 gTimeoutCount;
void OnTxTimeout() { gTimeoutCount++; }

// 送信シグナル 1 つ（SignalId 80）を持つ TX I-PDU（IPduId 0、DLC 1）を、各テストが組み立てる。
// 設定テーブルは Com が参照し続けるため、テスト本体のローカル変数ではなく静的な領域に置く。
Com_IPduConfigType    gTxIPdu;
Com_SignalConfigType  gTxSignal;
Com_ConfigType        gTxConfig;

const Com_SignalIdType kTxSignalId = 80U;

// mode と period（ms）を持つ TX I-PDU の設定を用意する。呼ぶ前後に、各テストが変えたい項目だけを書き換える。
void PrepareTxConfig(Com_TxModeModeType mode, uint16 periodMs)
{
    gTxIPdu = kTestTxIPdu;
    gTxIPdu.TxModeMode     = mode;
    gTxIPdu.TxModeModeTrue = mode;
    gTxIPdu.TxPeriodMs     = periodMs;
    gTxIPdu.TxPeriodMsTrue = periodMs;
    gTxSignal = kTestTxSignal;
    gTxSignal.SignalId         = kTxSignalId;
    gTxSignal.TransferProperty = COM_TRANSFER_PROPERTY_TRIGGERED_ON_CHANGE;
}

void InitWithTxConfig()
{
    gTxConfig.RxIPdus        = NULL;
    gTxConfig.RxIPduCount    = 0U;
    gTxConfig.TxIPdus        = &gTxIPdu;
    gTxConfig.TxIPduCount    = 1U;
    gTxConfig.Signals        = &gTxSignal;
    gTxConfig.SignalCount    = 1U;
    gTxConfig.GwMappings     = NULL;
    gTxConfig.GwMappingCount = 0U;

    FakeDetHw_LogSuppressed = 1U;
    Com_DeInit();
    Com_Init(&gTxConfig);
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
class Com_MainFunctionTx_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        WrapPduR_Reset();
        FailFromCallCount_PduR_ComTransmit = 0U;   // PduR への送信は、常に成功する（呼ばれた回数を数える）
        ForcedReturn_PduR_ComTransmit      = E_OK;
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        Com_Init(&kTestConfig);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        WrapPduR_Reset();   // 故障注入を残さない
        Com_DeInit();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Com_MainFunctionTx_Test, Com_MainFunctionTx_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Com_DeInit();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_MainFunctionTx();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, COM_E_UNINIT);
}

TEST_F(Com_MainFunctionTx_Test, Com_MainFunctionTx_OK_PeriodicIPduIsSentOnlyWhenPeriodElapses)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PrepareTxConfig(COM_TX_MODE_PERIODIC, 100U);
    InitWithTxConfig();
    const uint32 callsBefore = CallCount_PduR_ComTransmit;
    FakeMillis_Value += 99UL;
    Com_MainFunctionTx();   // 周期の手前
    const uint32 callsBeforePeriod = CallCount_PduR_ComTransmit;
    FakeMillis_Value += 1UL;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_MainFunctionTx();   // 周期が経過する

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(callsBeforePeriod, callsBefore);                 // 周期の手前では送らない
    EXPECT_EQ(CallCount_PduR_ComTransmit, callsBefore + 1U);   // 周期が経過したら送る
}

TEST_F(Com_MainFunctionTx_Test, Com_MainFunctionTx_OK_MixedIPduIsSentByPeriodWithoutAnyChange)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PrepareTxConfig(COM_TX_MODE_MIXED, 100U);
    InitWithTxConfig();
    const uint32 callsBefore = CallCount_PduR_ComTransmit;
    FakeMillis_Value += 99UL;
    Com_MainFunctionTx();   // 周期の手前（値も変わっていない）
    const uint32 callsBeforePeriod = CallCount_PduR_ComTransmit;
    FakeMillis_Value += 1UL;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_MainFunctionTx();   // 周期が経過する

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(callsBeforePeriod, callsBefore);
    EXPECT_EQ(CallCount_PduR_ComTransmit, callsBefore + 1U);   // 変化が無くても、周期の分は送る
}

TEST_F(Com_MainFunctionTx_Test, Com_MainFunctionTx_OK_DirectIPduWithoutChangeIsNotSentByPeriod)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PrepareTxConfig(COM_TX_MODE_DIRECT, 100U);
    InitWithTxConfig();
    const uint32 callsBefore = CallCount_PduR_ComTransmit;
    FakeMillis_Value += 1000UL;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_MainFunctionTx();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_PduR_ComTransmit, callsBefore);   // DIRECT は周期では送らない（変化・トリガーだけ）
}

TEST_F(Com_MainFunctionTx_Test, Com_MainFunctionTx_OK_ChangeIsHeldBackUntilMinimumDelayElapses)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PrepareTxConfig(COM_TX_MODE_DIRECT, 0U);
    gTxIPdu.MinDelayMs = 50U;   // 最小送信間隔 50 ms
    InitWithTxConfig();
    const uint8 value = 0x55U;
    ASSERT_EQ(Com_SendSignal(kTxSignalId, &value), E_OK);   // 値が変わり、送信が予約される
    const uint32 callsBefore = CallCount_PduR_ComTransmit;
    FakeMillis_Value += 49UL;
    Com_MainFunctionTx();   // 最小送信間隔の手前
    const uint32 callsBeforeDelay = CallCount_PduR_ComTransmit;
    FakeMillis_Value += 1UL;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_MainFunctionTx();   // 最小送信間隔が経過する

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(callsBeforeDelay, callsBefore);                  // 間隔が空くまで、予約した送信は待つ
    EXPECT_EQ(CallCount_PduR_ComTransmit, callsBefore + 1U);
}

TEST_F(Com_MainFunctionTx_Test, Com_MainFunctionTx_OK_ConfirmationTimeoutIsNotifiedOnlyOnce)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PrepareTxConfig(COM_TX_MODE_DIRECT, 0U);
    gTxIPdu.TxTimeoutMs      = 100U;
    gTxIPdu.TxFirstTimeoutMs = 100U;   // 最初の送信から監視する
    gTxSignal.TxTOutCbk   = OnTxTimeout;
    InitWithTxConfig();
    gTimeoutCount = 0U;
    const uint8 value = 0x55U;
    ASSERT_EQ(Com_SendSignal(kTxSignalId, &value), E_OK);
    Com_MainFunctionTx();   // 送信する（確認はまだ来ない）
    FakeMillis_Value += 100UL;   // 確認のデッドラインが切れる
    Com_MainFunctionTx();
    const uint32 afterFirst = gTimeoutCount;
    FakeMillis_Value += 100UL;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_MainFunctionTx();   // 通知済みの I-PDU は、続けて通知しない

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(afterFirst, 1U);
    EXPECT_EQ(gTimeoutCount, 1U);
}

}  // namespace
