/**
 * \file    Com_TxConfirmation_test.cpp
 * \brief   Com_TxConfirmation() の単体テスト
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

// 送信確認の通知関数が呼ばれた回数。
uint32 gSignalAckCount;
uint32 gGroupAckCount;
void OnSignalAck() { gSignalAckCount++; }
void OnGroupAck()  { gGroupAckCount++; }

// 送信シグナル 1 つ（SignalId 40）を持つ TX I-PDU（IPduId 0）で、Com を初期化し直す。
// 設定テーブルは Com が参照し続けるため、テスト本体のローカル変数ではなく静的な領域に置く。
Com_IPduConfigType    gAckIPdu;
Com_SignalConfigType  gAckSignal;
Com_ConfigType        gAckConfig;

void InitWithAckCallbacks(uint8 signalGroup, Com_IpduGroupIdType ipduGroupId)
{
    gAckIPdu = kTestTxIPdu;
    gAckIPdu.IsSignalGroup = signalGroup;
    gAckIPdu.IpduGroupId   = ipduGroupId;
    gAckIPdu.TxAckCbk      = OnGroupAck;
    gAckSignal = kTestTxSignal;
    gAckSignal.SignalId = 40U;
    gAckSignal.TxAckCbk = OnSignalAck;

    gAckConfig.RxIPdus        = NULL;
    gAckConfig.RxIPduCount    = 0U;
    gAckConfig.TxIPdus        = &gAckIPdu;
    gAckConfig.TxIPduCount    = 1U;
    gAckConfig.Signals        = &gAckSignal;
    gAckConfig.SignalCount    = 1U;
    gAckConfig.GwMappings     = NULL;
    gAckConfig.GwMappingCount = 0U;

    FakeDetHw_LogSuppressed = 1U;
    Com_DeInit();
    Com_Init(&gAckConfig);
    FakeDetHw_Reset();
    FakeDetHw_LogSuppressed = 0U;
    gSignalAckCount = 0U;
    gGroupAckCount  = 0U;
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

// ComTxIPduClearUpdateBit=Confirmation の確認用の設定（update-bit は byte1 の bit0）。
// 設定テーブルは Com が参照し続けるため、静的な領域に置く。
Com_IPduConfigType   gCuIPdu;
Com_SignalConfigType gCuSignal;
Com_ConfigType       gCuConfig;

const Com_SignalIdType kCuSignalId = 5U;

// repetitions: ComTxModeNumberOfRepetitions（SWS_Com_00305）。
void InitClearUpdateBitConfirmation(Com_TxClearUpdateBitType clearUpdateBit, uint8 repetitions)
{
    gCuIPdu = {};
    gCuIPdu.IPduId             = 0U;
    gCuIPdu.DLC                = 2U;
    gCuIPdu.PduRId             = 0U;
    gCuIPdu.TxModeMode         = COM_TX_MODE_DIRECT;
    gCuIPdu.TxModeModeTrue     = COM_TX_MODE_DIRECT;
    gCuIPdu.NumberOfRepetitions = repetitions;
    gCuIPdu.RepetitionPeriodMs = 100U;
    gCuIPdu.UpdateBitPosition  = 15U;
    gCuIPdu.IpduGroupId        = COM_IPDU_GROUP_NONE;
    gCuIPdu.ClearUpdateBit     = clearUpdateBit;

    gCuSignal = {};
    gCuSignal.SignalId             = kCuSignalId;
    gCuSignal.Direction            = COM_SIGNAL_DIRECTION_TX;
    gCuSignal.IPduId               = 0U;
    gCuSignal.BitPosition          = 0U;
    gCuSignal.BitSize              = 8U;
    gCuSignal.Endian               = COM_BIG_ENDIAN;
    gCuSignal.FilterAlgorithm      = COM_FILTER_ALWAYS;
    gCuSignal.Mask                 = 0xFFU;
    gCuSignal.UpdateBitContributor = 1U;
    gCuSignal.TransferProperty     = COM_TRANSFER_PROPERTY_TRIGGERED_ON_CHANGE;
    gCuSignal.RxDataTimeoutAction  = COM_RX_TIMEOUT_ACTION_NONE;
    gCuSignal.DataInvalidAction    = COM_DATA_INVALID_ACTION_NONE;

    gCuConfig = {};
    gCuConfig.TxIPdus     = &gCuIPdu;
    gCuConfig.TxIPduCount = 1U;
    gCuConfig.Signals     = &gCuSignal;
    gCuConfig.SignalCount = 1U;

    FakeMillis_Reset();   // Com_Init() が時刻を取り込むため、Init より前にリセットする
    WrapPduR_Reset();
    FailFromCallCount_PduR_ComTransmit = 0U;
    ForcedReturn_PduR_ComTransmit      = E_OK;
    FakeDetHw_LogSuppressed = 1U;
    Com_DeInit();
    Com_Init(&gCuConfig);
    FakeDetHw_Reset();
    FakeDetHw_LogSuppressed = 0U;
}

// update-bit（byte1 の bit0）を返す。
uint8 UpdateBit()
{
    return (uint8)(Com_Test_GetTxBuffer(0U)[1] & 0x01U);
}

// 値を書いて update-bit を立て、Com_MainFunctionTx() で送信する（PduR_ComTransmit() が 1 回呼ばれる）。
void WriteAndTransmit(uint8 value)
{
    (void)Com_SendSignal(kCuSignalId, &value);
    Com_MainFunctionTx();
}

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* Com の単体テスト用フィクスチャ。
 * SetUp(): Com_Init(&kTestConfig) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): Com_DeInit() で未初期化へ戻す。 */
