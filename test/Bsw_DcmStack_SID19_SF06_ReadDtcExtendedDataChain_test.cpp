/**
 * \file    Bsw_DcmStack_SID19_SF06_ReadDtcExtendedDataChain_test.cpp
 * \brief   UDS SID 0x19/0x06 reportExtendedDataRecordByDTCNumber の、物理層
 *          （Can_Hw フェイク）を起点・終点とするフルコールチェーンテスト
 *          （GoogleTest / CMake native_chain_tests）。
 *
 * \details Bsw_DcmStack_SID19_SF0A_ReadDtcSupportedChain_test.cpp で確立した
 *          マルチフレームの型（FF → Flow Control 注入 → CanTp_MainFunction()
 *          反復呼び出しによる CF 送出）を適用する。応答は8バイト
 *          （[0x59, 0x06, DTC_H, DTC_M, DTC_L, status, recordNumber,
 *          occurrenceCounter]）で Single Frame の7バイト制限を1バイトだけ
 *          超えるため、FF（6バイト）+ CF 1本（残り2バイト）という最小構成の
 *          マルチフレームになる。
 *
 *          リクエストは [0x19, 0x06, DTC_H, DTC_M, DTC_L, recordNumber]
 *          （6バイト、Single Frame に収まる）。DTC は
 *          DEM_EVENT_ENGINE_OVERHEAT（DTC=0x000101）を
 *          Dem_SetEventStatus() で FAILED 確定させたものを使う
 *          （Bsw_Dcm_ReadDtcInfo_test.cpp と同じ手段、新規 Wrap 関数は不要）。
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
#include "Dem_Cfg.h"
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
// テスト専用の最小 CanIf/PduR 設定（Bsw_DcmStack_SID19_SF01_ReadDtcCountChain_test.cpp
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

class Bsw_DcmStack_SID19_SF06_ReadDtcExtendedDataChain_Test : public ::testing::Test
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
         * （Bsw_ComStack_RxChain_test.cpp と同じ理由）。 */
        CanIf_SetControllerMode(0U, CAN_CS_STARTED);
        /* 本テストは CanSM 経由で FULL_COM を確立しないため、CanIf_Init()
         * 直後の既定値 CANIF_OFFLINE のままだと応答側の CanIf_Transmit() が
         * 常に E_NOT_OK になってしまう（Bsw_ComStack_TxChain_SendSignal_test.cpp
         * と同じ理由）。 */
        CanIf_SetPduMode(0U, CANIF_ONLINE);
        PduR_Init(&kTestPduRConfig);
        /* CanIf_RxIndication() は無条件に CanSM_RxIndication() を呼ぶため、
         * CanSM 未初期化のままだと DET_E_UNINIT が毎回報告される
         * （Bsw_ComStack_RxChain_test.cpp と同じ理由。CanSM 自体の状態機械は
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

    Can_ConfigType canConfig;
};

// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID19_SF06_ReadDtcExtendedDataChain_Test,
       ReadDtcExtendedData_OK_MultiFrameResponseReassemblesToExpectedPayloadOnCanHw)
{
    /* 準備 (Act 1): DEM_EVENT_ENGINE_OVERHEAT（limit=2、DTC=0x000101）を
     * FAILED 確定させてから、[0x19, 0x06, 0x00,0x01,0x01, recordNumber=0x01]
     * を 0x7E0 の受信バッファへセットする（SF: 06 19 06 00 01 01 01）。 */
    (void)Dem_SetEventStatus(DEM_EVENT_ENGINE_OVERHEAT, DEM_EVENT_STATUS_FAILED);
    (void)Dem_SetEventStatus(DEM_EVENT_ENGINE_OVERHEAT, DEM_EVENT_STATUS_FAILED);

    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 6U;
    FakeCanHw_RxData[1] = DCM_SID_READ_DTC_INFO;
    FakeCanHw_RxData[2] = DCM_DTC_SUBFUNC_REPORT_EXTDATA;
    FakeCanHw_RxData[3] = 0x00U;  // DTC_H
    FakeCanHw_RxData[4] = 0x01U;  // DTC_M
    FakeCanHw_RxData[5] = 0x01U;  // DTC_L
    FakeCanHw_RxData[6] = DCM_EXTENDED_DATA_RECORD_NUMBER;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act 1): リクエスト受信 → Dcm 応答生成 → CanTp_Transmit(8バイト)
     * → First Frame 送信（WAIT_FC へ遷移）まで同期的に進む。 */
    Can_MainFunction_Read();

    ASSERT_EQ(CallCount_CanTp_Transmit, 1U);
    ASSERT_EQ(LastLength_CanTp_Transmit, 8U);
    EXPECT_EQ(LastData_CanTp_Transmit[0], 0x59U);
    EXPECT_EQ(LastData_CanTp_Transmit[1], DCM_DTC_SUBFUNC_REPORT_EXTDATA);
    EXPECT_EQ(LastData_CanTp_Transmit[6], DCM_EXTENDED_DATA_RECORD_NUMBER);

    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x10U);  // FF PCI（len=8<256）
    EXPECT_EQ(FakeCanHw_LastSendData[1], 8U);     // len 下位8bit

    uint8 reassembled[16] = { 0U };
    uint8 pos = 0U;
    for (uint8 i = 0U; i < 6U; i++)
        reassembled[pos++] = FakeCanHw_LastSendData[2U + i];

    /* 準備 (Arrange 2): テスター役として Flow Control（CTS, BS=0, STmin=0）を
     * 0x7E0 から追加で受信させる。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 0x30U;
    FakeCanHw_RxData[1] = 0x00U;
    FakeCanHw_RxData[2] = 0x00U;
    for (uint8 i = 3U; i < 8U; i++)
        FakeCanHw_RxData[i] = 0U;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act 2) */
    Can_MainFunction_Read();
    ASSERT_EQ(FakeCanHw_SendCount, 1U);  // FC 受信自体は Can_Hw への送信を生まない

    /* 実行 (Act 3): 残り2バイトは Consecutive Frame 1本で運びきれる
     * （ceil((8-6)/7)=1）。 */
    CanTp_MainFunction();

    ASSERT_EQ(FakeCanHw_SendCount, 2U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x21U);  // CF PCI（SN=1）

    uint16 remaining = 8U - pos;
    uint8  copyLen   = (remaining > 7U) ? 7U : (uint8)remaining;
    for (uint8 i = 0U; i < copyLen; i++)
        reassembled[pos++] = FakeCanHw_LastSendData[1U + i];

    /* 評価 (Assert): CanTp が IDLE へ戻り、CAN フレームへ分割・送出された
     * 内容を結合すると Dcm が生成した元の8バイト UDS ペイロードと完全
     * 一致すること。 */
    ASSERT_EQ(pos, 8U);
    EXPECT_EQ(CanTp_IsTxBusy(), (boolean)0U);
    for (uint8 i = 0U; i < 8U; i++)
    {
        EXPECT_EQ(reassembled[i], LastData_CanTp_Transmit[i]) << "byte " << (unsigned)i;
    }
}

