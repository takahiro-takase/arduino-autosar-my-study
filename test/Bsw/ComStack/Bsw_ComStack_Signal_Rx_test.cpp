/**
 * \file    Bsw_ComStack_Signal_Rx_test.cpp
 * \brief   README.md「Rx 処理（Can → CanIf → PduR → Com の順）」および
 *          「デッドライン監視（受信タイムアウト）」コールチェーンの単体テスト
 *          （GoogleTest / CMake native_chain_tests）。通常のシグナル
 *          （非 Signal Group）受信シナリオ専用。
 *
 * \details 2026-09、`Bsw_ComStack_Rx_test.cpp`/`Bsw_ComStack_RxTimeout_test.cpp`
 *          のうち非 Signal Group（IsSignalGroup=0）関連分を、Msg内容
 *          （Signal/SignalGroup/E2E/SecOC）× 方向（Tx/Rx）の
 *          `Bsw_ComStack_{MsgType}_{Tx|Rx}_test.cpp` 命名規則へ統合した
 *          （ユーザー指示）。Signal Group 関連分は
 *          `Bsw_ComStack_SignalGroup_Rx_test.cpp` へ移した。
 *
 *          物理チェーン部分（`Bsw_ComStack_Signal_Rx_Test`）:
 *              Can_MainFunction_Read()
 *                → CanIf_RxIndication()         ← CAN ID → PduId へ変換
 *                  → PduR_CanIfRxIndication() (= PduR_ComRxIndication())
 *                    → Com_RxIndication()
 *          SignalId=0 (RX, 16bit BigEndian) 1本だけを持つ IPduId=0 の RX I-PDU
 *          （CanIf の RxPduId=0、CAN ID=0x100、Hrh=0）と、部分受信ゲーティング
 *          検証用の IPduId=2（byte0/byte1 個別シグナル、CanIf 側ルーティング
 *          なし、`Com_RxIndication()` を直接呼ぶ）のみを使う。
 *
 *          デッドライン監視部分（`rx_timeout::Bsw_ComStack_Signal_Rx_Timeout_Test`）:
 *              [100ms 周期タスク] Com_MainFunctionRx() → Com_SigTimedOut[] → Com_ReceiveSignal()
 *          Com.c 内で完結し PduR/CanIf/Can/CanSM は経由しないため、フェイクは
 *          `millis()`（`stub/Hal/Fake_Millis.c`）のみで足りる。IPduId=0
 *          （RxDataTimeoutAction=SUBSTITUTE）と、[SWS_Com_00872] 段階順の
 *          回帰用の IPduId=2（RxIpduCalloutCbk が必ず拒否）のみを使う。
 *          `CanIf_RxIndication()` は無条件に `CanSM_RxIndication()` を呼ぶため、
 *          物理チェーン部分では `[env:native_chain]` が実体リンクする CanSM の
 *          `CanSM_Init()`/`CanSM_DeInit()` を呼ぶ（呼ばないと DET_E_UNINIT が
 *          報告される）。
 */
#include <gtest/gtest.h>

