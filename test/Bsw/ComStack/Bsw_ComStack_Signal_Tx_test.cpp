/**
 * \file    Bsw_ComStack_Signal_Tx_test.cpp
 * \brief   README.md「Tx 処理（Com → PduR → CanIf → Can の順）」コールチェーンの
 *          単体テスト（GoogleTest / CMake native_chain_tests）。通常のシグナル
 *          （非 Signal Group）送信シナリオ専用。
 *
 * \details 2026-09、`Bsw_ComStack_Tx_{ComMainFunction,ComSendSignal,
 *          RepetitionSequence,TxIpduCallout,TxTOut,InvalidateSignal,
 *          NonGroupTmsTransition,SwitchIpduTxMode,TriggerIPDUSend}_test.cpp`
 *          （2026-09-20/21 に COM 内部メカニズムごとへ分割していた19ファイル）
 *          を、Msg内容（Signal/SignalGroup/E2E/SecOC）× 方向（Tx/Rx）の
 *          `Bsw_ComStack_{MsgType}_{Tx|Rx}_test.cpp` 命名規則へ統合し直した
 *          （ユーザー指示）。分割時の各ファイルはCOM_TX_IPDU_MAXの制約上、
 *          4 IPdu・7シグナルの共有configをそのまま複製していたが、本ファイルが
 *          実際に使うのは IPduId=0（通常の非グループシグナル）と IPduId=2
 *          （非グループだが変則的にTMSへ寄与する特殊ケース）のみのため、
 *          必要な分だけの最小configへ組み直している。IPduId=1（TMS Pending
 *          シグナルグループ）・IPduId=3（停止可能グループ）は
 *          `Bsw_ComStack_SignalGroup_Tx_test.cpp` 側へ移した。
 *
 *          SignalId=0 (TX, 16bit BigEndian) 1本だけを持つ IPduId=0 の TX I-PDU。
 *          PduR は SrcPduId=0 を CanIfTxPduId=0 へ直結。CanIf の TxPduId=0 は
 *          CAN ID=0x100, DLC=2 で Can_Write(Hth=0, ...) を呼ぶ。
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
#include "Wrap_Can.h"
#include "Wrap_CanIf.h"
#include "Wrap_PduR.h"
#include "Wrap_Com.h"
}

namespace
{

// SWS_Com_00878（TX 送信デッドライン監視、Com_CbkTxTOut）検証用のカウンタ
// 付きコールバック。kTestSignal（非 Signal Group、IPduId=0）に設定する
// （Com_InvokeTxNotification() は非 Signal Group の I-PDU ではシグナル単位の
// TxTOutCbk を配送する。Com_IPduConfigType.TxTOutCbk は Signal Group 専用）。
static uint8_t s_txTOutCount = 0U;
static void TestTxTOutCbk(void) { s_txTOutCount++; }

// Com_TxIpduCallout（SWS_Com_00346、TX I-PDU 単位のフィルタリングフック）
// 検証用。kTestTxIPdu（IPduId=0）に設定する。s_txCalloutAccept で戻り値を
// 切り替えられるトグル式（Bsw_ComStack_Signal_Rx_test.cpp の TestRxIpduCallout と対称）。
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
    kTestSignal, kTestNonGroupTmsContributorSignal,
    kTestNonGroupTmsCalledSignal, kTestInvalidateRxSignal
};
const Com_IPduConfigType   kTestTxIPdus[] = { kTestTxIPdu, kTestNonGroupTmsIPdu };

const Com_ConfigType kTestComConfig = {
    /* RxIPdus */       NULL,
    /* RxIPduCount */   0U,
    /* TxIPdus */       kTestTxIPdus,
    /* TxIPduCount */   2U,
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

