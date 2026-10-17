/**
 * \file    Com_IpduGroupStart_test.cpp
 * \brief   Com_IpduGroupStart() の単体テスト
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
    /* UpdateBitPosition */ 0xFFU,
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
    /* UpdateBitPosition */ 0xFFU,
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

// 所属グループ（I-PDU Group の ID。本番の COM_IPDU_GROUP_* とは無関係）。
const Com_IpduGroupIdType kGroup = 5U;

// I-PDU Group へ所属する RX/TX I-PDU（各 1 つ、DLC 2 バイト）とそのシグナルを、各テストが組み立てる。
// 設定テーブルは Com が参照し続けるため、テスト本体のローカル変数ではなく静的な領域に置く。
Com_IPduConfigType    gGroupRxIPdu;
Com_IPduConfigType    gGroupTxIPdu;
Com_SignalConfigType  gGroupSignals[2];
Com_ConfigType        gGroupConfig;

const Com_SignalIdType kGroupRxSignalId = 10U;
const Com_SignalIdType kGroupTxSignalId = 11U;
const uint8            kRxInitValue     = 0x42U;
const uint8            kTxInitValue     = 0x24U;
const uint8            kUpdateBitPos    = 15U;   // 2 バイト目（byte 1）の最下位ビット

// グループへ所属する I-PDU と、InitValue を持つ RX/TX シグナル 1 つずつで、Com を初期化し直す。
// signalGroup が真なら、両方の I-PDU を Signal Group にする。
void InitWithGroupMembers(uint8 signalGroup)
{
    gGroupRxIPdu = kTestRxIPdu;
    gGroupRxIPdu.DLC           = 2U;
    gGroupRxIPdu.IsSignalGroup = signalGroup;
    gGroupRxIPdu.IpduGroupId   = kGroup;

    gGroupTxIPdu = kTestTxIPdu;
    gGroupTxIPdu.DLC               = 2U;
    gGroupTxIPdu.IsSignalGroup     = signalGroup;
    gGroupTxIPdu.IpduGroupId       = kGroup;
    gGroupTxIPdu.UpdateBitPosition = kUpdateBitPos;

    gGroupSignals[0] = kTestRxSignal;
    gGroupSignals[0].SignalId  = kGroupRxSignalId;
    gGroupSignals[0].InitValue = kRxInitValue;
    gGroupSignals[1] = kTestTxSignal;
    gGroupSignals[1].SignalId             = kGroupTxSignalId;
    gGroupSignals[1].InitValue            = kTxInitValue;
    gGroupSignals[1].UpdateBitContributor = 1U;

    gGroupConfig.RxIPdus     = &gGroupRxIPdu;
    gGroupConfig.RxIPduCount = 1U;
    gGroupConfig.TxIPdus     = &gGroupTxIPdu;
    gGroupConfig.TxIPduCount = 1U;
    gGroupConfig.Signals     = gGroupSignals;
    gGroupConfig.SignalCount = 2U;
    gGroupConfig.GwMappings     = NULL;
    gGroupConfig.GwMappingCount = 0U;

    FakeDetHw_LogSuppressed = 1U;
    Com_DeInit();
    Com_Init(&gGroupConfig);
    FakeDetHw_Reset();
    FakeDetHw_LogSuppressed = 0U;
}

// 上の設定に、所属していない別の I-PDU（RX/TX とも IPduId 1、グループ無し）とそのシグナルを足した設定。
Com_IPduConfigType    gOtherRxIPdus[2];
Com_IPduConfigType    gOtherTxIPdus[2];
Com_SignalConfigType  gOtherSignals[4];
Com_ConfigType        gOtherConfig;

const Com_SignalIdType kOtherRxSignalId = 12U;
const Com_SignalIdType kOtherTxSignalId = 13U;

void InitWithOtherIPdus()
{
    InitWithGroupMembers(0U);   // gGroupRxIPdu / gGroupTxIPdu / gGroupSignals[2] を組み立てる

    gOtherRxIPdus[0] = gGroupRxIPdu;
    gOtherRxIPdus[1] = kTestRxIPdu;
    gOtherRxIPdus[1].IPduId = 1U;
    gOtherRxIPdus[1].PduRId = 1U;
    gOtherTxIPdus[0] = gGroupTxIPdu;
    gOtherTxIPdus[1] = kTestTxIPdu;
    gOtherTxIPdus[1].IPduId = 1U;

    gOtherSignals[0] = gGroupSignals[0];
    gOtherSignals[1] = gGroupSignals[1];
    gOtherSignals[2] = kTestRxSignal;
    gOtherSignals[2].SignalId  = kOtherRxSignalId;
    gOtherSignals[2].IPduId    = 1U;
    gOtherSignals[2].InitValue = kRxInitValue;
    gOtherSignals[3] = kTestTxSignal;
    gOtherSignals[3].SignalId  = kOtherTxSignalId;
    gOtherSignals[3].IPduId    = 1U;
    gOtherSignals[3].InitValue = kTxInitValue;

    gOtherConfig.RxIPdus        = gOtherRxIPdus;
    gOtherConfig.RxIPduCount    = 2U;
    gOtherConfig.TxIPdus        = gOtherTxIPdus;
    gOtherConfig.TxIPduCount    = 2U;
    gOtherConfig.Signals        = gOtherSignals;
    gOtherConfig.SignalCount    = 4U;
    gOtherConfig.GwMappings     = NULL;
    gOtherConfig.GwMappingCount = 0U;

    FakeDetHw_LogSuppressed = 1U;
    Com_DeInit();
    Com_Init(&gOtherConfig);
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
class Com_IpduGroupStart_Test : public ::testing::Test
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

// ------------------------------------------------------------
// Com_IpduGroupStart()/Com_IpduGroupStop()/Com_EnableReceptionDM()/
// Com_DisableReceptionDM()（いずれも Uninit チェックのみ）
// ------------------------------------------------------------
TEST_F(Com_IpduGroupStart_Test, Com_IpduGroupStart_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Com_DeInit();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_IpduGroupStart(0U, FALSE);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, COM_E_UNINIT);
}