extern "C" {
#include "Com.h"
#include "PduR.h"
#include "CanIf.h"
#include "CanSM.h"
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

// -----------------------------------------------------------------------
// SWS_Com_00700/00816 (Com_RxIpduCallout) 検証用。s_calloutAccept で
// 各テストから戻り値を制御し、s_calloutInvokeCount で呼び出し回数・
// s_calloutLastByte1 で受け取った生バイト列を確認する。
// -----------------------------------------------------------------------
static uint8_t s_calloutAccept = 1U;
static uint8_t s_calloutInvokeCount = 0U;
static uint8_t s_calloutLastByte1 = 0U;
static boolean TestRxIpduCallout(const uint8* SduDataPtr, uint8 SduLength)
{
    s_calloutInvokeCount++;
    s_calloutLastByte1 = (SduLength > 1U) ? SduDataPtr[1] : 0xFFU;
    return s_calloutAccept != 0U;
}

// SWS_Com_00555（Com_CbkRxAck）検証用カウンタ・コールバック。
static uint8_t s_rxAckCount = 0U;
static void TestRxAckCbk(void) { s_rxAckCount++; }

const Com_SignalConfigType kTestRxSignal = {
    /* SignalId */                0U,
    /* Direction */                COM_SIGNAL_DIRECTION_RX,
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
    /* InvalidValue */             0U,
    /* InvalidNotificationCbk */   NULL,
    /* FirstTimeoutMs */           0U,
    /* TimeoutMs */                0U,
    /* RxTOutCbk */                NULL,
    /* TxAckCbk */                 NULL,
    /* TxErrCbk */                 NULL,
    /* RxAckCbk */                 TestRxAckCbk
};

const Com_IPduConfigType kTestRxIPdu = {
    /* IPduId */           0U,
    /* DLC */              2U,
    /* PduRId */           0U,
    /* FirstTimeoutMs */   0U,
    /* TimeoutMs */        0U,
    /* IsSignalGroup */    0U,
    /* TxModeMode */       COM_TX_MODE_DIRECT,  /* RX I-PDU では未使用 */
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
    /* NumberOfRepetitions */ 0U,
    /* RepetitionPeriodMs */  0U,
    /* TxFirstTimeoutMs */    0U,
    /* TxTimeoutMs */         0U,
    /* TxTOutCbk */           NULL,
    /* RxTOutCbk */           NULL,
    /* RxIpduCalloutCbk */    TestRxIpduCallout
};

// -----------------------------------------------------------------------
// SWS_Com_00574 の部分受信ゲーティング（lastByte <= recvLen）検証用。
// 非 Signal Group の I-PDU（DLC=2）に byte0/byte1 それぞれ専用のシグナルを
// 置き、SduLength=1（byte0 のみ受信）で呼んだときに byte0 側のみ RxAckCbk
// が発火することを確認する。部分受信自体は実機では到達しない経路だが
// （CanIf の DLC が常に Com の DLC と同値のため）、Com_RxIndication() を
// 直接呼ぶユニットテストなら CanIf の制約を経由せず検証できる。
// -----------------------------------------------------------------------
static uint8_t s_partialAckCount0 = 0U;
static void TestPartialAckCbk0(void) { s_partialAckCount0++; }

static uint8_t s_partialAckCount1 = 0U;
static void TestPartialAckCbk1(void) { s_partialAckCount1++; }

const Com_SignalConfigType kTestPartialSignalByte0 = {
    /* SignalId */                3U,
    /* Direction */                COM_SIGNAL_DIRECTION_RX,
    /* IPduId */                   2U,
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
    /* TxErrCbk */                 NULL,
    /* RxAckCbk */                 TestPartialAckCbk0
};

const Com_SignalConfigType kTestPartialSignalByte1 = {
    /* SignalId */                4U,
    /* Direction */                COM_SIGNAL_DIRECTION_RX,
    /* IPduId */                   2U,
    /* BitPosition */              8U,
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
    /* TxErrCbk */                 NULL,
    /* RxAckCbk */                 TestPartialAckCbk1
};

const Com_IPduConfigType kTestPartialIPdu = {
    /* IPduId */           2U,
    /* DLC */              2U,
    /* PduRId */           2U,  // 本テストは Com_MainFunctionRx()/Tx()/CanIf/PduR まで進めないため未使用
    /* FirstTimeoutMs */   0U,
    /* TimeoutMs */        0U,
    /* IsSignalGroup */    0U,
    /* TxModeMode */       COM_TX_MODE_DIRECT,  /* RX I-PDU では未使用 */
    /* TxPeriodMs */       0U,
    /* TxModeModeTrue */   COM_TX_MODE_DIRECT,
    /* TxPeriodMsTrue */   0U,
    /* MinDelayMs */       0U,
    /* UpdateBitPosition */ 0xFFU,
    /* IpduGroupId */      COM_IPDU_GROUP_NONE,
    /* RxIndicationCbk */  NULL,
    /* TxTransformCbk */   NULL
};

const Com_SignalConfigType kTestRxSignals[] = {
    kTestRxSignal, kTestPartialSignalByte0, kTestPartialSignalByte1
};
const Com_IPduConfigType kTestRxIPdus[] = { kTestRxIPdu, kTestPartialIPdu };

const Com_ConfigType kTestComRxConfig = {
    /* RxIPdus */       kTestRxIPdus,
    /* RxIPduCount */   2U,
    /* TxIPdus */       NULL,
    /* TxIPduCount */   0U,
    /* Signals */       kTestRxSignals,
    /* SignalCount */   3U,
    /* GwMappings */    NULL,
    /* GwMappingCount */ 0U
};

const PduR_RxDestType kTestPduRRxDest = {
    /* Module */    PDUR_MODULE_COM,
    /* DestPduId */ 0U,
    /* RxIndFct */  Com_RxIndication
};

const PduR_RxRoutingPathType kTestPduRRxPath = {
    /* SrcPduId */  0U,
    /* Dests */     &kTestPduRRxDest,
    /* DestCount */ 1U
};

const PduR_PBConfigType kTestPduRRxConfig = {
    /* RxPaths */     &kTestPduRRxPath,
    /* RxPathCount */ 1U,
    /* TxPaths */     NULL,
    /* TxPathCount */ 0U
};

const CanIf_RxPduConfigType kTestCanIfRxPdu = {
    /* CanId */                0x100U,
    /* Hrh */                  0U,  /* Can_MainFunction_Read() が構築する Mailbox は常に Hoh=0 */
    /* UpperLayerRxPduId */    0U,
    /* Dlc */                  2U,
    /* RxIndicationFct */      PduR_ComRxIndication,  /* = PduR_CanIfRxIndication（#define エイリアス） */
    /* ReadRxPduDataEnabled */ 1U  // CanIf_ReadRxPduData（SWS_CANIF_00194）検証用
};

// CanIf_ReadRxPduData()（SWS_CANIF_00194、2026-08 追加）の opt-in ゲート
// （ReadRxPduDataEnabled=0）検証用。実際に受信させても、この PDU 自体は
// バッファリング対象外のままであることを確認する。上位層ルーティングは
// 不要（RxIndicationFct=NULL）なため Com/PduR 側の設定は増やさない。
const CanIf_RxPduConfigType kTestCanIfRxPduNoBuffer = {
    /* CanId */                0x101U,
    /* Hrh */                  0U,
    /* UpperLayerRxPduId */    1U,
    /* Dlc */                  2U,
    /* RxIndicationFct */      NULL,
    /* ReadRxPduDataEnabled */ 0U
};

// CanIf_ReadRxPduData() のバッファ長クランプ検証用（/code-review 指摘の
// 是正確認）。CanIf_RxIndication() の既存の長さチェックは SduLength <
// Dlc（不足）のみを棄却し、超過は素通りするため、Dlc(2) より長い
// フレーム（8byte）を受けたときにバッファ長がこの PDU 自身の Dlc(2) で
// クランプされ、モジュール共通の CANIF_MAX_DLC(8) まで届かないことを
// 確認する。上位層ルーティングは不要。
const CanIf_RxPduConfigType kTestCanIfRxPduSmallDlc = {
    /* CanId */                0x102U,
    /* Hrh */                  0U,
    /* UpperLayerRxPduId */    2U,
    /* Dlc */                  2U,
    /* RxIndicationFct */      NULL,
    /* ReadRxPduDataEnabled */ 1U
};

const CanIf_RxPduConfigType kTestCanIfRxPdus[] = {
    kTestCanIfRxPdu, kTestCanIfRxPduNoBuffer, kTestCanIfRxPduSmallDlc
};

const CanIf_ConfigType kTestCanIfRxConfig = {
    /* TxPduConfig */ NULL,
    /* TxPduCount */  0U,
    /* RxPduConfig */ kTestCanIfRxPdus,
    /* RxPduCount */  3U
};

class Bsw_ComStack_Signal_Rx_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
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
        CanIf_Init(&kTestCanIfRxConfig);
        CanIf_SetControllerMode(0U, CAN_CS_STARTED);
        PduR_Init(&kTestPduRRxConfig);
        Com_Init(&kTestComRxConfig);
        s_rxAckCount = 0U;
        s_partialAckCount0 = 0U;
        s_partialAckCount1 = 0U;
        s_calloutAccept = 1U;
        s_calloutInvokeCount = 0U;
        s_calloutLastByte1 = 0U;
        // CanIf_RxIndication() は無条件に CanSM_RxIndication() を呼ぶため
        // （CanIf.c 参照）、CanSM 未初期化のままだと毎回 DET_E_UNINIT が
        // 報告されてしまう。本テストは CanSM_State を FULL_COM/NO_COM のまま
        // （WAKEUP_VALIDATING にしない）保つため、CanSM 自体は何もしない
        // no-op として通過するだけになる。
        CanSM_Init(NULL);

        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;  // DeInit() のログを抑制
        CanSM_DeInit();
        Com_DeInit();
        CanIf_DeInit();
    }

    Can_ConfigType canConfig;
};