class Bsw_ComStack_Signal_Tx_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();  // Com_Init() が millis() を Com_TxLastSentMs[] へ
                              // 取り込むため、Com_Init() より前にリセットする
        FakeCanHw_Reset();
        WrapCan_Reset();
        WrapCanIf_Reset();
        WrapPduR_Reset();
        WrapCom_Reset();
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
        s_txTOutCount      = 0U;
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
// Com_MainFunctionTx() ─ Com_TxPending というフラグで切れる非同期境界
// ------------------------------------------------------------
TEST_F(Bsw_ComStack_Signal_Tx_Test, ComMainFunction_OK_DrivesToCanHwSend)
{
    /* 準備 (Arrange): セグメント①の終端状態（Com_TxPending が立った状態）を用意する */
    uint16_t value = 0x1234U;
    Com_SendSignal(0U, &value);
    ASSERT_EQ(Com_Test_GetTxPending(0U), 1U);

    /* 実行 (Act) */
    FakeDetHw_LogSuppressed = 0U;  // ログ出力
    Com_MainFunctionTx();
    FakeDetHw_LogSuppressed = 1U;  // ログ抑制

    /* 評価 (Assert) */
    EXPECT_EQ(Com_Test_GetTxPending(0U), 0U);  // 送信要求が消費された
    EXPECT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x100U);   // CanIf_TxPduConfigType.CanId
    EXPECT_EQ(FakeCanHw_LastSendDlc, 2U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x12U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x34U);
}


TEST_F(Bsw_ComStack_Signal_Tx_Test, ComMainFunction_NG_NothingPending_DoesNotReachCanHw)
{
    /* 準備 (Arrange): Com_SendSignal() を呼ばない（Com_TxPending が立っていない） */

    /* 実行 (Act) */
    FakeDetHw_LogSuppressed = 0U;  // ログ出力
    Com_MainFunctionTx();
    FakeDetHw_LogSuppressed = 1U;  // ログ抑制

    /* 評価 (Assert) */
    EXPECT_EQ(FakeCanHw_SendCount, 0U);
}


TEST_F(Bsw_ComStack_Signal_Tx_Test, ComMainFunction_NG_Can_Write_CAN_BUSY)
{
    /* 準備 (Arrange): セグメント①の終端状態（Com_TxPending が立った状態）を
     * 用意した上で、Can_Write() を強制的に CAN_BUSY で失敗させる
     * （stub/Bsw/Can/Wrap_Can.h 参照）。Com_SendSignal() を呼ばないと
     * Com_MainFunctionTx() が Can_Write() 自体を呼ばず、
     * ComMainFunction_NG_NothingPending_DoesNotReachCanHw と区別が
     * つかなくなってしまう点に注意。 */
    uint16_t value = 0x1234U;
    Com_SendSignal(0U, &value);
    ASSERT_EQ(Com_Test_GetTxPending(0U), 1U);
    FailFromCallCount_Can_Write = 1U;
    ForcedReturn_Can_Write = CAN_BUSY;

    /* 実行 (Act) */
    FakeDetHw_LogSuppressed = 0U;  // ログ出力
    Com_MainFunctionTx();
    FakeDetHw_LogSuppressed = 1U;  // ログ抑制

    /* 評価 (Assert) */
    EXPECT_EQ(FakeCanHw_SendCount, 0U);
}


// ------------------------------------------------------------
// Com_SendSignal() ─ シャドウバッファへの書き込みと Com_TxPending の設定
// ------------------------------------------------------------
TEST_F(Bsw_ComStack_Signal_Tx_Test, ComSendSignal_OK_SetsPendingAndPacksBuffer)
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


TEST_F(Bsw_ComStack_Signal_Tx_Test, ComSendSignal_NG_UnknownSignalId_DoesNotSetPending)
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
// ComTxModeNumberOfRepetitions（SWS_Com_00305）: 送信後の自動リピート
// ------------------------------------------------------------
TEST_F(Bsw_ComStack_Signal_Tx_Test, RepetitionSequence_OK_FiresConfiguredNumberOfRepeatsThenStops)
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


TEST_F(Bsw_ComStack_Signal_Tx_Test, RepetitionSequence_NG_DoesNotFireBeforePeriodElapsed)
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


TEST_F(Bsw_ComStack_Signal_Tx_Test, RepetitionSequence_OK_NewSendSignalRestartsSequence)
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


TEST_F(Bsw_ComStack_Signal_Tx_Test, RepetitionSequence_OK_InitialSendDoesNotConsumeRepeatBudgetEvenWhenElapsedAlreadyExceedsPeriod)
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


TEST_F(Bsw_ComStack_Signal_Tx_Test, RepetitionSequence_OK_DoesNotConsumeBudgetWhileCommunicationControlDisabled)
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


