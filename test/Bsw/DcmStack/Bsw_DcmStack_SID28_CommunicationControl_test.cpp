/**
 * \file    Bsw_DcmStack_SID28_CommunicationControl_test.cpp
 * \brief   UDS SID 0x28 CommunicationControl の、物理層（Can_Hw フェイク）を
 *          起点・終点とするフルコールチェーンテスト（GoogleTest /
 *          CMake native_chain_tests）。
 *
 * \details Bsw_DcmStack_SID19_SF01_ReadDtcCount_test.cpp 系列で確立した
 *          チェーンの型を適用する。リクエスト（3バイト）・応答（正応答2
 *          バイト／否定応答3バイト）とも Single Frame に収まるため、
 *          CanTp のマルチフレーム分割は関与しない。
 *
 *          0x28 は extendedSession 限定のため、
 *          Bsw_DcmStack_SID31_RoutineControl_test.cpp と同じ
 *          `EnterExtendedSession()` ヘルパーで事前にセッションを遷移させる。
 *
 *          本サービスは UDS 応答に加え、`BswM_Dcm_CommunicationMode_CurrentState()`
 *          （`Wrap_BswM.h` で呼び出し回数・引数をキャプチャ）への通知も
 *          検証対象に含む。BswM.c 自体は実体でリンクされるが `BswM_Init()`
 *          は本テストでは呼ばないため `BswM_Cfg==NULL` ガードでルール評価
 *          自体は走らない。よって「Dcm が正しい Dcm_CommunicationModeType
 *          値で BswM を呼んだか」のみを検証し、Com/Nm への実際の反映
 *          （`BswM_ApplyDcmCommMode()`）は対象外とする
 *          （旧 Bsw_Dcm_CommunicationControl_test.cpp 冒頭コメント参照）。
 *
 *          旧 Bsw_Dcm_CommunicationControl_test.cpp の全シナリオ（UDS要求
 *          データ（controlType/communicationType/長さ）の組み合わせで確認
 *          できる内容）をここへ移植する。
 */
#include <gtest/gtest.h>

extern "C" {
#include "Can.h"
#include "Can_Hw.h"
#include "CanIf.h"
#include "CanSM.h"
#include "PduR.h"
#include "CanTp.h"
#include "CanTp_Cfg.h"
#include "Dcm.h"
#include "Dcm_Cfg.h"
#include "Dem.h"
#include "Fake_Can_Hw.h"
#include "Fake_Det_Hw.h"
#include "Fake_Millis.h"
#include "Wrap_Can.h"
#include "Wrap_CanIf.h"
#include "Wrap_PduR.h"
#include "Wrap_CanTp.h"
#include "Wrap_BswM.h"
#include "Wrap_ComM.h"
}

namespace
{

// -----------------------------------------------------------------------
// テスト専用の最小 CanIf/PduR 設定（Bsw_DcmStack_SID19_SF01_ReadDtcCount_test.cpp
// と同一。ファイル冒頭コメント参照）。
// -----------------------------------------------------------------------

const CanIf_RxPduConfigType kTestCanIfRxPdu = {
    /* CanId */                0x7E0U,
    /* Hrh */                  0U,  /* Can_MainFunction_Read() が構築する Mailbox は常に Hoh=0 */
    /* UpperLayerRxPduId */    0U,
    /* Dlc */                  8U,
    /* RxIndicationFct */      PduR_CanIfRxIndication,
    /* ReadRxPduDataEnabled */ 0U
};

const CanIf_TxPduConfigType kTestCanIfTxPdu = {
    /* UpperLayerTxPduId */ 0U,
    /* CanId */             0x7E8U,
    /* Dlc */               8U,
    /* Hth */               0U,
    /* TxConfirmFct */      PduR_CanIfTxConfirmation
};

const CanIf_ConfigType kTestCanIfConfig = {
    /* TxPduConfig */ &kTestCanIfTxPdu,
    /* TxPduCount */  1U,
    /* RxPduConfig */ &kTestCanIfRxPdu,
    /* RxPduCount */  1U
};

const PduR_RxDestType kTestPduRRxDest = {
    /* Module */    PDUR_MODULE_CANTP,
    /* DestPduId */ CANTP_RX_SDU_ID,
    /* RxIndFct */  CanTp_RxIndication
};

const PduR_RxRoutingPathType kTestPduRRxPath = {
    /* SrcPduId */  0U,  /* = kTestCanIfRxPdu.UpperLayerRxPduId */
    /* Dests */     &kTestPduRRxDest,
    /* DestCount */ 1U
};

const PduR_TxRoutingPathType kTestPduRTxPath = {
    /* SrcPduId */             CANTP_PDUR_TX_SDU_ID,
    /* CanIfTxPduId */         0U,  /* = kTestCanIfTxPdu の登録順インデックス */
    /* ConfDestPduId */        0U,
    /* ConfFct */              CanTp_TxConfirmation,
    /* TransmitOverrideFct */  NULL,
    /* TransmitOverrideId */   0U
};

const PduR_PBConfigType kTestPduRConfig = {
    /* RxPaths */     &kTestPduRRxPath,
    /* RxPathCount */ 1U,
    /* TxPaths */     &kTestPduRTxPath,
    /* TxPathCount */ 1U
};

class Bsw_DcmStack_SID28_CommunicationControl_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        FakeCanHw_Reset();
        WrapCan_Reset();
        WrapCanIf_Reset();
        WrapPduR_Reset();
        WrapCanTp_Reset();
        WrapBswM_Reset();
        Suppressed_ComM_DcmDiagnostic = 1U;  // 本テストは通信管理(ComM/CanSM/Nm)が対象外
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制