// ------------------------------------------------------------
// Can_MainFunction_Read() ─ Can_Hw から Com_ReceiveSignal() まで
// ------------------------------------------------------------
TEST_F(Bsw_ComStack_Signal_Rx_Test, CanMainFunctionRead_OK_DrivesToComReceiveSignal)
{
    /* 準備 (Arrange): フェイク Can_Hw に受信フレーム1件を積む */
    FakeCanHw_RxPendingCount = 1U;
    FakeCanHw_RxId  = 0x100U;
    FakeCanHw_RxDlc = 2U;
    FakeCanHw_RxData[0] = 0x56U;
    FakeCanHw_RxData[1] = 0x78U;

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert) */
    EXPECT_EQ(FakeCanHw_RxPendingCount, 0U);  // ドレインし尽くしたこと
    uint16_t value = 0U;
    uint8 ret = Com_ReceiveSignal(0U, &value);
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(value, 0x5678U);  // BigEndian: byte[0]=MSB
}


TEST_F(Bsw_ComStack_Signal_Rx_Test, CanMainFunctionRead_NG_NothingReceived_LeavesInitValue)
{
    /* 準備 (Arrange): 受信フレームなし */
    FakeCanHw_RxPendingCount = 0U;

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert): Com_SignalConfigType.InitValue（既定 0）のまま */
    uint16_t value = 0xFFFFU;  // 上書きされていないことが分かるよう非0で初期化
    uint8 ret = Com_ReceiveSignal(0U, &value);
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(value, 0U);
}


TEST_F(Bsw_ComStack_Signal_Rx_Test, CanMainFunctionRead_NG_InsufficientDlcIsDiscardedAndReportsRuntimeError)
{
    /* 準備 (Arrange): 設定 Dlc(2) に満たない 1byte フレームを積む */
    FakeCanHw_RxPendingCount = 1U;
    FakeCanHw_RxId  = 0x100U;
    FakeCanHw_RxDlc = 1U;
    FakeCanHw_RxData[0] = 0x56U;
    FakeDetHw_Reset();

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert): 上位層(Com)へは渡らず InitValue のまま、かつ
     * CANIF_E_INVALID_DATA_LENGTH がランタイムエラーとして報告される */
    uint16_t value = 0xFFFFU;
    uint8 ret = Com_ReceiveSignal(0U, &value);
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(value, 0U);

    EXPECT_EQ(FakeDetHw_LastModuleId, static_cast<uint16>(CANIF_MODULE_ID));
    EXPECT_EQ(FakeDetHw_LastApiId, static_cast<uint8>(CANIF_API_ID_RX_INDICATION));
    EXPECT_EQ(FakeDetHw_LastErrorId, static_cast<uint8>(CANIF_E_INVALID_DATA_LENGTH));
}