TEST_F(Com_IpduGroupStart_Test, Com_IpduGroupStart_OK_InitializeResetsReceivedSignalsToInitValue)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    InitWithGroupMembers(0U);
    Com_IpduGroupStart(kGroup, FALSE);
    uint8 frame[2] = { 0x77U, 0x00U };
    PduInfoType info = { frame, 2U };
    Com_RxIndication(0U, &info);
    uint8 received = 0U;
    ASSERT_EQ(Com_ReceiveSignal(kGroupRxSignalId, &received), E_OK);
    ASSERT_EQ(received, 0x77U);   // 受信した値が見えている

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_IpduGroupStart(kGroup, TRUE);   // 初期化付きで再始動する
    uint8 afterStart = 0xFFU;
    const uint8 ret = Com_ReceiveSignal(kGroupRxSignalId, &afterStart);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(afterStart, kRxInitValue);   // 受信バッファが InitValue に戻る
}

TEST_F(Com_IpduGroupStart_Test, Com_IpduGroupStart_OK_InitializeResetsSignalGroupShadowBuffersToInitValue)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    InitWithGroupMembers(1U);
    Com_IpduGroupStart(kGroup, FALSE);
    const uint8 txValue = 0x55U;
    ASSERT_EQ(Com_SendSignal(kGroupTxSignalId, &txValue), E_OK);   // 送信側のシャドウバッファへ書く

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_IpduGroupStart(kGroup, TRUE);
    ASSERT_EQ(Com_SendSignalGroup(0U), E_OK);   // シャドウバッファを実バッファへ反映する
    const uint8* tx = Com_Test_GetTxBuffer(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_NE(tx, nullptr);
    EXPECT_EQ(tx[0], kTxInitValue);   // 再始動前に書いた値ではなく、InitValue が送られる
}

TEST_F(Com_IpduGroupStart_Test, Com_IpduGroupStart_OK_InitializeResetsTransmitBufferToInitValue)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    InitWithGroupMembers(0U);
    Com_IpduGroupStart(kGroup, FALSE);
    const uint8 txValue = 0x55U;
    ASSERT_EQ(Com_SendSignal(kGroupTxSignalId, &txValue), E_OK);
    ASSERT_EQ(Com_Test_GetTxBuffer(0U)[0], 0x55U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_IpduGroupStart(kGroup, TRUE);
    const uint8* tx = Com_Test_GetTxBuffer(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_NE(tx, nullptr);
    EXPECT_EQ(tx[0], kTxInitValue);
}

TEST_F(Com_IpduGroupStart_Test, Com_IpduGroupStart_OK_RestartClearsUpdateBit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    InitWithGroupMembers(0U);
    Com_IpduGroupStart(kGroup, FALSE);
    const uint8 txValue = 0x55U;
    ASSERT_EQ(Com_SendSignal(kGroupTxSignalId, &txValue), E_OK);   // update-bit が立つ
    ASSERT_NE(Com_Test_GetTxBuffer(0U)[1] & 0x01U, 0U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_IpduGroupStart(kGroup, FALSE);   // 初期化なしで再始動する

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Com_Test_GetTxBuffer(0U)[1] & 0x01U, 0U);   // update-bit だけがクリアされる
    EXPECT_EQ(Com_Test_GetTxBuffer(0U)[0], 0x55U);        // 値は残る
}

TEST_F(Com_IpduGroupStart_Test, Com_IpduGroupStart_OK_InitializeDoesNotTouchSignalsOfOtherIPdus)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    InitWithOtherIPdus();
    Com_IpduGroupStart(kGroup, FALSE);
    uint8 frame[1] = { 0x77U };
    PduInfoType info = { frame, 1U };
    Com_RxIndication(1U, &info);   // グループに属さない RX I-PDU（PduRId 1）が、値を受け取る
    const uint8 txValue = 0x55U;
    ASSERT_EQ(Com_SendSignal(kOtherTxSignalId, &txValue), E_OK);   // グループに属さない TX I-PDU へ、値を書く

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_IpduGroupStart(kGroup, TRUE);   // 別のグループの I-PDU だけが初期化される
    uint8 received = 0U;
    const uint8 retRx = Com_ReceiveSignal(kOtherRxSignalId, &received);
    const uint8* tx = Com_Test_GetTxBuffer(1U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(retRx, E_OK);
    EXPECT_EQ(received, 0x77U);   // 受信した値は、そのまま
    ASSERT_NE(tx, nullptr);
    EXPECT_EQ(tx[0], 0x55U);      // 送信バッファも、そのまま
}

}  // namespace
