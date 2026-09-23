/**
 * \file    Bsw_DcmStack_SID2F_IoControl_test.cpp
 * \brief   UDS SID 0x2F InputOutputControlByIdentifier の、物理層
 *          （Can_Hw フェイク）を起点・終点とするフルコールチェーンテスト
 *          （GoogleTest / CMake native_chain_tests）。
 *
 * \details Bsw_DcmStack_SID19_SF01_ReadDtcCount_test.cpp 系列で確立した
 *          チェーンの型を適用する。リクエスト・応答とも Single Frame に
 *          収まるため、CanTp のマルチフレーム分割は関与しない。
 *
 *          0x2F は extendedSession 限定（SecurityAccess は不要、
 *          Dcm_HandleIoControl() の \details 参照）のため、
 *          Bsw_DcmStack_SID31_RoutineControl_test.cpp と同じ
 *          `EnterExtendedSession()` ヘルパーで事前にセッションを遷移させる。
 *
 *          Rte_IoControl_Lamp_* 呼び出し先は `stub/Rte/Fake_Rte.c` の
 *          最小リンクスタブで満たしており、`Rte_IoControl_Lamp_
 *          GetCurrentLevel()` は常に level=0 を返す固定応答のため、
 *          returnControlToECU/freezeCurrentState の controlStatusRecord は
 *          常に0になる（shortTermAdjustment のみ要求値をそのまま echo する）。
 *
 *          本ファイルは本プロジェクトでこの SID を対象とする初めての
 *          テストファイル（従来は単体・チェーンいずれのテストも存在せず、
 *          移植元は無い。ゼロから新設）。
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

class Bsw_DcmStack_SID2F_IoControl_Test : public ::testing::Test
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
        EnterExtendedSession();  // 0x2F は extendedSession 限定

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
     * （Bsw_DcmStack_SID31_RoutineControl_test.cpp と同じ理由）。 */
    void EnterExtendedSession()
    {
        FakeCanHw_Reset();
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

    /* [0x2F, DID_H, DID_L, controlOption, (controlState)] を Can_Hw から
     * 受信させ、チェーン全体を駆動する（Act）。controlState が不要な場合は
     * stateLen=0 を渡す。 */
    void SendIoControl(uint16 did, uint8 controlOption, uint8 controlState, uint8 stateLen)
    {
        FakeCanHw_Reset();
        FakeCanHw_RxId  = 0x7E0U;
        FakeCanHw_RxDlc = 8U;
        FakeCanHw_RxData[0] = (uint8)(4U + stateLen);
        FakeCanHw_RxData[1] = DCM_SID_IO_CONTROL;
        FakeCanHw_RxData[2] = (uint8)(did >> 8U);
        FakeCanHw_RxData[3] = (uint8)(did & 0xFFU);
        FakeCanHw_RxData[4] = controlOption;
        if (stateLen > 0U)
            FakeCanHw_RxData[5] = controlState;
        for (uint8 i = (uint8)(5U + stateLen); i < 8U; i++)
            FakeCanHw_RxData[i] = 0U;
        FakeCanHw_RxPendingCount = 1U;

        Can_MainFunction_Read();
    }

    Can_ConfigType canConfig;
};