        canConfig.filter.filterId = 0x0000U;
        canConfig.filter.mask     = 0x0000U;  // 全ID受理（Can_PBCfg.c の本番設定と同じ）
        canConfig.csPin           = 10U;
        canConfig.intPin          = 2U;
        canConfig.baudrate        = 500000U;
        canConfig.crystalFreq     = CAN_CRYSTAL_16MHZ;

        Can_Init(&canConfig);
        CanIf_Init(&kTestCanIfConfig);
        /* CanIf_Init() 直後は CanIf_ControllerMode[] が CAN_CS_STOPPED に
         * 巻き戻るため、CanIf_SetControllerMode() 経由で明示的に起動する
         * （Bsw_ComStack_Signal_Rx_test.cpp と同じ理由）。 */
        CanIf_SetControllerMode(0U, CAN_CS_STARTED);
        /* 本テストは CanSM 経由で FULL_COM を確立しないため、CanIf_Init()
         * 直後の既定値 CANIF_OFFLINE のままだと応答側の CanIf_Transmit() が
         * 常に E_NOT_OK になってしまう（Bsw_ComStack_Signal_Tx_test.cpp
         * と同じ理由）。 */
        CanIf_SetPduMode(0U, CANIF_ONLINE);
        PduR_Init(&kTestPduRConfig);
        /* CanIf_RxIndication() は無条件に CanSM_RxIndication() を呼ぶため、
         * CanSM 未初期化のままだと DET_E_UNINIT が毎回報告される
         * （Bsw_ComStack_Signal_Rx_test.cpp と同じ理由。CanSM 自体の状態機械は
         * 本テストの対象外）。 */
        CanSM_Init(NULL);
        CanTp_Init(NULL);
        Dem_Init(NULL);
        Dcm_Init(NULL);
        EnterExtendedSession();  // 0x28 は extendedSession 限定

        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;  // DeInit() のログを抑制
        WrapComM_Reset();  // 他のテストファイルへ影響を残さない
        CanSM_DeInit();
        CanIf_DeInit();
    }

    /* [0x10, 0x03] extendedDiagnosticSession を実際に Can_Hw から受信させ、
     * 物理応答が正応答であることまで確認する前提確立専用のヘルパー
     * （Bsw_DcmStack_SID31_RoutineControl_test.cpp と同じ理由）。 */
    void EnterExtendedSession()
    {
        FakeCanHw_RxId  = 0x7E0U;
        FakeCanHw_RxDlc = 8U;
        FakeCanHw_RxData[0] = 2U;
        FakeCanHw_RxData[1] = DCM_SID_SESSION_CTRL;
        FakeCanHw_RxData[2] = DCM_SESSION_EXTENDED;
        for (uint8 i = 3U; i < 8U; i++)
            FakeCanHw_RxData[i] = 0U;
        FakeCanHw_RxPendingCount = 1U;

        Can_MainFunction_Read();

        ASSERT_EQ(FakeCanHw_SendCount, 1U);
        ASSERT_EQ(FakeCanHw_LastSendData[1], 0x50U);  // 正応答確認（前提が崩れていないこと）
        FakeCanHw_Reset();  // 後続の本題リクエストの送信結果を素直に見るためリセット
    }

    /* [0x28, controlType, communicationType] を Can_Hw から受信させ、チェーン
     * 全体を駆動する（Act）。 */
    void SendCommunicationControl(uint8 controlType, uint8 communicationType)
    {
        FakeCanHw_RxId  = 0x7E0U;
        FakeCanHw_RxDlc = 8U;
        FakeCanHw_RxData[0] = 3U;
        FakeCanHw_RxData[1] = DCM_SID_COMM_CONTROL;
        FakeCanHw_RxData[2] = controlType;
        FakeCanHw_RxData[3] = communicationType;
        for (uint8 i = 4U; i < 8U; i++)
            FakeCanHw_RxData[i] = 0U;
        FakeCanHw_RxPendingCount = 1U;

        Can_MainFunction_Read();
    }

    Can_ConfigType canConfig;
};

