/**
 * \file    Bsw_DcmStack_SID31_RoutineControl_test.cpp
 * \brief   UDS SID 0x31 RoutineControl の、物理層（Can_Hw フェイク）を
 *          起点・終点とするフルコールチェーンテスト（GoogleTest /
 *          CMake native_chain_tests）。
 *
 * \details Bsw_DcmStack_SID19_SF01_ReadDtcCount_test.cpp 系列で確立した
 *          チェーンの型（Can_ConfigType/CanIf_ConfigType/PduR_PBConfigType の
 *          テスト専用ローカル設定、Init() の呼び出し順序、Can_Hw への
 *          べた書きリクエスト）を適用する。リクエスト・応答とも4バイトで
 *          Single Frame に収まるため、CanTp のマルチフレーム分割は関与
 *          しない。
 *
 *          0x31 は extendedSession 限定のため、各テストは
 *          `EnterExtendedSession()`（[0x10, 0x03] を実際に Can_Hw から
 *          受信させ、物理応答が正応答であることまで確認する前提確立専用の
 *          ヘルパー。本題のリクエストそのものではないため、Bsw_Dcm_*_test.cpp
 *          と同様にヘルパー化して良いと判断した）で事前にセッションを
 *          遷移させる。対応 RID は DCM_RID_ENGINE_HEALTH_CHECK (0x0203) の
 *          みで、routineControlOptionRecord は定義しない
 *          （Dcm_HandleRoutineControl() 参照）。
 *
 *          旧 Bsw_Dcm_RoutineControl_test.cpp の全シナリオ（UDS要求データ
 *          （subFunc/RID/長さ）の組み合わせで確認できる内容）をここへ移植
 *          する。null ポインタ等の単体的な異常値チェックはそもそも存在しない
 *          （UDS はバイト列であり C API のように NULL を渡せないため）。
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

class Bsw_DcmStack_SID31_RoutineControl_Test : public ::testing::Test
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

        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;  // DeInit() のログを抑制
        CanSM_DeInit();
        CanIf_DeInit();
    }

    /* [0x10, 0x03] extendedDiagnosticSession を実際に Can_Hw から受信させ、
     * 物理応答が正応答であることまで確認する前提確立専用のヘルパー
     * （本題のリクエストではないため、ヘルパー化してもAct隠蔽の懸念は
     * 無いと判断した。ファイル冒頭コメント参照）。 */
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

    Can_ConfigType canConfig;
};

// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID31_RoutineControl_Test,
       RoutineControl_OK_StartWithExactLengthProducesExpectedResponseOnCanHw)
{
    EnterExtendedSession();

    /* 準備 (Arrange): [0x31, 0x01(start), RID_H=0x02, RID_L=0x03] を 0x7E0 の
     * 受信バッファへセットする（SF: 04 31 01 02 03）。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 4U;
    FakeCanHw_RxData[1] = DCM_SID_ROUTINE_CONTROL;
    FakeCanHw_RxData[2] = DCM_ROUTINE_SUBFUNC_START;
    FakeCanHw_RxData[3] = (uint8)(DCM_RID_ENGINE_HEALTH_CHECK >> 8U);
    FakeCanHw_RxData[4] = (uint8)(DCM_RID_ENGINE_HEALTH_CHECK & 0xFFU);
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert): [0x71, 0x01, 0x02, 0x03] が Can_Hw まで到達すること。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendDlc, 8U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x04U);  // SF PCI（UDSペイロード長=4）
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x71U);  // 肯定応答 SID (0x31+0x40)
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_ROUTINE_SUBFUNC_START);
    EXPECT_EQ(FakeCanHw_LastSendData[3], 0x02U);
    EXPECT_EQ(FakeCanHw_LastSendData[4], 0x03U);
}

// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID31_RoutineControl_Test,
       RoutineControl_OK_StopAfterStartProducesExpectedResponseOnCanHw)
{
    EnterExtendedSession();

    /* 準備 (Act 1): 先に start を送っておく（stop の前提）。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 4U;
    FakeCanHw_RxData[1] = DCM_SID_ROUTINE_CONTROL;
    FakeCanHw_RxData[2] = DCM_ROUTINE_SUBFUNC_START;
    FakeCanHw_RxData[3] = (uint8)(DCM_RID_ENGINE_HEALTH_CHECK >> 8U);
    FakeCanHw_RxData[4] = (uint8)(DCM_RID_ENGINE_HEALTH_CHECK & 0xFFU);
    FakeCanHw_RxPendingCount = 1U;
    Can_MainFunction_Read();
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    ASSERT_EQ(FakeCanHw_LastSendData[1], 0x71U);  // start が正応答であること（前提確認）
    FakeCanHw_Reset();

    /* 準備 (Arrange 2): [0x31, 0x02(stop), RID_H, RID_L] を 0x7E0 の受信
     * バッファへセットする（SF: 04 31 02 02 03）。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 4U;
    FakeCanHw_RxData[1] = DCM_SID_ROUTINE_CONTROL;
    FakeCanHw_RxData[2] = DCM_ROUTINE_SUBFUNC_STOP;
    FakeCanHw_RxData[3] = (uint8)(DCM_RID_ENGINE_HEALTH_CHECK >> 8U);
    FakeCanHw_RxData[4] = (uint8)(DCM_RID_ENGINE_HEALTH_CHECK & 0xFFU);
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act 2) */
    Can_MainFunction_Read();

    /* 評価 (Assert) */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x04U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x71U);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_ROUTINE_SUBFUNC_STOP);
}