// ------------------------------------------------------------
// OK: shortTermAdjustment(0x03) は要求された controlState をそのまま
// controlStatusRecord として echo する。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID2F_IoControl_Test,
       IoControl_OK_ShortTermAdjustmentEchoesRequestedLevelOnCanHw)
{
    /* 実行 (Act) */
    SendIoControl(DCM_DID_RUN_LAMP, DCM_IOCTRL_SHORT_TERM_ADJUSTMENT, 1U, 1U);

    /* 評価 (Assert): 正応答 [0x6F, DID_H, DID_L, 0x03, 0x01] が Can_Hw まで
     * 到達すること。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x05U);  // SF PCI（UDSペイロード長=5）
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x6FU);
    EXPECT_EQ(FakeCanHw_LastSendData[2], (uint8)(DCM_DID_RUN_LAMP >> 8U));
    EXPECT_EQ(FakeCanHw_LastSendData[3], (uint8)(DCM_DID_RUN_LAMP & 0xFFU));
    EXPECT_EQ(FakeCanHw_LastSendData[4], DCM_IOCTRL_SHORT_TERM_ADJUSTMENT);
    EXPECT_EQ(FakeCanHw_LastSendData[5], 0x01U);  // echo された controlState
}

// ------------------------------------------------------------
// OK: resetToDefault(0x01) は controlStatusRecord=0（消灯）を返す。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID2F_IoControl_Test,
       IoControl_OK_ResetToDefaultProducesZeroLevelOnCanHw)
{
    SendIoControl(DCM_DID_FAULT_LAMP, DCM_IOCTRL_RESET_TO_DEFAULT, 0U, 0U);

    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x05U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x6FU);
    EXPECT_EQ(FakeCanHw_LastSendData[2], (uint8)(DCM_DID_FAULT_LAMP >> 8U));
    EXPECT_EQ(FakeCanHw_LastSendData[3], (uint8)(DCM_DID_FAULT_LAMP & 0xFFU));
    EXPECT_EQ(FakeCanHw_LastSendData[4], DCM_IOCTRL_RESET_TO_DEFAULT);
    EXPECT_EQ(FakeCanHw_LastSendData[5], 0x00U);
}

// ------------------------------------------------------------
// OK: returnControlToECU(0x00)/freezeCurrentState(0x02) はいずれも
// Rte_IoControl_Lamp_GetCurrentLevel() 経由の固定応答(level=0)を返す。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID2F_IoControl_Test,
       IoControl_OK_ReturnControlToEcuProducesPositiveResponseOnCanHw)
{
    SendIoControl(DCM_DID_ABS_LAMP, DCM_IOCTRL_RETURN_CONTROL_TO_ECU, 0U, 0U);

    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x6FU);
    EXPECT_EQ(FakeCanHw_LastSendData[2], (uint8)(DCM_DID_ABS_LAMP >> 8U));
    EXPECT_EQ(FakeCanHw_LastSendData[3], (uint8)(DCM_DID_ABS_LAMP & 0xFFU));
    EXPECT_EQ(FakeCanHw_LastSendData[4], DCM_IOCTRL_RETURN_CONTROL_TO_ECU);
}

TEST_F(Bsw_DcmStack_SID2F_IoControl_Test,
       IoControl_OK_FreezeCurrentStateProducesPositiveResponseOnCanHw)
{
    SendIoControl(DCM_DID_RUN_LAMP, DCM_IOCTRL_FREEZE_CURRENT_STATE, 0U, 0U);

    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x6FU);
    EXPECT_EQ(FakeCanHw_LastSendData[4], DCM_IOCTRL_FREEZE_CURRENT_STATE);
}

// ------------------------------------------------------------
// NG: 未対応 DID（ランプ用途以外）は NRC 0x31 requestOutOfRange になる。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID2F_IoControl_Test,
       IoControl_NG_UnknownDidReturnsRequestOutOfRangeOnCanHw)
{
    SendIoControl(0x0001U, DCM_IOCTRL_RETURN_CONTROL_TO_ECU, 0U, 0U);

    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x03U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_SID_IO_CONTROL);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DCM_NRC_REQUEST_OUT_OF_RANGE);
}

// ------------------------------------------------------------
// NG: shortTermAdjustment の controlState が 0/1 以外は
// NRC 0x31 requestOutOfRange になる。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID2F_IoControl_Test,
       IoControl_NG_InvalidShortTermAdjustmentStateReturnsRequestOutOfRangeOnCanHw)
{
    SendIoControl(DCM_DID_RUN_LAMP, DCM_IOCTRL_SHORT_TERM_ADJUSTMENT, 0x02U, 1U);

    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_SID_IO_CONTROL);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DCM_NRC_REQUEST_OUT_OF_RANGE);
}

// ------------------------------------------------------------
// NG: resetToDefault に余分な1バイト（controlState 付き、4バイト厳密一致の
// ため上限超過）は incorrectMessageLength (NRC 0x13) になる。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID2F_IoControl_Test,
       IoControl_NG_ResetToDefaultWithExtraByteReturnsIncorrectMessageLengthOnCanHw)
{
    SendIoControl(DCM_DID_RUN_LAMP, DCM_IOCTRL_RESET_TO_DEFAULT, 0x00U, 1U);

    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_SID_IO_CONTROL);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DCM_NRC_INCORRECT_MESSAGE_LENGTH);
}

}  // namespace