// ------------------------------------------------------------
// CanIf_ReadRxPduData（SWS_CANIF_00194、2026-08 追加）
// ------------------------------------------------------------
TEST_F(Bsw_ComStack_Signal_Rx_Test, CanIfReadRxPduData_OK_ReturnsBufferedDataAfterReceive)
{
    /* 準備 (Arrange): CanMainFunctionRead_OK_DrivesToComReceiveSignal と
     * 同じ手順で実際に1フレーム受信させる（CanIf_RxIndication() の内部で
     * バッファへ複製される）。 */
    FakeCanHw_RxPendingCount = 1U;
    FakeCanHw_RxId  = 0x100U;
    FakeCanHw_RxDlc = 2U;
    FakeCanHw_RxData[0] = 0x56U;
    FakeCanHw_RxData[1] = 0x78U;
    Can_MainFunction_Read();

    /* 実行 (Act) */
    uint8 buf[CANIF_MAX_DLC] = {0U};
    PduInfoType info = { buf, 0U };
    Std_ReturnType ret = CanIf_ReadRxPduData(0U, &info);

    /* 評価 (Assert) */
    EXPECT_EQ(ret, E_OK);
    ASSERT_EQ(info.SduLength, 2U);
    EXPECT_EQ(buf[0], 0x56U);
    EXPECT_EQ(buf[1], 0x78U);
}


TEST_F(Bsw_ComStack_Signal_Rx_Test, CanIfReadRxPduData_NG_ReturnsErrorBeforeAnyReceive)
{
    /* 準備 (Arrange): 一切受信させない */

    /* 実行 (Act) + 評価 (Assert): spec 原文 "No valid data has been received" */
    uint8 buf[CANIF_MAX_DLC] = {0U};
    PduInfoType info = { buf, 0U };
    EXPECT_EQ(CanIf_ReadRxPduData(0U, &info), E_NOT_OK);
}


TEST_F(Bsw_ComStack_Signal_Rx_Test, CanIfReadRxPduData_NG_NotOptedInReturnsErrorEvenAfterReceive)
{
    /* 準備 (Arrange): kTestCanIfRxPduNoBuffer（CanIfRxSduId=1、CAN 0x101、
     * ReadRxPduDataEnabled=0）を実際に受信させる。 */
    FakeCanHw_RxPendingCount = 1U;
    FakeCanHw_RxId  = 0x101U;
    FakeCanHw_RxDlc = 2U;
    FakeCanHw_RxData[0] = 0xAAU;
    FakeCanHw_RxData[1] = 0xBBU;
    Can_MainFunction_Read();

    /* 実行 (Act) + 評価 (Assert): [SWS_CANIF_00325] opt-in されていない
     * PDU への要求は E_NOT_OK（受信済みかどうかによらない）。 */
    uint8 buf[CANIF_MAX_DLC] = {0U};
    PduInfoType info = { buf, 0U };
    EXPECT_EQ(CanIf_ReadRxPduData(1U, &info), E_NOT_OK);
}


TEST_F(Bsw_ComStack_Signal_Rx_Test, CanIfReadRxPduData_NG_UnknownPduIdReturnsError)
{
    /* 実行 (Act) + 評価 (Assert) */
    uint8 buf[CANIF_MAX_DLC] = {0U};
    PduInfoType info = { buf, 0U };
    EXPECT_EQ(CanIf_ReadRxPduData(99U, &info), E_NOT_OK);
}


TEST_F(Bsw_ComStack_Signal_Rx_Test, CanIfReadRxPduData_NG_NullPointerReturnsError)
{
    /* 実行 (Act) + 評価 (Assert) */
    EXPECT_EQ(CanIf_ReadRxPduData(0U, NULL), E_NOT_OK);
}


TEST_F(Bsw_ComStack_Signal_Rx_Test, CanIfReadRxPduData_OK_ClampsBufferedLengthToPduDlcNotModuleMax)
{
    /* 準備 (Arrange): kTestCanIfRxPduSmallDlc（CanIfRxSduId=2、Dlc=2）に対し、
     * 設定 Dlc(2) より長い 8byte フレームを受信させる。既存の長さチェックは
     * 不足のみ棄却するため、この受信自体は素通りする。 */
    FakeCanHw_RxPendingCount = 1U;
    FakeCanHw_RxId  = 0x102U;
    FakeCanHw_RxDlc = 8U;
    for (uint8_t b = 0U; b < 8U; b++)
        FakeCanHw_RxData[b] = (uint8_t)(0xC0U + b);
    Can_MainFunction_Read();

    /* 実行 (Act) */
    uint8 buf[CANIF_MAX_DLC] = {0U};
    PduInfoType info = { buf, 0U };
    Std_ReturnType ret = CanIf_ReadRxPduData(2U, &info);

    /* 評価 (Assert): /code-review 指摘の是正確認。バッファ長はこの PDU
     * 自身の設定 Dlc(2) でクランプされ、モジュール共通の CANIF_MAX_DLC(8)
     * までは届かない——CanIfRxSduId=2 の呼び出し元が Dlc(2) 分だけ確保した
     * バッファでも安全であることの検証。 */
    EXPECT_EQ(ret, E_OK);
    ASSERT_EQ(info.SduLength, 2U);
    EXPECT_EQ(buf[0], 0xC0U);
    EXPECT_EQ(buf[1], 0xC1U);
}


