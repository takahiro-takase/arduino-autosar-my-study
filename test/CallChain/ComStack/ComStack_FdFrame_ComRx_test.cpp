/**
 * ile    ComStack_FdFrame_ComRx_test.cpp
 * rief   CAN FD の 64 byte のフレームの受信（Can_MainFunction_Read → CanIf → PduR → Com → Com_ReceiveSignal）の
 *          コールチェーンのテスト（GoogleTest / CMake native_chain_tests）。
 *
 * \details 本番の FdTestRx（CAN 0x320、64 byte）と同じ形の設定（先頭 byte と末尾 byte に 8 bit のシグナルを
 *          1 つずつ置く）を、テスト専用に定義して使う。CanIf の 64 byte の受信は ComStack_FdFrame_Rx_Test が
 *          検証済みのため、本テストは PduR を経て Com の 64 byte のバッファに入り、先頭と末尾のシグナルを
 *          取り出せることに絞る。
 */
/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Com.h"
#include "PduR.h"
#include "CanIf.h"
#include "CanSM.h"
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

/* 先頭 byte（byte[0]）の 8 bit のシグナル */
const Com_SignalConfigType kHeadSignal = {
    /* SignalId */                0U,
    /* Direction */                COM_SIGNAL_DIRECTION_RX,
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
    /* Direction */                COM_SIGNAL_DIRECTION_RX,
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

const Com_IPduConfigType kFdRxIPdu = {
    /* IPduId */           0U,
    /* DLC */              64U,
    /* PduRId */           0U,
    /* FirstTimeoutMs */   0U,
    /* TimeoutMs */        0U,
    /* IsSignalGroup */    0U,
    /* TxModeMode */       COM_TX_MODE_DIRECT,
    /* TxPeriodMs */       0U,
    /* TxModeModeTrue */   COM_TX_MODE_DIRECT,
    /* TxPeriodMsTrue */   0U,
    /* MinDelayMs */       0U,
    /* UpdateBitPosition */ 0xFFFFU,
    /* IpduGroupId */      COM_IPDU_GROUP_NONE,
    /* RxIndicationCbk */  NULL,
    /* TxTransformCbk */   NULL
};

const Com_ConfigType kComConfig = {
    /* RxIPdus */       &kFdRxIPdu,
    /* RxIPduCount */   1U,
    /* TxIPdus */       NULL,
    /* TxIPduCount */   0U,
    /* Signals */       kSignals,
    /* SignalCount */   2U,
    /* GwMappings */    NULL,
    /* GwMappingCount */ 0U
};

const PduR_RxDestType kPduRRxDest = {
    /* Module */    PDUR_MODULE_COM,
    /* DestPduId */ 0U,
    /* RxIndFct */  Com_RxIndication
};

const PduR_RxRoutingPathType kPduRRxPath = {
    /* SrcPduId */  0U,
    /* Dests */     &kPduRRxDest,
    /* DestCount */ 1U
};

const PduR_PBConfigType kPduRConfig = {
    /* RxPaths */     &kPduRRxPath,
    /* RxPathCount */ 1U,
    /* TxPaths */     NULL,
    /* TxPathCount */ 0U
};

const CanIf_RxPduConfigType kCanIfRxPdu = {
    /* CanId */                0x320U,
    /* Hrh */                  0U,
    /* UpperLayerRxPduId */    0U,
    /* Dlc */                  64U,
    /* RxIndicationFct */      PduR_CanIfRxIndication,
    /* ReadRxPduDataEnabled */ 0U
};

const CanIf_ConfigType kCanIfConfig = {
    /* TxPduConfig */ NULL,
    /* TxPduCount */  0U,
    /* RxPduConfig */ &kCanIfRxPdu,
    /* RxPduCount */  1U
};

/* 64 byte のフレームの受信のコールチェーンのフィクスチャ。
 * SetUp(): 模擬 Can_Hw・wrap をリセットし、Can_Init()（CAN FD 有効）→ CanIf_Init() → コントローラを STARTED
 *          → PduR_Init() → Com_Init() → CanSM_Init(NULL) の順に初期化する（Init 中の DET ログは抑制する）。
 * TearDown(): CanSM・Com・CanIf を未初期化へ戻す。 */
class ComStack_FdFrame_ComRx_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;
        FakeMillis_Reset();
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
        PduR_Init(&kPduRConfig);
        Com_Init(&kComConfig);
        CanSM_Init(NULL);

        FakeDetHw_LogSuppressed = 0U;
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        CanSM_DeInit();
        Com_DeInit();
        CanIf_DeInit();
    }

    Can_ConfigType canConfig;
};

}  // namespace

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(ComStack_FdFrame_ComRx_Test, OK_Step01_64ByteFrameHeadAndTailReachCom)
{
    /* ---- 準備 (Arrange) ---- */
    FakeCanHw_RxPendingCount = 1U;
    FakeCanHw_RxId  = 0x320U;
    FakeCanHw_RxDlc = 64U;
    for (uint8_t b = 0U; b < 64U; b++)
        FakeCanHw_RxData[b] = 0U;
    FakeCanHw_RxData[0]  = 0x11U;
    FakeCanHw_RxData[63] = 0x22U;

    /* ---- 実行 (Act) ---- */
    Can_MainFunction_Read();
    uint8 head = 0xFFU;
    uint8 tail = 0xFFU;
    const uint8 retHead = Com_ReceiveSignal(0U, &head);
    const uint8 retTail = Com_ReceiveSignal(1U, &tail);

    /* ---- 評価 (Assert) ---- */
    EXPECT_EQ(retHead, E_OK);
    EXPECT_EQ(retTail, E_OK);
    EXPECT_EQ(head, 0x11U);
    EXPECT_EQ(tail, 0x22U);
}

TEST_F(ComStack_FdFrame_ComRx_Test, OK_Step02_ZeroValuesAreReceivedAsWell)
{
    /* ---- 準備 (Arrange) ---- */
    FakeCanHw_RxPendingCount = 1U;
    FakeCanHw_RxId  = 0x320U;
    FakeCanHw_RxDlc = 64U;
    for (uint8_t b = 0U; b < 64U; b++)
        FakeCanHw_RxData[b] = 0xEEU;
    FakeCanHw_RxData[0]  = 0x00U;
    FakeCanHw_RxData[63] = 0x00U;
    uint8 head = 0xFFU;
    uint8 tail = 0xFFU;

    /* ---- 実行 (Act) ---- */
    Can_MainFunction_Read();
    (void)Com_ReceiveSignal(0U, &head);
    (void)Com_ReceiveSignal(1U, &tail);

    /* ---- 評価 (Assert) ---- */
    EXPECT_EQ(head, 0x00U);   // 間の byte（0xEE）が混ざらず、位置どおりに取り出せる
    EXPECT_EQ(tail, 0x00U);
}