// ------------------------------------------------------------
// Dcm_CommunicationModeType への変換（controlType + (communicationType-1)*4）
// ------------------------------------------------------------

TEST_F(Bsw_DcmStack_SID28_CommunicationControl_Test,
       OK_EnableRxTxNormalMapsToDcmEnableRxTxNormOnCanHw)
{
    /* 実行 (Act) */
    SendCommunicationControl(0x00U /* enableRxAndTx */, 0x01U /* normal */);

    /* 評価 (Assert): [0x68, 0x00] が Can_Hw まで到達し、BswM へも正しい値で
     * 通知されること。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x02U);  // SF PCI（UDSペイロード長=2）
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x68U);
    EXPECT_EQ(FakeCanHw_LastSendData[2], 0x00U);
    EXPECT_EQ(CallCount_BswM_Dcm_CommunicationMode_CurrentState, 1U);
    EXPECT_EQ(LastRequestedMode_BswM_Dcm_CommunicationMode_CurrentState, DCM_ENABLE_RX_TX_NORM);
}

TEST_F(Bsw_DcmStack_SID28_CommunicationControl_Test,
       OK_DisableRxTxNmMapsToDcmDisableRxTxNmOnCanHw)
{
    SendCommunicationControl(0x03U /* disableRxAndTx */, 0x02U /* NM */);

    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x02U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x68U);
    EXPECT_EQ(FakeCanHw_LastSendData[2], 0x03U);
    EXPECT_EQ(CallCount_BswM_Dcm_CommunicationMode_CurrentState, 1U);
    EXPECT_EQ(LastRequestedMode_BswM_Dcm_CommunicationMode_CurrentState, DCM_DISABLE_RX_TX_NM);
}

TEST_F(Bsw_DcmStack_SID28_CommunicationControl_Test,
       OK_EnableRxDisableTxNormAndNmMapsToDcmEnableRxDisableTxNormNmOnCanHw)
{
    SendCommunicationControl(0x01U /* enableRxAndDisableTx */, 0x03U /* normal + NM */);

    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x02U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x68U);
    EXPECT_EQ(FakeCanHw_LastSendData[2], 0x01U);
    EXPECT_EQ(CallCount_BswM_Dcm_CommunicationMode_CurrentState, 1U);
    EXPECT_EQ(LastRequestedMode_BswM_Dcm_CommunicationMode_CurrentState, DCM_ENABLE_RX_DISABLE_TX_NORM_NM);
}

TEST_F(Bsw_DcmStack_SID28_CommunicationControl_Test,
       OK_DisableRxEnableTxNormMapsToDcmDisableRxEnableTxNormOnCanHw)
{
    SendCommunicationControl(0x02U /* disableRxAndEnableTx */, 0x01U /* normal */);

    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(LastRequestedMode_BswM_Dcm_CommunicationMode_CurrentState, DCM_DISABLE_RX_ENABLE_TX_NORM);
}

// ------------------------------------------------------------
// 異常系
// ------------------------------------------------------------