TEST_F(Bsw_ComStack_Signal_Rx_Test, CanIfInit_NG_RejectsConfigWithRxPduCountAboveMaxWithoutActivating)
{
    /* 準備 (Arrange): CanIf_RxPduDataBuffer[]/Length[]/Valid[] は
     * CANIF_RX_PDU_MAX（native_chain バイナリ全体で共有される固定サイズ）
     * でしか確保されていない（/code-review・/simplify 指摘）。それを超える
     * RxPduCount を渡した場合に初期化自体が拒否されることを確認する。
     * RxPduConfig 自体は CanIf_Init() 内で走査されないため NULL のままでよい
     * （範囲チェックのみで早期 return するため、その後の配列アクセスは
     * 一切発生しない）。まず SetUp() が設定した有効な状態を DeInit() で
     * クリアしておく。 */
    CanIf_DeInit();
    const CanIf_ConfigType kOversizedConfig = {
        /* TxPduConfig */ NULL,
        /* TxPduCount */  0U,
        /* RxPduConfig */ NULL,
        /* RxPduCount */  (uint8_t)(CANIF_RX_PDU_MAX + 1U)
    };

    /* 実行 (Act) */
    CanIf_Init(&kOversizedConfig);

    /* 評価 (Assert): 拒否されて未初期化のままのため、他の API は
     * CanIf_ConfigPtr==NULL の早期 return 経路（DET 報告なし）を通り、
     * E_NOT_OK を返す。 */
    uint8 buf[CANIF_MAX_DLC] = {0U};
    PduInfoType info = { buf, 0U };
    EXPECT_EQ(CanIf_ReadRxPduData(0U, &info), E_NOT_OK);

    /* 後始末 (Cleanup): TearDown() が CanIf_DeInit() を呼ぶだけなので、
     * kTestCanIfRxConfig で再度有効化しておく必要はない
     * （DeInit は未初期化状態への遷移で、既に未初期化のため冪等）。 */
}


// ------------------------------------------------------------
// SWS_Com_00700/00816（Com_RxIpduCallout）
// ------------------------------------------------------------
TEST_F(Bsw_ComStack_Signal_Rx_Test, ComRxIndication_OK_AcceptedByCalloutProcessesNormally)
{
    /* 準備 (Arrange): コールバックは受理（既定の s_calloutAccept=1U） */
    uint8 buf[2] = { 0x12U, 0x34U };
    PduInfoType pduInfo = { buf, 2U };

    /* 実行 (Act) */
    Com_RxIndication(0U, &pduInfo);

    /* 評価 (Assert): コールアウトは1回、生バイト列そのまま呼ばれ、
     * 通常どおりバッファへ格納され RxAckCbk も発火する */
    EXPECT_EQ(s_calloutInvokeCount, 1U);
    EXPECT_EQ(s_calloutLastByte1, 0x34U);
    EXPECT_EQ(s_rxAckCount, 1U);
    uint16_t value = 0U;
    EXPECT_EQ(Com_ReceiveSignal(0U, &value), E_OK);
    EXPECT_EQ(value, 0x1234U);
}


TEST_F(Bsw_ComStack_Signal_Rx_Test, ComRxIndication_NG_RejectedByCalloutDiscardsFrameEntirely)
{
    /* 準備 (Arrange): コールバックが拒否する設定にする */
    s_calloutAccept = 0U;
    uint8 buf[2] = { 0x12U, 0x34U };
    PduInfoType pduInfo = { buf, 2U };

    /* 実行 (Act) */
    Com_RxIndication(0U, &pduInfo);

    /* 評価 (Assert): [SWS_Com_00700] "false: I-PDU will not be processed any
     * further" のとおり、バッファは更新されず（InitValue のまま）、
     * RxAckCbk（バッファ格納後の通知）も発火しない */
    EXPECT_EQ(s_calloutInvokeCount, 1U);
    EXPECT_EQ(s_rxAckCount, 0U);
    uint16_t value = 0xFFFFU;
    EXPECT_EQ(Com_ReceiveSignal(0U, &value), E_OK);
    EXPECT_EQ(value, 0U);  // InitValue のまま（部分受信ではなく完全な不採用）
}


