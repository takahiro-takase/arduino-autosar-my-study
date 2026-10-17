/**
 * \file    Com_ReceiveSignal_test.cpp
 * \brief   Com_ReceiveSignal() の単体テスト
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

// 受信シグナル 1 つ（SignalId 20、8 ビット）と、それを持つ RX I-PDU（DLC 1 バイト）を、各テストが組み立てる。
// 設定テーブルは Com が参照し続けるため、テスト本体のローカル変数ではなく静的な領域に置く。
Com_IPduConfigType    gRxIPdu;
Com_SignalConfigType  gRxSignal;
Com_ConfigType        gRxConfig;

const Com_SignalIdType kRxSignalId    = 20U;
const uint8            kInitValue     = 0x42U;
const unsigned long    kTimeoutMs     = 100UL;

// gRxSignal / gRxIPdu を既定（kTestRxSignal / kTestRxIPdu。InitValue は kInitValue）にして、Com を初期化し直す。
// 呼ぶ前に、各テストが gRxSignal / gRxIPdu のうち変えたい項目だけを書き換えられるよう、準備と初期化を分けてある。
void PrepareRxConfig()
{
    gRxIPdu    = kTestRxIPdu;
    gRxSignal  = kTestRxSignal;
    gRxSignal.SignalId  = kRxSignalId;
    gRxSignal.InitValue = kInitValue;
}

void InitWithRxConfig()
{
    gRxConfig.RxIPdus     = &gRxIPdu;
    gRxConfig.RxIPduCount = 1U;
    gRxConfig.TxIPdus     = NULL;
    gRxConfig.TxIPduCount = 0U;
    gRxConfig.Signals     = &gRxSignal;
    gRxConfig.SignalCount = 1U;
    gRxConfig.GwMappings     = NULL;
    gRxConfig.GwMappingCount = 0U;

    FakeDetHw_LogSuppressed = 1U;
    Com_DeInit();
    Com_Init(&gRxConfig);
    FakeDetHw_Reset();
    FakeDetHw_LogSuppressed = 0U;
}

// 通知関数が呼ばれた回数。
uint32 gInvalidNotifyCount;
uint32 gFilterRejectCount;
void OnInvalidNotify() { gInvalidNotifyCount++; }
void OnFilterReject()  { gFilterRejectCount++; }

// 1 バイトのデータを、受信したことにする。
void ReceiveByte(uint8 value)
{
    uint8 frame[1] = { value };
    PduInfoType info = { frame, 1U };
    Com_RxIndication(0U, &info);
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
class Com_ReceiveSignal_Test : public ::testing::Test
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

TEST_F(Com_ReceiveSignal_Test, Com_ReceiveSignal_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Com_DeInit();
    uint8 data = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    uint8 ret = Com_ReceiveSignal(1U, &data);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, COM_E_UNINIT);
}

TEST_F(Com_ReceiveSignal_Test, Com_ReceiveSignal_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    uint8 ret = Com_ReceiveSignal(1U, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, COM_E_PARAM_POINTER);
}

TEST_F(Com_ReceiveSignal_Test, Com_ReceiveSignal_NG_IPduIdOutOfRange)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 data = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    uint8 ret = Com_ReceiveSignal(4U, &data);  // kCorruptRxSignal

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, COM_E_PARAM);
}

TEST_F(Com_ReceiveSignal_Test, Com_ReceiveSignal_NG_NotRegisteredRxIPdu)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 data = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    uint8 ret = Com_ReceiveSignal(5U, &data);  // kUnregisteredRxSignal

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, COM_E_PARAM);
}

TEST_F(Com_ReceiveSignal_Test, Com_ReceiveSignal_NG_SignalNotFound)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 data = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    uint8 ret = Com_ReceiveSignal(99U, &data);  // 未登録の SignalId

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, COM_E_PARAM);
}

TEST_F(Com_ReceiveSignal_Test, Com_ReceiveSignal_NG_TimedOutSignalWithoutActionFails)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PrepareRxConfig();
    gRxIPdu.TimeoutMs   = (uint16)kTimeoutMs;
    gRxSignal.TimeoutMs = (uint16)kTimeoutMs;
    gRxSignal.RxDataTimeoutAction = COM_RX_TIMEOUT_ACTION_NONE;
    InitWithRxConfig();
    ReceiveByte(0x11U);
    FakeMillis_Value += kTimeoutMs;   // 受信デッドラインが切れる
    Com_MainFunctionRx();
    uint8 value = 0xFFU;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const uint8 ret = Com_ReceiveSignal(kRxSignalId, &value);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);   // 代わりの値が設定されていないので、失敗として返す
}

TEST_F(Com_ReceiveSignal_Test, Com_ReceiveSignal_OK_TimedOutSignalIsReplacedByInitValue)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PrepareRxConfig();
    gRxIPdu.TimeoutMs  = (uint16)kTimeoutMs;
    gRxSignal.TimeoutMs = (uint16)kTimeoutMs;
    gRxSignal.RxDataTimeoutAction = COM_RX_TIMEOUT_ACTION_REPLACE;
    InitWithRxConfig();
    ReceiveByte(0x11U);
    FakeMillis_Value += kTimeoutMs;   // 受信デッドラインが切れる
    Com_MainFunctionRx();
    uint8 value = 0xFFU;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const uint8 ret = Com_ReceiveSignal(kRxSignalId, &value);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(value, kInitValue);   // タイムアウトした値の代わりに、InitValue を返す
}

TEST_F(Com_ReceiveSignal_Test, Com_ReceiveSignal_OK_InvalidValueWithNotifyReturnsLastValidValue)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PrepareRxConfig();
    gRxSignal.InvalidValue      = 0xFFU;
    gRxSignal.DataInvalidAction = COM_DATA_INVALID_ACTION_NOTIFY;
    InitWithRxConfig();
    ReceiveByte(0x11U);
    uint8 first = 0U;
    ASSERT_EQ(Com_ReceiveSignal(kRxSignalId, &first), E_OK);
    ASSERT_EQ(first, 0x11U);   // 有効な値を、最後の有効値として覚える
    ReceiveByte(0xFFU);        // 無効値を受信する
    uint8 value = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const uint8 ret = Com_ReceiveSignal(kRxSignalId, &value);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(value, 0x11U);   // 無効値は上位へ渡さず、最後の有効値を返す
}

TEST_F(Com_ReceiveSignal_Test, Com_ReceiveSignal_OK_InvalidValueWithReplaceReturnsInitValue)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PrepareRxConfig();
    gRxSignal.InvalidValue      = 0xFFU;
    gRxSignal.DataInvalidAction = COM_DATA_INVALID_ACTION_REPLACE;
    InitWithRxConfig();
    ReceiveByte(0xFFU);   // 無効値を受信する
    uint8 value = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const uint8 ret = Com_ReceiveSignal(kRxSignalId, &value);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(value, kInitValue);   // 無効値の代わりに、InitValue を返す
}

TEST_F(Com_ReceiveSignal_Test, Com_ReceiveSignal_OK_ValueOutsideFilterRangeReturnsLastValidValue)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PrepareRxConfig();
    gRxSignal.FilterAlgorithm = COM_FILTER_NEW_IS_WITHIN;
    gRxSignal.FilterMin       = 10U;
    gRxSignal.FilterMax       = 20U;
    InitWithRxConfig();
    ReceiveByte(15U);
    uint8 first = 0U;
    ASSERT_EQ(Com_ReceiveSignal(kRxSignalId, &first), E_OK);
    ASSERT_EQ(first, 15U);   // 範囲内の値は、最後の有効値になる
    ReceiveByte(30U);        // 範囲の外の値を受信する
    uint8 value = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const uint8 ret = Com_ReceiveSignal(kRxSignalId, &value);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(value, 15U);   // 範囲の外の値は上位へ渡さず、最後の有効値を返す
}

TEST_F(Com_ReceiveSignal_Test, Com_ReceiveSignal_OK_FrameIsIgnoredWhileReceptionIsSuppressed)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PrepareRxConfig();
    InitWithRxConfig();
    ReceiveByte(0x11U);
    Com_SetCommunicationEnabled(FALSE, TRUE);   // 受信だけを止める（診断の CommunicationControl 相当）
    ReceiveByte(0x22U);
    uint8 value = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const uint8 ret = Com_ReceiveSignal(kRxSignalId, &value);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(value, 0x11U);   // 止めている間のフレームは取り込まない
}

TEST_F(Com_ReceiveSignal_Test, Com_ReceiveSignal_OK_FrameForUnknownPduIdIsIgnored)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PrepareRxConfig();
    InitWithRxConfig();
    uint8 frame[1] = { 0x22U };
    PduInfoType info = { frame, 1U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_RxIndication(7U, &info);   // どの RX I-PDU にも割り当てられていない PduId
    uint8 value = 0U;
    const uint8 ret = Com_ReceiveSignal(kRxSignalId, &value);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(value, kInitValue);   // 取り込まれていない（初期化時の InitValue のまま）
    EXPECT_EQ(FakeDetHw_LastErrorId, 0xFFU);   // DET も報告しない
}

TEST_F(Com_ReceiveSignal_Test, Com_ReceiveSignal_OK_InvalidValueNotificationIsCalledFromMainFunctionRx)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PrepareRxConfig();
    gRxSignal.InvalidValue           = 0xFFU;
    gRxSignal.DataInvalidAction      = COM_DATA_INVALID_ACTION_NOTIFY;
    gRxSignal.InvalidNotificationCbk = OnInvalidNotify;
    InitWithRxConfig();
    gInvalidNotifyCount = 0U;
    ReceiveByte(0xFFU);
    uint8 value = 0U;
    (void)Com_ReceiveSignal(kRxSignalId, &value);   // 無効値を読んだ（通知が予約される）
    ASSERT_EQ(gInvalidNotifyCount, 0U);             // 読んだ時点では呼ばない

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_MainFunctionRx();
    const uint32 afterFirst = gInvalidNotifyCount;
    Com_MainFunctionRx();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(afterFirst, 1U);
    EXPECT_EQ(gInvalidNotifyCount, 1U);   // 1 回の予約に対して 1 回だけ呼ぶ
}

TEST_F(Com_ReceiveSignal_Test, Com_ReceiveSignal_OK_FilterRejectNotificationIsCalledFromMainFunctionRx)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PrepareRxConfig();
    gRxSignal.FilterAlgorithm = COM_FILTER_NEW_IS_WITHIN;
    gRxSignal.FilterMin       = 10U;
    gRxSignal.FilterMax       = 20U;
    gRxSignal.FilterRejectCbk = OnFilterReject;
    InitWithRxConfig();
    gFilterRejectCount = 0U;
    ReceiveByte(30U);   // 範囲の外
    uint8 value = 0U;
    (void)Com_ReceiveSignal(kRxSignalId, &value);   // 読んだ（通知が予約される）
    ASSERT_EQ(gFilterRejectCount, 0U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_MainFunctionRx();
    const uint32 afterFirst = gFilterRejectCount;
    Com_MainFunctionRx();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(afterFirst, 1U);
    EXPECT_EQ(gFilterRejectCount, 1U);
}

TEST_F(Com_ReceiveSignal_Test, Com_ReceiveSignal_OK_ReenablingReceptionRestartsDeadlineMonitoring)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PrepareRxConfig();
    gRxIPdu.TimeoutMs   = (uint16)kTimeoutMs;
    gRxSignal.TimeoutMs = (uint16)kTimeoutMs;
    gRxSignal.RxDataTimeoutAction = COM_RX_TIMEOUT_ACTION_REPLACE;
    InitWithRxConfig();
    ReceiveByte(0x11U);
    Com_SetCommunicationEnabled(FALSE, TRUE);   // 受信を止める
    FakeMillis_Value += 2U * kTimeoutMs;        // 止めている間に、デッドライン分の時間が過ぎる
    Com_SetCommunicationEnabled(TRUE, TRUE);    // 受信を再開する
    uint8 value = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_MainFunctionRx();   // 再開の直後
    const uint8 ret = Com_ReceiveSignal(kRxSignalId, &value);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(value, 0x11U);   // 止めていた時間は数えず、タイムアウトにはしない（InitValue には置き換わらない）
}

TEST_F(Com_ReceiveSignal_Test, Com_ReceiveSignal_OK_FrameIsIgnoredWhileIPduGroupIsStopped)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PrepareRxConfig();
    gRxIPdu.IpduGroupId = 3U;   // まだ開始していない I-PDU Group
    InitWithRxConfig();
    ReceiveByte(0x22U);
    uint8 value = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const uint8 ret = Com_ReceiveSignal(kRxSignalId, &value);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, COM_SERVICE_NOT_AVAILABLE);
    EXPECT_EQ(value, kInitValue);   // 停止中のフレームは取り込まない
}

TEST_F(Com_ReceiveSignal_Test, Com_ReceiveSignal_OK_LittleEndianSignalIsUnpackedBitReversedWithinByte)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PrepareRxConfig();
    gRxSignal.Endian = COM_LITTLE_ENDIAN;
    InitWithRxConfig();
    ReceiveByte(0x01U);
    uint8 value = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const uint8 ret = Com_ReceiveSignal(kRxSignalId, &value);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(value, 0x80U);   // 本実装のビット番号は byte 内で最上位ビットが 0 番。リトルエンディアンは 0 番を値の最下位ビットにするため、ビットが反転して見える
}

}  // namespace