// ------------------------------------------------------------
// Com_TxIpduCallout（SWS_Com_00346、TX I-PDU 単位のフィルタリングフック）
// ------------------------------------------------------------
TEST_F(Bsw_ComStack_Signal_Tx_Test, TxIpduCallout_OK_AcceptedTransmitsNormally)
{
    /* 準備 (Arrange): s_txCalloutAccept は SetUp() で 1（既定）にリセット済み */
    uint16_t value = 0x1234U;
    Com_SendSignal(0U, &value);

    /* 実行 (Act) */
    FakeDetHw_LogSuppressed = 0U;  // ログ出力
    Com_MainFunctionTx();
    FakeDetHw_LogSuppressed = 1U;  // ログ抑制

    /* 評価 (Assert): callout は送信直前の最終バイト列で 1 回呼ばれ、
     * 通常どおり Can_Hw まで到達する。実際に PduR へ渡したため
     * Com_TxConfPending もセットされる。 */
    EXPECT_EQ(s_txCalloutInvokeCount, 1U);
    EXPECT_EQ(s_txCalloutLastByte0, 0x12U);
    EXPECT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(Com_Test_GetTxConfPending(0U), 1U);
}


TEST_F(Bsw_ComStack_Signal_Tx_Test, TxIpduCallout_NG_RejectedDiscardsTransmission)
{
    /* 準備 (Arrange) */
    s_txCalloutAccept = 0U;
    uint16_t value = 0x1234U;
    Com_SendSignal(0U, &value);

    /* 実行 (Act) */
    FakeDetHw_LogSuppressed = 0U;  // ログ出力
    Com_MainFunctionTx();
    FakeDetHw_LogSuppressed = 1U;  // ログ抑制

    /* 評価 (Assert): [SWS_Com_00346] false のため PduR_ComTransmit() 以降
     * （CanIf/Can/Can_Hw）に一切到達しない。実際には送信していないため
     * Com_TxConfPending もセットされない（TX 送信デッドライン監視タイマも
     * 起動しない）。 */
    EXPECT_EQ(s_txCalloutInvokeCount, 1U);
    EXPECT_EQ(FakeCanHw_SendCount, 0U);
    EXPECT_EQ(Com_Test_GetTxConfPending(0U), 0U);
}


// ------------------------------------------------------------
// Com_CbkTxTOut（SWS_Com_00878、送信デッドライン監視）
// ------------------------------------------------------------
TEST_F(Bsw_ComStack_Signal_Tx_Test, TxTOut_OK_FiresAfterFirstTimeoutWhenArmedAndUnconfirmed)
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


TEST_F(Bsw_ComStack_Signal_Tx_Test, TxTOut_OK_ConfirmationBeforeDeadlineCancelsIt)
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


TEST_F(Bsw_ComStack_Signal_Tx_Test, TxTOut_OK_UsesSteadyTimeoutAfterFirstConfirmedCycle)
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


TEST_F(Bsw_ComStack_Signal_Tx_Test, TxTOut_OK_RepeatsDoNotRestartOrExtendDeadline)
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


// ------------------------------------------------------------
// Com_InvalidateSignal（SWS_Com_00099、2026-08 追加）
// ------------------------------------------------------------
TEST_F(Bsw_ComStack_Signal_Tx_Test, InvalidateSignal_OK_WritesConfiguredInvalidValueToBuffer)
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


TEST_F(Bsw_ComStack_Signal_Tx_Test, InvalidateSignal_NG_UnconfiguredInvalidValueReturnsServiceNotAvailableWithoutWriting)
{
    /* 準備 (Arrange): kTestNonGroupTmsCalledSignal（SignalId=5、IPduId=2）は
     * InvalidValueConfigured が既定の 0（未設定）のまま。 */

    /* 実行 (Act) */
    uint8 ret = Com_InvalidateSignal(5U);

    /* 評価 (Assert): [SWS_Com_00643] 原文どおり ComSignalDataInvalidValue
     * 未設定のため COM_SERVICE_NOT_AVAILABLE（2026-09-20 是正。以前は
     * COM_SERVICE_NOT_AVAILABLE 定数が存在せず E_NOT_OK で代用していた）。
     * 副作用（バッファ書き込み）も一切起きない。 */
    EXPECT_EQ(ret, COM_SERVICE_NOT_AVAILABLE);
    const uint8* buf = Com_Test_GetTxBuffer(2U);
    ASSERT_NE(buf, nullptr);
    // bit0 は kTestNonGroupTmsContributorSignal（SignalId=4、InitValue=1）が
    // Com_Init() 時点で既にパック済み（0x80）。SignalId=5（bit1）側は
    // 今回の呼び出しが失敗したので変化しない。
    EXPECT_EQ(buf[0], 0x80U);
}