// ------------------------------------------------------------
// SWS_Com_00555（Com_CbkRxAck、非グループ側）/ SWS_Com_00574（部分受信）
// ------------------------------------------------------------
TEST_F(Bsw_ComStack_Signal_Rx_Test, ComRxIndication_OK_NonGroupAck_FiresOnFullReception)
{
    /* 準備 (Arrange): IPduId=0 を DLC 分フルで受信 */
    uint8 buf[2] = { 0x12U, 0x34U };
    PduInfoType pduInfo = { buf, 2U };

    /* 実行 (Act) */
    Com_RxIndication(0U, &pduInfo);

    /* 評価 (Assert) */
    EXPECT_EQ(s_rxAckCount, 1U);
}


TEST_F(Bsw_ComStack_Signal_Rx_Test, ComRxIndication_OK_NonGroupAck_FiresOncePerFrame)
{
    /* 準備 (Arrange) */
    uint8 buf[2] = { 0x12U, 0x34U };
    PduInfoType pduInfo = { buf, 2U };

    /* 実行 (Act): 2フレーム受信 */
    Com_RxIndication(0U, &pduInfo);
    Com_RxIndication(0U, &pduInfo);

    /* 評価 (Assert): フレーム数と同じ回数だけ呼ばれる */
    EXPECT_EQ(s_rxAckCount, 2U);
}


TEST_F(Bsw_ComStack_Signal_Rx_Test, ComRxIndication_OK_PartialReception_OnlyAcksSignalsWithinRecvLen)
{
    /* 準備 (Arrange): IPduId=2（DLC=2、byte0/byte1 それぞれ専用シグナル）を
     * byte0 のみ（SduLength=1）で受信する部分受信シナリオ */
    uint8 buf[1] = { 0xABU };
    PduInfoType pduInfo = { buf, 1U };

    /* 実行 (Act) */
    Com_RxIndication(2U, &pduInfo);

    /* 評価 (Assert): recvLen(1) 以内に収まる byte0 側のみ RxAckCbk が発火し、
     * 範囲外の byte1 側は発火しない（[SWS_Com_00574]）。 */
    EXPECT_EQ(s_partialAckCount0, 1U);
    EXPECT_EQ(s_partialAckCount1, 0U);
}



// -----------------------------------------------------------------------
// デッドライン監視（受信タイムアウト）専用の独立したフィクスチャ。
// Com.c 内で完結し PduR/CanIf/Can/CanSM は経由しないため、フェイクは
// millis()（Fake_Millis.c）のみで足りる。上の物理チェーン用configとは
// 別の最小 Com_ConfigType を使う。
// -----------------------------------------------------------------------
namespace rx_timeout
{

// SWS_Com_00536/00556（Com_CbkRxTOut）検証用カウンタ・コールバック。
static uint8_t s_sigRxTOutCount = 0U;
static void TestSigRxTOutCbk(void) { s_sigRxTOutCount++; }

const Com_SignalConfigType kTestRxTimeoutSignal = {
    /* SignalId */                0U,
    /* Direction */                COM_SIGNAL_DIRECTION_RX,
    /* IPduId */                   0U,
    /* BitPosition */              0U,
    /* BitSize */                  16U,
    /* Endian */                   COM_BIG_ENDIAN,
    /* InitValue */                0xAAAAU,
    /* FilterAlgorithm */          COM_FILTER_ALWAYS,
    /* Mask */                     0U,
    /* FilterX */                  0U,
    /* FilterMin */                0U,
    /* FilterMax */                0U,
    /* FilterRejectCbk */          NULL,
    /* TmsContributor */           0U,
    /* UpdateBitContributor */     0U,
    /* TransferProperty */         COM_TRANSFER_PROPERTY_PENDING,
    /* RxDataTimeoutAction */      COM_RX_TIMEOUT_ACTION_SUBSTITUTE,
    /* TimeoutSubstitutionValue */ 0xFFFFU,
    /* DataInvalidAction */        COM_DATA_INVALID_ACTION_NONE,
    /* InvalidValue */             0U,
    /* InvalidNotificationCbk */   NULL,
    /* FirstTimeoutMs */           500U,
    /* TimeoutMs */                500U,
    /* RxTOutCbk */                TestSigRxTOutCbk,
    /* TxAckCbk */                 NULL,
    /* TxErrCbk */                 NULL
};

const Com_IPduConfigType kTestRxTimeoutIPdu = {
    /* IPduId */           0U,
    /* DLC */              2U,
    /* PduRId */           0U,
    /* FirstTimeoutMs */   0U,  /* I-PDU 単位の監視は無効化（本テストの対象外） */
    /* TimeoutMs */        0U,
    /* IsSignalGroup */    0U,
    /* TxModeMode */       COM_TX_MODE_DIRECT,  /* RX I-PDU では未使用 */
    /* TxPeriodMs */       0U,
    /* TxModeModeTrue */   COM_TX_MODE_DIRECT,
    /* TxPeriodMsTrue */   0U,
    /* MinDelayMs */       0U,
    /* UpdateBitPosition */ 0xFFU,
    /* IpduGroupId */      COM_IPDU_GROUP_NONE,
    /* RxIndicationCbk */  NULL,
    /* TxTransformCbk */   NULL
};

// IPduId=1: [SWS_Com_00872] 段階順の回帰テスト用。RxIpduCalloutCbk が
// 必ず拒否する（＝バッファ・通知はいずれも動かない）設定でも、デッドライン
// 監視タイマ（段階1）だけは reject の前に既にリセットされていることを
// 検証する。
static uint8_t s_rejectCalloutInvokeCount = 0U;
static boolean TestAlwaysRejectCallout(const uint8* SduDataPtr, uint8 SduLength)
{
    (void)SduDataPtr;
    (void)SduLength;
    s_rejectCalloutInvokeCount++;
    return FALSE;
}

const Com_IPduConfigType kTestRxTimeoutRejectedIPdu = {
    /* IPduId */           2U,
    /* DLC */              1U,
    /* PduRId */           2U,
    /* FirstTimeoutMs */   1000U,  /* TimeoutMs と意図的に異なる値にして、
                                    * reject されても First→steady 状態
                                    * 遷移が起きることを検証できるようにする。 */
    /* TimeoutMs */        500U,
    /* IsSignalGroup */    0U,
    /* TxModeMode */       COM_TX_MODE_DIRECT,  /* RX I-PDU では未使用 */
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
    /* NumberOfRepetitions */ 0U,
    /* RepetitionPeriodMs */  0U,
    /* TxFirstTimeoutMs */    0U,
    /* TxTimeoutMs */         0U,
    /* TxTOutCbk */           NULL,
    /* RxTOutCbk */           NULL,
    /* RxIpduCalloutCbk */    TestAlwaysRejectCallout
};

const Com_SignalConfigType kTestRxTimeoutSignals[] = { kTestRxTimeoutSignal };
const Com_IPduConfigType kTestRxTimeoutIPdus[] = {
    kTestRxTimeoutIPdu, kTestRxTimeoutRejectedIPdu
};

const Com_ConfigType kTestComRxTimeoutConfig = {
    /* RxIPdus */       kTestRxTimeoutIPdus,
    /* RxIPduCount */   2U,
    /* TxIPdus */       NULL,
    /* TxIPduCount */   0U,
    /* Signals */       kTestRxTimeoutSignals,
    /* SignalCount */   1U,
    /* GwMappings */    NULL,
    /* GwMappingCount */ 0U
};

class Bsw_ComStack_Signal_Rx_Timeout_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制