class Com_TxConfirmation_Test : public ::testing::Test
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

TEST_F(Com_TxConfirmation_Test, Com_TxConfirmation_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Com_DeInit();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_TxConfirmation(0U, E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, COM_E_UNINIT);
}

TEST_F(Com_TxConfirmation_Test, Com_TxConfirmation_NG_StoppedIPduConfirmationIsIgnored)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    InitWithAckCallbacks(0U, 3U);   // I-PDU Group 3 に所属し、まだ開始していない

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_TxConfirmation(0U, E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(gSignalAckCount, 0U);   // 停止中の I-PDU への確認は、通知しない
}

TEST_F(Com_TxConfirmation_Test, Com_TxConfirmation_NG_FailedConfirmationDoesNotCallAckCallback)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    InitWithAckCallbacks(0U, COM_IPDU_GROUP_NONE);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_TxConfirmation(0U, E_NOT_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(gSignalAckCount, 0U);   // 送信に失敗した確認では、成功の通知を呼ばない
}

TEST_F(Com_TxConfirmation_Test, Com_TxConfirmation_OK_IdBeyondTableIsIgnoredWithoutError)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）。範囲外の ID は、停止中かどうかの配列参照より前に除外される。

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_TxConfirmation(200U, E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, 0xFFU);   // 配列範囲外を読まず、DET も報告しない
}

TEST_F(Com_TxConfirmation_Test, Com_TxConfirmation_OK_SignalAckCallbackIsCalled)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    InitWithAckCallbacks(0U, COM_IPDU_GROUP_NONE);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_TxConfirmation(0U, E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(gSignalAckCount, 1U);
    EXPECT_EQ(gGroupAckCount, 0U);   // Signal Group ではないので、グループ単位の通知は呼ばない
}

TEST_F(Com_TxConfirmation_Test, Com_TxConfirmation_OK_SignalGroupAckCallbackIsCalled)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    InitWithAckCallbacks(1U, COM_IPDU_GROUP_NONE);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_TxConfirmation(0U, E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(gGroupAckCount, 1U);
    EXPECT_EQ(gSignalAckCount, 0U);   // Signal Group では、グループ単位の通知だけを呼ぶ
}

TEST_F(Com_TxConfirmation_Test, Com_TxConfirmation_OK_ClearsUpdateBitWhenConfiguredConfirmation)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    InitClearUpdateBitConfirmation(COM_CLEAR_UPDATE_BIT_CONFIRMATION, 0U);
    WriteAndTransmit(0x55U);
    ASSERT_EQ(CallCount_PduR_ComTransmit, 1U);
    ASSERT_EQ(UpdateBit(), 1U);   // Transmit 直後には、クリアしない

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_TxConfirmation(0U, E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(UpdateBit(), 0U);   // 確認が届いたあとに、クリアする（[SWS_Com_00577]）
}