TEST_F(Bsw_ComStack_Signal_Tx_Test, InvalidateSignal_NG_UnknownSignalIdReturnsError)
{
    /* 実行 (Act) + 評価 (Assert) */
    EXPECT_EQ(Com_InvalidateSignal(255U), E_NOT_OK);
}


TEST_F(Bsw_ComStack_Signal_Tx_Test, InvalidateSignal_NG_RxSignalReturnsErrorWithoutReachingSendSignal)
{
    /* 準備 (Arrange): kTestInvalidateRxSignal（SignalId=8、Direction=RX）は
     * InvalidValueConfigured=1（誤設定された想定）だが、RX/TX の IPduId が
     * 数値空間を共有するため、Direction チェックが無いと Com_SendSignal()
     * 側で偶然一致する TX I-PDU を静かに書き換えかねない（/code-review 指摘）。 */

    /* 実行 (Act) + 評価 (Assert): Direction チェックのみで拒否される */
    EXPECT_EQ(Com_InvalidateSignal(8U), E_NOT_OK);
}


// ------------------------------------------------------------
// SWS_Com_00495 の非 Signal Group 側経路（Com_SendSignal() 内の
// tmsChanged 分岐、IPduId=2）
// ------------------------------------------------------------
TEST_F(Bsw_ComStack_Signal_Tx_Test, NonGroupTmsTransition_OK_TriggersImmediateSendEvenWhenCalledSignalFilterFails)
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
// Com_TriggerIPDUSend（SWS_Com_00861/SWS_Com_00388）
// ------------------------------------------------------------
TEST_F(Bsw_ComStack_Signal_Tx_Test, TriggerIPDUSend_OK_ForcesDispatchWithoutValueChange)
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


TEST_F(Bsw_ComStack_Signal_Tx_Test, TriggerIPDUSend_NG_UnknownPduIdReturnsError)
{
    /* 実行 (Act) + 評価 (Assert) */
    EXPECT_EQ(Com_TriggerIPDUSend(99U), E_NOT_OK);
}


TEST_F(Bsw_ComStack_Signal_Tx_Test, TriggerIPDUSend_OK_DoesNotConsumeNumberOfRepetitionsBudget)
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
// Com_SwitchIpduTxMode/Com_TriggerIPDUSend が実効 TxModeMode を PERIODIC に
// 遷移させる場合の周期タイマ再始動（[SWS_Com_00244]）専用の独立した
// フィクスチャ。COM_TX_IPDU_MAX の制約により上記の共有 kTestComConfig とは
// 別の最小 Com_ConfigType を必要とするため（tx_switch_periodic/
// tx_trigger_periodic の2箇所で同型のSetUp()/TearDown()が必要なため
// 共通基底クラスへ切り出す。/code-review 指摘、rule of three）。
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

class Bsw_ComStack_Signal_Tx_SwitchIpduTxModePeriodic_Test : public IsolatedComTxFixtureBase
{
protected:
    const Com_ConfigType* GetComConfig() const override { return &kTestComConfig; }
};

TEST_F(Bsw_ComStack_Signal_Tx_SwitchIpduTxModePeriodic_Test, SwitchIpduTxMode_OK_RestartsPeriodicTimerOnTransitionIntoPeriodic)
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

namespace tx_trigger_periodic
{

const Com_IPduConfigType kTestPeriodicIPdu = {
    /* IPduId */           0U,
    /* DLC */              1U,
    /* PduRId */           0U,   // PduR_Init() を呼ばないため未登録のまま
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

class Bsw_ComStack_Signal_Tx_TriggerIPDUSendPeriodic_Test : public IsolatedComTxFixtureBase
{
protected:
    const Com_ConfigType* GetComConfig() const override { return &kTestComConfig; }
};

TEST_F(Bsw_ComStack_Signal_Tx_TriggerIPDUSendPeriodic_Test, TriggerIPDUSend_OK_FiresBetweenPeriodsOnceMdtElapses)
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

TEST_F(Bsw_ComStack_Signal_Tx_TriggerIPDUSendPeriodic_Test, TriggerIPDUSend_NG_DoesNotFireBeforePeriodElapsedWithoutTrigger)
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

}  // namespace