// ------------------------------------------------------------
// NG: [SWS_Dcm_01140] overall length の厳密チェック（RID が1byte欠けている、
// 3バイト）は incorrectMessageLength (NRC 0x13) になる。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID31_RoutineControl_Test,
       RoutineControl_NG_TooShortProducesIncorrectMessageLengthResponseOnCanHw)
{
    EnterExtendedSession();

    /* 準備 (Arrange): RID が1byte欠けている [0x31, 0x01, 0x02]（3バイト）を
     * 0x7E0 の受信バッファへセットする（SF: 03 31 01 02）。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 3U;
    FakeCanHw_RxData[1] = DCM_SID_ROUTINE_CONTROL;
    FakeCanHw_RxData[2] = DCM_ROUTINE_SUBFUNC_START;
    FakeCanHw_RxData[3] = 0x02U;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert): 否定応答 [0x7F, 0x31, 0x13] が Can_Hw まで到達すること。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x03U);  // SF PCI（UDSペイロード長=3）
    EXPECT_EQ(FakeCanHw_LastSendData[1], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_SID_ROUTINE_CONTROL);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DCM_NRC_INCORRECT_MESSAGE_LENGTH);
}

// ------------------------------------------------------------
// NG: [SWS_Dcm_01140] 対応 RID は routineControlOptionRecord を定義しない
// ため、余分な1バイト付き（5バイト）は incorrectMessageLength になる。
// 拒否された要求が副作用を持たない（ルーチンが開始されていない）ことも
// 合わせて確認する: 続けて stop を送ると「未開始」の requestSequenceError
// で拒否されるはず。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID31_RoutineControl_Test,
       RoutineControl_NG_ExtraTrailingByteProducesIncorrectMessageLengthAndLeavesRoutineNotStartedOnCanHw)
{
    EnterExtendedSession();

    /* 準備 (Arrange 1): 余分な1バイト(0x99)付きの [0x31, 0x01, 0x02, 0x03, 0x99]
     * （5バイト）を 0x7E0 の受信バッファへセットする（SF: 05 31 01 02 03 99）。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 5U;
    FakeCanHw_RxData[1] = DCM_SID_ROUTINE_CONTROL;
    FakeCanHw_RxData[2] = DCM_ROUTINE_SUBFUNC_START;
    FakeCanHw_RxData[3] = (uint8)(DCM_RID_ENGINE_HEALTH_CHECK >> 8U);
    FakeCanHw_RxData[4] = (uint8)(DCM_RID_ENGINE_HEALTH_CHECK & 0xFFU);
    FakeCanHw_RxData[5] = 0x99U;
    FakeCanHw_RxData[6] = 0U;
    FakeCanHw_RxData[7] = 0U;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act 1) */
    Can_MainFunction_Read();

    /* 評価 (Assert 1): 否定応答 [0x7F, 0x31, 0x13] が Can_Hw まで到達すること。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x03U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_SID_ROUTINE_CONTROL);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DCM_NRC_INCORRECT_MESSAGE_LENGTH);
    FakeCanHw_Reset();

    /* 準備 (Arrange 2): 続けて stop [0x31, 0x02, RID_H, RID_L]（4バイト）を
     * 0x7E0 の受信バッファへセットする。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 4U;
    FakeCanHw_RxData[1] = DCM_SID_ROUTINE_CONTROL;
    FakeCanHw_RxData[2] = DCM_ROUTINE_SUBFUNC_STOP;
    FakeCanHw_RxData[3] = (uint8)(DCM_RID_ENGINE_HEALTH_CHECK >> 8U);
    FakeCanHw_RxData[4] = (uint8)(DCM_RID_ENGINE_HEALTH_CHECK & 0xFFU);
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act 2) */
    Can_MainFunction_Read();

    /* 評価 (Assert 2): 拒否された start が副作用を持たなかった（ルーチンが
     * 開始されていない）ことの傍証として、stop が requestSequenceError で
     * 拒否されること。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DCM_NRC_REQUEST_SEQUENCE_ERROR);
}

// ------------------------------------------------------------
// NG: defaultSession（extendedSession へ遷移しない）のままでは
// serviceNotSupportedInSession (NRC 0x7F) になる。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID31_RoutineControl_Test,
       RoutineControl_NG_DefaultSessionProducesServiceNotSupportedInSessionResponseOnCanHw)
{
    /* 準備 (Arrange): EnterExtendedSession() を呼ばず、defaultSession のまま
     * [0x31, 0x01, RID_H, RID_L] を 0x7E0 の受信バッファへセットする
     * （SF: 04 31 01 02 03）。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 4U;
    FakeCanHw_RxData[1] = DCM_SID_ROUTINE_CONTROL;
    FakeCanHw_RxData[2] = DCM_ROUTINE_SUBFUNC_START;
    FakeCanHw_RxData[3] = (uint8)(DCM_RID_ENGINE_HEALTH_CHECK >> 8U);
    FakeCanHw_RxData[4] = (uint8)(DCM_RID_ENGINE_HEALTH_CHECK & 0xFFU);
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert): 否定応答 [0x7F, 0x31, 0x7F] が Can_Hw まで到達すること。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x03U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_SID_ROUTINE_CONTROL);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DCM_NRC_SERVICE_NOT_SUPPORTED_IN_SESSION);
}

// ------------------------------------------------------------
// NG: 未対応の RID（0xFFFF）は requestOutOfRange (NRC 0x31) になる。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID31_RoutineControl_Test,
       RoutineControl_NG_UnsupportedRidProducesRequestOutOfRangeResponseOnCanHw)
{
    EnterExtendedSession();

    /* 準備 (Arrange): [0x31, 0x01, 0xFF, 0xFF] を 0x7E0 の受信バッファへ
     * セットする（SF: 04 31 01 FF FF）。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 4U;
    FakeCanHw_RxData[1] = DCM_SID_ROUTINE_CONTROL;
    FakeCanHw_RxData[2] = DCM_ROUTINE_SUBFUNC_START;
    FakeCanHw_RxData[3] = 0xFFU;
    FakeCanHw_RxData[4] = 0xFFU;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert): 否定応答 [0x7F, 0x31, 0x31] が Can_Hw まで到達すること。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x03U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_SID_ROUTINE_CONTROL);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DCM_NRC_REQUEST_OUT_OF_RANGE);
}

}  // namespace