// ------------------------------------------------------------
// recordNumber=0xFF（DCM_RECORD_NUMBER_ALL、[SWS_Dcm_00441]の「全レコード
// 要求」エイリアス）を送っても、実レコード番号0x01を指定した場合と同じ応答
// になる（応答のrecordNumberフィールドは実レコード番号であり、要求の0xFFを
// そのままechoしない）ことを確認する。UDS要求データ（recordNumber）と
// 外部データ（Dem に記録された唯一のレコード）の組み合わせで確認できる
// シナリオのため、チェーンテスト側に追加する。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID19_SF06_ReadDtcExtendedDataChain_Test,
       ReadDtcExtendedData_OK_RecordNumber0xFFAliasReturnsSameMultiFrameResponseOnCanHw)
{
    /* 準備 (Act 1): DEM_EVENT_ENGINE_OVERHEAT を FAILED 確定させてから、
     * recordNumber=0xFF ([0x19, 0x06, 0x00,0x01,0x01, 0xFF]) を 0x7E0 の
     * 受信バッファへセットする（SF: 06 19 06 00 01 01 FF）。 */
    (void)Dem_SetEventStatus(DEM_EVENT_ENGINE_OVERHEAT, DEM_EVENT_STATUS_FAILED);
    (void)Dem_SetEventStatus(DEM_EVENT_ENGINE_OVERHEAT, DEM_EVENT_STATUS_FAILED);

    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 6U;
    FakeCanHw_RxData[1] = DCM_SID_READ_DTC_INFO;
    FakeCanHw_RxData[2] = DCM_DTC_SUBFUNC_REPORT_EXTDATA;
    FakeCanHw_RxData[3] = 0x00U;
    FakeCanHw_RxData[4] = 0x01U;
    FakeCanHw_RxData[5] = 0x01U;
    FakeCanHw_RxData[6] = DCM_RECORD_NUMBER_ALL;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act 1) */
    Can_MainFunction_Read();

    /* 評価 (Assert 1): 応答の recordNumber フィールド（LastData_CanTp_Transmit[6]）
     * が要求値0xFFではなく実レコード番号0x01であること。 */
    ASSERT_EQ(CallCount_CanTp_Transmit, 1U);
    ASSERT_EQ(LastLength_CanTp_Transmit, 8U);
    EXPECT_EQ(LastData_CanTp_Transmit[0], 0x59U);
    EXPECT_EQ(LastData_CanTp_Transmit[1], DCM_DTC_SUBFUNC_REPORT_EXTDATA);
    EXPECT_EQ(LastData_CanTp_Transmit[6], DCM_EXTENDED_DATA_RECORD_NUMBER);

    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x10U);  // FF PCI（len=8<256）
    EXPECT_EQ(FakeCanHw_LastSendData[1], 8U);     // len 下位8bit

    uint8 reassembled[16] = { 0U };
    uint8 pos = 0U;
    for (uint8 i = 0U; i < 6U; i++)
        reassembled[pos++] = FakeCanHw_LastSendData[2U + i];

    /* 準備 (Arrange 2): テスター役として Flow Control（CTS, BS=0, STmin=0）を
     * 0x7E0 から追加で受信させる。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 0x30U;
    FakeCanHw_RxData[1] = 0x00U;
    FakeCanHw_RxData[2] = 0x00U;
    for (uint8 i = 3U; i < 8U; i++)
        FakeCanHw_RxData[i] = 0U;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act 2) */
    Can_MainFunction_Read();
    ASSERT_EQ(FakeCanHw_SendCount, 1U);  // FC 受信自体は Can_Hw への送信を生まない

    /* 実行 (Act 3) */
    CanTp_MainFunction();

    ASSERT_EQ(FakeCanHw_SendCount, 2U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x21U);  // CF PCI（SN=1）

    uint16 remaining = 8U - pos;
    uint8  copyLen   = (remaining > 7U) ? 7U : (uint8)remaining;
    for (uint8 i = 0U; i < copyLen; i++)
        reassembled[pos++] = FakeCanHw_LastSendData[1U + i];

    /* 評価 (Assert 2): CanTp が IDLE へ戻り、CAN フレームへ分割・送出された
     * 内容を結合すると Dcm が生成した元の8バイト UDS ペイロード（実レコード
     * 番号0x01を含む）と完全一致すること。 */
    ASSERT_EQ(pos, 8U);
    EXPECT_EQ(CanTp_IsTxBusy(), (boolean)0U);
    for (uint8 i = 0U; i < 8U; i++)
    {
        EXPECT_EQ(reassembled[i], LastData_CanTp_Transmit[i]) << "byte " << (unsigned)i;
    }
    EXPECT_EQ(reassembled[6], DCM_EXTENDED_DATA_RECORD_NUMBER);  // 要求0xFFをechoしない
}

