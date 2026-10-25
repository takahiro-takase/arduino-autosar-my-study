/**
 * \file    Com_SendSignalGroup_test.cpp
 * \brief   Com_SendSignalGroup() の単体テスト
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

// update-bit（byte 1 の最下位ビット）を持つ TX Signal Group（IPduId 0、DLC 2）と、そのメンバー（SignalId 60）で Com を初期化し直す。
// 設定テーブルは Com が参照し続けるため、テスト本体のローカル変数ではなく静的な領域に置く。
Com_IPduConfigType    gTxGroupIPdu;
Com_SignalConfigType  gTxGroupSignal;
Com_ConfigType        gTxGroupConfig;

const Com_SignalIdType kTxGroupSignalId = 60U;

// 呼ぶ前に、各テストが gTxGroupIPdu / gTxGroupSignal のうち変えたい項目だけを書き換える。
void PrepareTxGroupConfig(Com_TransferPropertyType property)
{
    gTxGroupIPdu = kTestTxIPdu;
    gTxGroupIPdu.DLC               = 2U;
    gTxGroupIPdu.IsSignalGroup     = 1U;
    gTxGroupIPdu.UpdateBitPosition = 15U;
    gTxGroupSignal = kTestTxSignal;
    gTxGroupSignal.SignalId         = kTxGroupSignalId;
    gTxGroupSignal.TransferProperty = property;
}

void InitWithTxGroupConfig()
{
    gTxGroupConfig.RxIPdus        = NULL;
    gTxGroupConfig.RxIPduCount    = 0U;
    gTxGroupConfig.TxIPdus        = &gTxGroupIPdu;
    gTxGroupConfig.TxIPduCount    = 1U;
    gTxGroupConfig.Signals        = &gTxGroupSignal;
    gTxGroupConfig.SignalCount    = 1U;
    gTxGroupConfig.GwMappings     = NULL;
    gTxGroupConfig.GwMappingCount = 0U;

    FakeDetHw_LogSuppressed = 1U;
    Com_DeInit();
    Com_Init(&gTxGroupConfig);
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
class Com_SendSignalGroup_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
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

TEST_F(Com_SendSignalGroup_Test, Com_SendSignalGroup_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Com_DeInit();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    uint8 ret = Com_SendSignalGroup(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, COM_E_UNINIT);
}

TEST_F(Com_SendSignalGroup_Test, Com_SendSignalGroup_NG_IdOutOfRange)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    uint8 ret = Com_SendSignalGroup(kOutOfRangeIPduId);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, COM_E_PARAM);
}

TEST_F(Com_SendSignalGroup_Test, Com_SendSignalGroup_NG_NotFoundOrNotGroup)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    // IPduId=0 は登録済みだが IsSignalGroup=0（kTestTxIPdu）。
    uint8 ret = Com_SendSignalGroup(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, COM_E_PARAM);
}

TEST_F(Com_SendSignalGroup_Test, Com_SendSignalGroup_NG_UnregisteredId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // IPduId=1 は配列範囲内だが、I-PDU 設定テーブルに登録されていない。


    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    uint8 ret = Com_SendSignalGroup(kUnregisteredIPduId);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, COM_E_PARAM);
}

TEST_F(Com_SendSignalGroup_Test, Com_SendSignalGroup_OK_ChangedTriggeredMemberRequestsTransmitAndSetsUpdateBit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PrepareTxGroupConfig(COM_TRANSFER_PROPERTY_TRIGGERED_ON_CHANGE);
    InitWithTxGroupConfig();
    const uint8 value = 0x55U;   // 前回の値（InitValue 0）から変わる
    ASSERT_EQ(Com_SendSignal(kTxGroupSignalId, &value), E_OK);
    ASSERT_EQ(Com_Test_GetTxPending(0U), 0U);   // シャドウバッファへ書いただけでは、送信を予約しない

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const uint8 ret = Com_SendSignalGroup(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(Com_Test_GetTxPending(0U), 1U);                  // グループ全体の送信を予約する
    EXPECT_EQ(Com_Test_GetTxBuffer(0U)[0], 0x55U);
    EXPECT_NE(Com_Test_GetTxBuffer(0U)[1] & 0x01U, 0U);        // update-bit が立つ
}

TEST_F(Com_SendSignalGroup_Test, Com_SendSignalGroup_OK_UnchangedTriggeredMemberDoesNotRequestTransmit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PrepareTxGroupConfig(COM_TRANSFER_PROPERTY_TRIGGERED_ON_CHANGE);
    InitWithTxGroupConfig();
    const uint8 value = 0U;   // InitValue（0）と同じ値
    ASSERT_EQ(Com_SendSignal(kTxGroupSignalId, &value), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const uint8 ret = Com_SendSignalGroup(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(Com_Test_GetTxPending(0U), 0U);                  // 値が変わっていないので、送信を予約しない
    EXPECT_EQ(Com_Test_GetTxBuffer(0U)[1] & 0x01U, 0U);        // update-bit も立てない
}

TEST_F(Com_SendSignalGroup_Test, Com_SendSignalGroup_OK_PendingMemberNeverRequestsTransmit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PrepareTxGroupConfig(COM_TRANSFER_PROPERTY_PENDING);
    InitWithTxGroupConfig();
    const uint8 value = 0x55U;   // 値は変わるが、PENDING は送信のきっかけにならない
    ASSERT_EQ(Com_SendSignal(kTxGroupSignalId, &value), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const uint8 ret = Com_SendSignalGroup(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(Com_Test_GetTxPending(0U), 0U);
    EXPECT_EQ(Com_Test_GetTxBuffer(0U)[0], 0x55U);   // 値は、他の送信に便乗して運ばれるよう、バッファへ反映する
}

TEST_F(Com_SendSignalGroup_Test, Com_SendSignalGroup_OK_PeriodicIPduSetsUpdateBitWithoutOnChangeRequest)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PrepareTxGroupConfig(COM_TRANSFER_PROPERTY_TRIGGERED_ON_CHANGE);
    gTxGroupIPdu.TxModeMode     = COM_TX_MODE_PERIODIC;
    gTxGroupIPdu.TxModeModeTrue = COM_TX_MODE_PERIODIC;
    gTxGroupIPdu.TxPeriodMs     = 100U;
    gTxGroupIPdu.TxPeriodMsTrue = 100U;
    InitWithTxGroupConfig();
    const uint8 value = 0x55U;
    ASSERT_EQ(Com_SendSignal(kTxGroupSignalId, &value), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const uint8 ret = Com_SendSignalGroup(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(Com_Test_GetTxPending(0U), 0U);            // 周期送信の I-PDU は、値の変化では送らない（周期が運ぶ）
    EXPECT_NE(Com_Test_GetTxBuffer(0U)[1] & 0x01U, 0U);  // update-bit は、トリガーされたので立てる
}

TEST_F(Com_SendSignalGroup_Test, Com_SendSignalGroup_OK_GroupWithoutUpdateBitStillRequestsTransmitOnChange)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PrepareTxGroupConfig(COM_TRANSFER_PROPERTY_TRIGGERED_ON_CHANGE);
    gTxGroupIPdu.UpdateBitPosition = 0xFFU;   // update-bit を使わない
    InitWithTxGroupConfig();
    const uint8 value = 0x55U;
    ASSERT_EQ(Com_SendSignal(kTxGroupSignalId, &value), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const uint8 ret = Com_SendSignalGroup(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(Com_Test_GetTxPending(0U), 1U);
    EXPECT_EQ(Com_Test_GetTxBuffer(0U)[0], 0x55U);
}

}  // namespace