        Com_Init(&kTestComRxTimeoutConfig);
        s_sigRxTOutCount = 0U;
        s_rejectCalloutInvokeCount = 0U;

        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;  // DeInit() のログを抑制
        Com_DeInit();
    }

    /** 「受信していたが、その後途絶えた」状況を作る共通ヘルパー。 */
    void ReceiveOnce(uint16_t value)
    {
        uint8 data[2] = { (uint8)(value >> 8), (uint8)(value & 0xFFU) };
        PduInfoType pdu = { data, 2U };
        Com_RxIndication(0U, &pdu);
    }

    /** ReceiveOnce() の RxIpduCalloutCbk が必ず拒否する I-PDU（IPduId=2）版。 */
    void ReceiveOnceRejected(void)
    {
        uint8 data[1] = { 0x00U };
        PduInfoType pdu = { data, 1U };
        Com_RxIndication(2U, &pdu);
    }
};

// ------------------------------------------------------------
// セグメント①: Com_MainFunctionRx() ─ Com_SigTimedOut というフラグで切れるまで
// ------------------------------------------------------------
TEST_F(Bsw_ComStack_Signal_Rx_Timeout_Test, ComMainFunction_OK_DetectsTimeoutAfterThresholdElapsed)
{
    /* 準備 (Arrange): 一度受信させてから、しきい値(500ms)を超えて時間を進める */
    ReceiveOnce(0x1234U);
    FakeMillis_Value = 600UL;

    /* 実行 (Act) */
    Com_MainFunctionRx();

    /* 評価 (Assert): フラグに加えて SWS_Com_00536/00556 (Com_CbkRxTOut) の
     * シグナル単位コールバックも新規検出の瞬間に1回だけ呼ばれる */
    EXPECT_EQ(Com_Test_GetSigTimedOut(0U), 1U);
    EXPECT_EQ(s_sigRxTOutCount, 1U);
}


TEST_F(Bsw_ComStack_Signal_Rx_Timeout_Test, ComMainFunction_NG_BeforeThreshold_LeavesSigTimedOutClear)
{
    /* 準備 (Arrange): 一度受信させるが、しきい値(500ms)未満しか時間を進めない */
    ReceiveOnce(0x1234U);
    FakeMillis_Value = 400UL;

    /* 実行 (Act) */
    Com_MainFunctionRx();

    /* 評価 (Assert): まだ検知しない。コールバックも呼ばれない */
    EXPECT_EQ(Com_Test_GetSigTimedOut(0U), 0U);
    EXPECT_EQ(s_sigRxTOutCount, 0U);
}


