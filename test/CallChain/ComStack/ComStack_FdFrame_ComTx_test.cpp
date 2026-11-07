/**
 * \file    ComStack_FdFrame_ComTx_test.cpp
 * \brief   CAN FD の 64 byte のフレームの周期送信（Com_SendSignal → Com_MainFunctionTx → PduR → CanIf → Can_Write）の
 *          コールチェーンのテスト（GoogleTest / CMake native_chain_tests）。
 *
 * \details 本番の FdTestMsg（CAN 0x310、64 byte、PERIODIC）と同じ形の設定（先頭 byte と末尾 byte に
 *          8 bit のシグナルを 1 つずつ置く）を、テスト専用に定義して使う。
 *          ComStack_FdFrame_Tx_Test が CanIf_Transmit() 以下を検証済みのため、本テストは
 *          Com の層（64 byte のバッファへのパック、周期送信）から Can_Hw_Send() まで通ることに絞る。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Com.h"
#include "PduR.h"
#include "CanIf.h"
#include "Can.h"
#include "Fake_Can_Hw.h"
#include "Fake_Det_Hw.h"
#include "Fake_Millis.h"
#include "Wrap_Can.h"
#include "Wrap_CanIf.h"
#include "Wrap_PduR.h"
#include "Wrap_Com.h"
}

/* ======================================================================
 * Global Variables
 * ====================================================================== */

namespace
{

constexpr uint16 kPeriodMs = 1000U;

/* 先頭 byte（byte[0]）の 8 bit のシグナル */
const Com_SignalConfigType kHeadSignal = {
    /* SignalId */                0U,
    /* Direction */                COM_SIGNAL_DIRECTION_TX,
    /* IPduId */                   0U,
    /* BitPosition */              0U,
    /* BitSize */                  8U,
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
    /* TxErrCbk */                 NULL
};

/* 末尾 byte（byte[63]、ビット位置 504）の 8 bit のシグナル */
const Com_SignalConfigType kTailSignal = {
    /* SignalId */                1U,
    /* Direction */                COM_SIGNAL_DIRECTION_TX,
    /* IPduId */                   0U,
    /* BitPosition */              504U,
    /* BitSize */                  8U,
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
    /* TxErrCbk */                 NULL
};

const Com_SignalConfigType kSignals[] = { kHeadSignal, kTailSignal };

const Com_IPduConfigType kFdTxIPdu = {
    /* IPduId */           0U,
    /* DLC */              64U,
    /* PduRId */           0U,
    /* FirstTimeoutMs */   0U,
    /* TimeoutMs */        0U,
    /* IsSignalGroup */    0U,
    /* TxModeMode */       COM_TX_MODE_PERIODIC,
    /* TxPeriodMs */       kPeriodMs,
    /* TxModeModeTrue */   COM_TX_MODE_PERIODIC,
    /* TxPeriodMsTrue */   kPeriodMs,
    /* MinDelayMs */       0U,
    /* UpdateBitPosition */ 0xFFFFU,
    /* IpduGroupId */      COM_IPDU_GROUP_NONE,
    /* RxIndicationCbk */  NULL,
    /* TxTransformCbk */   NULL
};

const Com_ConfigType kComConfig = {
    /* RxIPdus */       NULL,
    /* RxIPduCount */   0U,
    /* TxIPdus */       &kFdTxIPdu,
    /* TxIPduCount */   1U,
    /* Signals */       kSignals,
    /* SignalCount */   2U,
    /* GwMappings */    NULL,
    /* GwMappingCount */ 0U
};

const PduR_TxRoutingPathType kPduRTxPath = {
    /* SrcPduId */             0U,
    /* CanIfTxPduId */         0U,
    /* ConfDestPduId */        0U,
    /* ConfFct */              Com_TxConfirmation,
    /* TransmitOverrideFct */  NULL,
    /* TransmitOverrideId */   0U
};

const PduR_PBConfigType kPduRConfig = {
    /* RxPaths */     NULL,
    /* RxPathCount */ 0U,
    /* TxPaths */     &kPduRTxPath,
    /* TxPathCount */ 1U
};

const CanIf_TxPduConfigType kCanIfTxPdu = {
    /* UpperLayerTxPduId */ 0U,
    /* CanId */             0x310U,
    /* Dlc */               64U,
    /* Hth */               0U,
    /* TxConfirmFct */      PduR_CanIfTxConfirmation
};

const CanIf_ConfigType kCanIfConfig = {
    /* TxPduConfig */ &kCanIfTxPdu,
    /* TxPduCount */  1U,
    /* RxPduConfig */ NULL,
    /* RxPduCount */  0U
};

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* 64 byte の周期送信フレームのコールチェーンのフィクスチャ。
 * SetUp(): 時刻・模擬 Can_Hw・wrap をリセットし、Can_Init()（CAN FD 有効）→ CanIf_Init() → コントローラを STARTED → PDU を ONLINE
 *          → PduR_Init() → Com_Init() の順に初期化する（Init 中の DET ログは抑制する）。
 * TearDown(): Com_DeInit() と CanIf_DeInit() で未初期化へ戻す。 */
class ComStack_FdFrame_ComTx_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;