TEST_F(Bsw_DcmStack_SID28_CommunicationControl_Test,
       NG_UnsupportedControlTypeProducesSubFuncNotSupportedResponseWithoutCallingBswMOnCanHw)
{
    SendCommunicationControl(0x04U /* enableRxAndDisableTxWithEnhancedAddressInformation、非対応 */, 0x01U);

    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x03U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x7FU);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DCM_NRC_SUB_FUNC_NOT_SUPPORTED);
    EXPECT_EQ(CallCount_BswM_Dcm_CommunicationMode_CurrentState, 0U);
}

TEST_F(Bsw_DcmStack_SID28_CommunicationControl_Test,
       NG_InvalidCommunicationTypeProducesRequestOutOfRangeResponseWithoutCallingBswMOnCanHw)
{
    SendCommunicationControl(0x00U, 0x00U /* 0 は未定義 */);

    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x03U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x7FU);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DCM_NRC_REQUEST_OUT_OF_RANGE);
    EXPECT_EQ(CallCount_BswM_Dcm_CommunicationMode_CurrentState, 0U);
}

TEST_F(Bsw_DcmStack_SID28_CommunicationControl_Test,
       NG_IncorrectLengthProducesIncorrectMessageLengthResponseWithoutCallingBswMOnCanHw)
{
    /* 準備 (Arrange): communicationType が無い [0x28, 0x00]（2バイト）を
     * 0x7E0 の受信バッファへセットする（SF: 02 28 00）。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 2U;
    FakeCanHw_RxData[1] = DCM_SID_COMM_CONTROL;
    FakeCanHw_RxData[2] = 0x00U;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert) */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x03U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x7FU);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DCM_NRC_INCORRECT_MESSAGE_LENGTH);
    EXPECT_EQ(CallCount_BswM_Dcm_CommunicationMode_CurrentState, 0U);
}

TEST_F(Bsw_DcmStack_SID28_CommunicationControl_Test,
       NG_UnsupportedControlTypeWithWrongLengthPrefersSubFuncNrcOnCanHw)
{
    /* [SWS_Dcm_00273]/[SWS_Dcm_00696]: サブ機能サポート確認は最小メッセージ長
     * 確認より先に行う処理順序（2026-09 是正）。controlType(uds[1])が
     * 不正かつ communicationType(uds[2]) が欠けている(udsLen=2<3)場合でも、
     * NRC 0x13(incorrectMessageLength)ではなく 0x12(subFunctionNotSupported)
     * を返すべき。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 2U;
    FakeCanHw_RxData[1] = DCM_SID_COMM_CONTROL;
    FakeCanHw_RxData[2] = 0xFFU;
    FakeCanHw_RxPendingCount = 1U;

    Can_MainFunction_Read();

    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x03U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x7FU);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DCM_NRC_SUB_FUNC_NOT_SUPPORTED);
    EXPECT_EQ(CallCount_BswM_Dcm_CommunicationMode_CurrentState, 0U);
}

// ------------------------------------------------------------
// Dcm_CommControlReset()（defaultSession への遷移で通信を初期状態へ戻す）
// ------------------------------------------------------------

TEST_F(Bsw_DcmStack_SID28_CommunicationControl_Test,
       OK_ReturnToDefaultSessionResetsToEnableRxTxNormNmOnCanHw)
{
    SendCommunicationControl(0x03U /* disableRxAndTx */, 0x03U /* normal + NM */);
    ASSERT_EQ(LastRequestedMode_BswM_Dcm_CommunicationMode_CurrentState, DCM_DISABLE_RX_TX_NORM_NM);
    WrapBswM_Reset();
    FakeCanHw_Reset();

    /* 準備 (Arrange): [0x10, 0x01] defaultSession を 0x7E0 の受信バッファへ
     * セットする（SF: 02 10 01）。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 2U;
    FakeCanHw_RxData[1] = DCM_SID_SESSION_CTRL;
    FakeCanHw_RxData[2] = DCM_SESSION_DEFAULT;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert) */
    EXPECT_EQ(CallCount_BswM_Dcm_CommunicationMode_CurrentState, 1U);
    EXPECT_EQ(LastRequestedMode_BswM_Dcm_CommunicationMode_CurrentState, DCM_ENABLE_RX_TX_NORM_NM);
}

}  // namespace