TEST_F(Bsw_ComStack_Signal_Rx_Timeout_Test, ComMainFunction_OK_SigRxTOutCbkFiresOnlyOnceAcrossRepeatedCalls)
{
    /* 準備 (Arrange): しきい値超過を検出させたあと、時間をさらに進めて
     * Com_MainFunctionRx() を再度呼ぶ（エッジトリガのため2回目は発火しない） */
    ReceiveOnce(0x1234U);
    FakeMillis_Value = 600UL;
    Com_MainFunctionRx();
    ASSERT_EQ(s_sigRxTOutCount, 1U);

    /* 実行 (Act) */
    FakeMillis_Value = 700UL;
    Com_MainFunctionRx();

    /* 評価 (Assert): 新規検出時のみ発火するため回数は増えない */
    EXPECT_EQ(s_sigRxTOutCount, 1U);
}


// ------------------------------------------------------------
// [SWS_Com_00872] 段階順の回帰テスト
// ------------------------------------------------------------
TEST_F(Bsw_ComStack_Signal_Rx_Timeout_Test, ComMainFunction_NG_RejectedFrameStillResetsDeadlineTimer)
{
    /* 準備 (Arrange): t=300ms でフレーム到着→callout に拒否される。
     * もしタイマがリセットされていなければ、Com_Init() 時点(t=0)を
     * 起点に t=600ms で 500ms しきい値を超えてタイムアウトしてしまう。
     * タイマが正しくリセットされていれば、拒否された t=300ms を起点に
     * t=600ms 時点ではまだ 300ms しか経過しておらず、タイムアウトしない。 */
    FakeMillis_Value = 300UL;
    ReceiveOnceRejected();
    ASSERT_EQ(s_rejectCalloutInvokeCount, 1U);  // 拒否経路を通ったことの裏付け

    /* 実行 (Act) */
    FakeMillis_Value = 600UL;
    Com_MainFunctionRx();

    /* 評価 (Assert): 実 AUTOSAR ならタイムアウトしない状況
     * （バスは正常、ペイロードが拒否されただけ）で、実際にタイムアウト
     * しないことを確認する。 */
    EXPECT_EQ(Com_IsRxTimedOut(2U), 0U);
}


TEST_F(Bsw_ComStack_Signal_Rx_Timeout_Test, ComMainFunction_OK_RejectedFrameStillTransitionsToSteadyTimeout)
{
    /* 準備 (Arrange): IPduId=2 の初回受信（t=300ms）が callout に拒否される。
     * [SWS_Com_00715]（Com_RxIndication 呼び出し自体でタイマ再始動）と
     * [SWS_Com_00738]（シグナル値を考慮しない）により、拒否されても
     * First→steady の状態遷移自体は起きるはず。しきい値を FirstTimeoutMs
     * (1000ms) ではなく steady の TimeoutMs (500ms) に切り替えさせて
     * 検証する。 */
    FakeMillis_Value = 300UL;
    ReceiveOnceRejected();
    ASSERT_EQ(s_rejectCalloutInvokeCount, 1U);

    /* t=750ms（拒否からの経過 450ms）: steady(500ms) 未満のためまだ
     * タイムアウトしない */
    FakeMillis_Value = 750UL;
    Com_MainFunctionRx();
    ASSERT_EQ(Com_IsRxTimedOut(2U), 0U);

    /* 実行 (Act): t=850ms（拒否からの経過 550ms） */
    FakeMillis_Value = 850UL;
    Com_MainFunctionRx();

    /* 評価 (Assert): steady の 500ms は超えているためタイムアウトする。
     * もし拒否によって First(1000ms) のまま据え置かれていたら、
     * 550ms < 1000ms でタイムアウトしないはずなので、この違いで
     * 状態遷移の有無を判別できる。 */
    EXPECT_EQ(Com_IsRxTimedOut(2U), 1U);
}


// ------------------------------------------------------------
// セグメント②: Com_ReceiveSignal() ─ フラグの続きから RxDataTimeoutAction 適用まで
// ------------------------------------------------------------
TEST_F(Bsw_ComStack_Signal_Rx_Timeout_Test, ComReceiveSignal_OK_SubstitutesValueAfterTimeout)
{
    /* 準備 (Arrange): セグメント①の終端状態（Com_SigTimedOut が立った状態）を用意する */
    ReceiveOnce(0x1234U);
    FakeMillis_Value = 600UL;
    Com_MainFunctionRx();
    ASSERT_EQ(Com_Test_GetSigTimedOut(0U), 1U);

    /* 実行 (Act) */
    uint16_t value = 0U;
    uint8 ret = Com_ReceiveSignal(0U, &value);

    /* 評価 (Assert): 実受信値(0x1234)ではなく TimeoutSubstitutionValue が返る */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(value, 0xFFFFU);
}


TEST_F(Bsw_ComStack_Signal_Rx_Timeout_Test, ComReceiveSignal_NG_BeforeTimeout_ReturnsLastReceivedValue)
{
    /* 準備 (Arrange): 受信直後、まだタイムアウトしきい値に達していない */
    ReceiveOnce(0x1234U);
    FakeMillis_Value = 400UL;
    Com_MainFunctionRx();
    ASSERT_EQ(Com_Test_GetSigTimedOut(0U), 0U);

    /* 実行 (Act) */
    uint16_t value = 0U;
    uint8 ret = Com_ReceiveSignal(0U, &value);

    /* 評価 (Assert): 実受信値がそのまま返る（置換されない） */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(value, 0x1234U);
}



}  // namespace rx_timeout

}  // namespace