        FakeMillis_Reset();  // Com_Init() が時刻を取り込むため、Com_Init() より前にリセットする
        FakeCanHw_Reset();
        WrapCan_Reset();
        WrapCanIf_Reset();
        WrapPduR_Reset();
        WrapCom_Reset();

        canConfig.filter.filterId = 0x0220U;
        canConfig.filter.mask     = 0x1FFFU;
        canConfig.csPin           = 10U;
        canConfig.intPin          = 2U;
        canConfig.baudrate        = 500000U;
        canConfig.crystalFreq     = CAN_CRYSTAL_16MHZ;
        canConfig.fdEnabled       = 1U;

        Can_Test_SetConfigPtr(NULL);
        Can_Init(&canConfig);
        CanIf_Init(&kCanIfConfig);
        CanIf_SetControllerMode(0U, CAN_CS_STARTED);
        CanIf_SetPduMode(0U, CANIF_ONLINE);
        PduR_Init(&kPduRConfig);
        Com_Init(&kComConfig);

        FakeDetHw_LogSuppressed = 0U;
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Com_DeInit();
        CanIf_DeInit();
    }

    Can_ConfigType canConfig;
};

}  // namespace

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(ComStack_FdFrame_ComTx_Test, OK_Step01_PeriodicFrameCarriesHeadAndTailAs64Bytes)
{
    /* ---- 準備 (Arrange) ---- */
    const uint8 head = 0x5AU;
    const uint8 tail = 0xA5U;
    ASSERT_EQ(Com_SendSignal(0U, &head), E_OK);
    ASSERT_EQ(Com_SendSignal(1U, &tail), E_OK);

    /* ---- 実行 (Act) ---- */
    FakeMillis_Value += kPeriodMs;  // 周期が経過した
    Com_MainFunctionTx();

    /* ---- 評価 (Assert) ---- */
    EXPECT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x310U);
    EXPECT_EQ(FakeCanHw_LastSendDlc, 64U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x5AU);
    EXPECT_EQ(FakeCanHw_LastSendData[63], 0xA5U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x00U);   // 間の byte は初期値のまま
    EXPECT_EQ(FakeCanHw_LastSendData[62], 0x00U);
}

TEST_F(ComStack_FdFrame_ComTx_Test, OK_Step02_NoSendBeforePeriodElapses)
{
    /* ---- 準備 (Arrange) ---- */
    FakeMillis_Value += (kPeriodMs - 1U);

    /* ---- 実行 (Act) ---- */
    Com_MainFunctionTx();

    /* ---- 評価 (Assert) ---- */
    EXPECT_EQ(FakeCanHw_SendCount, 0U);
}