// ------------------------------------------------------------
// NG: DTC/recordNumber が揃わない（[0x19, 0x06, DTC_H, DTC_M] のみ、4バイト。
// 6バイト必須）リクエストは incorrectMessageLength (NRC 0x13) になる（OK と
// 同じファイルに同居させる方針、[[feedback_test_file_one_scenario_per_file]]）。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID19_SF06_ReadDtcExtendedDataChain_Test,
       ReadDtcExtendedData_NG_TooShortRequestProducesIncorrectMessageLengthResponseOnCanHw)
{
    /* 準備 (Arrange): DTC/recordNumber が揃わない [0x19, 0x06, 0x00, 0x01]
     * を 0x7E0 の受信バッファへセットする（SF: 04 19 06 00 01）。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 4U;
    FakeCanHw_RxData[1] = DCM_SID_READ_DTC_INFO;
    FakeCanHw_RxData[2] = DCM_DTC_SUBFUNC_REPORT_EXTDATA;
    FakeCanHw_RxData[3] = 0x00U;
    FakeCanHw_RxData[4] = 0x01U;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert): 否定応答 [0x7F, 0x19, 0x13] が Can_Hw まで到達すること。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendDlc, 8U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x03U);  // SF PCI（UDSペイロード長=3）
    EXPECT_EQ(FakeCanHw_LastSendData[1], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_SID_READ_DTC_INFO);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DCM_NRC_INCORRECT_MESSAGE_LENGTH);
}

// ------------------------------------------------------------
// NG: 一度も FAILED 確定していない DTC を指定すると requestOutOfRange
// (NRC 0x31) になる（Dem_SetEventStatus() を一切呼ばない、ExtendedData
// 未記録の状態のまま）。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID19_SF06_ReadDtcExtendedDataChain_Test,
       ReadDtcExtendedData_NG_NeverFailedDtcProducesRequestOutOfRangeResponseOnCanHw)
{
    /* 準備 (Arrange): ExtendedData 未記録のまま
     * [0x19, 0x06, 0x00,0x01,0x01, recordNumber=0x01] を 0x7E0 の受信
     * バッファへセットする（SF: 06 19 06 00 01 01 01）。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 6U;
    FakeCanHw_RxData[1] = DCM_SID_READ_DTC_INFO;
    FakeCanHw_RxData[2] = DCM_DTC_SUBFUNC_REPORT_EXTDATA;
    FakeCanHw_RxData[3] = 0x00U;
    FakeCanHw_RxData[4] = 0x01U;
    FakeCanHw_RxData[5] = 0x01U;
    FakeCanHw_RxData[6] = DCM_EXTENDED_DATA_RECORD_NUMBER;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert): 否定応答 [0x7F, 0x19, 0x31] が Can_Hw まで到達すること。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendDlc, 8U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x03U);  // SF PCI（UDSペイロード長=3）
    EXPECT_EQ(FakeCanHw_LastSendData[1], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_SID_READ_DTC_INFO);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DCM_NRC_REQUEST_OUT_OF_RANGE);
}

}  // namespace