TEST_F(Com_TxConfirmation_Test, Com_TxConfirmation_OK_DoesNotClearUpdateBitWhenConfiguredTransmit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    InitClearUpdateBitConfirmation(COM_CLEAR_UPDATE_BIT_TRANSMIT, 0U);
    WriteAndTransmit(0x55U);
    ASSERT_EQ(UpdateBit(), 0U);   // Transmit 設定は、送信の直後にクリアする
    const uint8 again = 0x66U;
    (void)Com_SendSignal(kCuSignalId, &again);   // update-bit を立て直す
    ASSERT_EQ(UpdateBit(), 1U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_TxConfirmation(0U, E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(UpdateBit(), 1U);   // Transmit 設定は、確認ではクリアしない
}

TEST_F(Com_TxConfirmation_Test, Com_TxConfirmation_NG_FailedConfirmationKeepsUpdateBit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    InitClearUpdateBitConfirmation(COM_CLEAR_UPDATE_BIT_CONFIRMATION, 0U);
    WriteAndTransmit(0x55U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_TxConfirmation(0U, E_NOT_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(UpdateBit(), 1U);   // 送信の失敗の通知では、クリアしない（確認されていない）
}

TEST_F(Com_TxConfirmation_Test, Com_TxConfirmation_OK_KeepsUpdateBitSetAgainAfterTransmit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    InitClearUpdateBitConfirmation(COM_CLEAR_UPDATE_BIT_CONFIRMATION, 0U);
    WriteAndTransmit(0x55U);
    const uint8 newer = 0x66U;
    (void)Com_SendSignal(kCuSignalId, &newer);   // 送信してから確認が届くまでの間に、新しい値を書く

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_TxConfirmation(0U, E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(UpdateBit(), 1U);                              // 新しい値の update-bit は、消さない
    EXPECT_EQ(Com_Test_GetTxBuffer(0U)[0], 0x66U);
}

TEST_F(Com_TxConfirmation_Test, Com_TxConfirmation_OK_ClearsUpdateBitAtNextConfirmationAfterNewerTransmit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    InitClearUpdateBitConfirmation(COM_CLEAR_UPDATE_BIT_CONFIRMATION, 0U);
    WriteAndTransmit(0x55U);
    const uint8 newer = 0x66U;
    (void)Com_SendSignal(kCuSignalId, &newer);
    Com_TxConfirmation(0U, E_OK);                // 1 回目の確認では、クリアしない
    ASSERT_EQ(UpdateBit(), 1U);
    Com_MainFunctionTx();                        // 新しい値を送信する
    ASSERT_EQ(CallCount_PduR_ComTransmit, 2U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_TxConfirmation(0U, E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(UpdateBit(), 0U);   // 新しい値の送信の確認で、クリアする
}

TEST_F(Com_TxConfirmation_Test, Com_TxConfirmation_OK_ClearsUpdateBitAtLastConfirmationOfRealRepeatSequence)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    InitClearUpdateBitConfirmation(COM_CLEAR_UPDATE_BIT_CONFIRMATION, 1U);   // 1 回の再送（合計 2 回の送信）
    WriteAndTransmit(0x55U);                       // 1 回目の送信（再送が 1 回残る）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_TxConfirmation(0U, E_OK);                  // 1 回目の確認
    const uint8 afterFirstConfirm = UpdateBit();
    FakeMillis_Value += 100U;                      // 再送の間隔（RepetitionPeriodMs）を経過させる
    Com_MainFunctionTx();                          // 2 回目の送信（再送を使い切る）
    const uint32 transmits = CallCount_PduR_ComTransmit;
    const uint8 afterSecondTransmit = UpdateBit();
    Com_TxConfirmation(0U, E_OK);                  // 2 回目の確認

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(afterFirstConfirm, 1U);              // 再送が残っている間は、クリアしない
    EXPECT_EQ(transmits, 2U);
    EXPECT_EQ(afterSecondTransmit, 1U);            // 再送の送信の直後にも、クリアしない
    EXPECT_EQ(UpdateBit(), 0U);                    // 最後の送信の確認で、クリアする
}

}  // namespace
