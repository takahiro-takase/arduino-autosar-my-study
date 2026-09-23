/**
 * \file    Bsw_DcmStack_SID19_SF02_ReadDtcByMask_test.cpp
 * \brief   UDS SID 0x19/0x02 reportDTCByStatusMask の、物理層（Can_Hw
 *          フェイク）を起点・終点とするフルコールチェーンテスト
 *          （GoogleTest / CMake native_chain_tests）。
 *
 * \details Bsw_DcmStack_SID19_SF01_ReadDtcCount_test.cpp（Single Frame
 *          専用）と Bsw_DcmStack_SID19_SF0A_ReadDtcSupported_test.cpp
 *          （マルチフレーム専用）で確立した2つのチェーンの型を、本ファイルの
 *          中で両方使い分ける。reportDTCByStatusMask の応答長は
 *          `3 + 4*一致件数` で一致件数に依存するため、一致件数を作り分ける
 *          ことで Single Frame（0件・1件）と マルチフレーム（2件）の両方を
 *          同じサブファンクションで作れる。
 *
 *          DTC 一致件数の作り方: DEM_EVENT_ENGINE_OVERHEAT
 *          （DEM_DEBOUNCE_LIMIT=2）と DEM_EVENT_CAN_BUSOFF
 *          （DEM_DEBOUNCE_LIMIT=1）を Dem_SetEventStatus() で FAILED 確定
 *          させ、statusMask=DEM_STATUS_TEST_FAILED でフィルタする
 *          （Bsw_Dcm_ReadDtcInfo_test.cpp と同じ手段、新規 Wrap 関数は不要）。
 *
 *          マルチフレーム（2件、11バイト）のケースは、DTC レコードの内容
 *          （並び順は Dem 内部テーブル順に依存し本テストの関心事ではない）を
 *          個別にハードコードせず、`Wrap_CanTp.h` が境界でキャプチャした
 *          `LastData_CanTp_Transmit`（Dcm が生成した元のペイロード）と、
 *          実際に CAN フレームへ分割・送出された内容を再結合したものが
 *          完全一致することだけを確認する
 *          （Bsw_DcmStack_SID19_SF0A_ReadDtcSupported_test.cpp と同じ
 *          検証方針）。
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

class Bsw_DcmStack_SID19_SF02_ReadDtcByMask_Test : public ::testing::Test
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

    Can_ConfigType canConfig;
};

// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID19_SF02_ReadDtcByMask_Test,
       ReadDtcByMask_OK_ImpossibleMaskProducesHeaderOnlySingleFrameResponseOnCanHw)
{
    /* 準備 (Arrange): [0x19, 0x02, mask=0x00]（どの DTC とも一致しない）を
     * 0x7E0 の受信バッファへセットする（SF: 03 19 02 00）。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 3U;
    FakeCanHw_RxData[1] = DCM_SID_READ_DTC_INFO;
    FakeCanHw_RxData[2] = DCM_DTC_SUBFUNC_REPORT_BY_MASK;
    FakeCanHw_RxData[3] = 0x00U;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert): DTC 列挙部分の無い [0x59, 0x02, availMask] のみが
     * Can_Hw まで到達すること。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendDlc, 8U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x03U);  // SF PCI（UDSペイロード長=3）
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x59U);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_DTC_SUBFUNC_REPORT_BY_MASK);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DEM_STATUS_AVAILABILITY_MASK);
}

// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID19_SF02_ReadDtcByMask_Test,
       ReadDtcByMask_OK_OneMatchingDtcProducesSevenByteSingleFrameResponseOnCanHw)
{
    /* 準備 (Arrange): DEM_EVENT_ENGINE_OVERHEAT（limit=2）を testFailed
     * 確定させてから、[0x19, 0x02, mask=testFailedのみ] を送る。一致件数
     * 1件なら応答は 3+4=7 バイトで Single Frame の境界に収まる。 */
    (void)Dem_SetEventStatus(DEM_EVENT_ENGINE_OVERHEAT, DEM_EVENT_STATUS_FAILED);
    (void)Dem_SetEventStatus(DEM_EVENT_ENGINE_OVERHEAT, DEM_EVENT_STATUS_FAILED);

    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 3U;
    FakeCanHw_RxData[1] = DCM_SID_READ_DTC_INFO;
    FakeCanHw_RxData[2] = DCM_DTC_SUBFUNC_REPORT_BY_MASK;
    FakeCanHw_RxData[3] = DEM_STATUS_TEST_FAILED;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert): [0x59, 0x02, availMask, DTC_H, DTC_M, DTC_L, status]
     * （7バイト、SF PCI=0x07）が Can_Hw まで到達すること。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendDlc, 8U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x07U);  // SF PCI（UDSペイロード長=7）
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x59U);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_DTC_SUBFUNC_REPORT_BY_MASK);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DEM_STATUS_AVAILABILITY_MASK);
}

// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID19_SF02_ReadDtcByMask_Test,
       ReadDtcByMask_OK_TwoMatchingDtcsProduceMultiFrameResponseOnCanHw)
{
    /* 準備 (Act 1): DEM_EVENT_ENGINE_OVERHEAT（limit=2）と
     * DEM_EVENT_CAN_BUSOFF（limit=1）を testFailed 確定させる。一致件数
     * 2件なら応答は 3+4*2=11 バイトで Single Frame を超えマルチフレームに
     * なる。 */
    (void)Dem_SetEventStatus(DEM_EVENT_ENGINE_OVERHEAT, DEM_EVENT_STATUS_FAILED);
    (void)Dem_SetEventStatus(DEM_EVENT_ENGINE_OVERHEAT, DEM_EVENT_STATUS_FAILED);
    (void)Dem_SetEventStatus(DEM_EVENT_CAN_BUSOFF, DEM_EVENT_STATUS_FAILED);

    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 3U;
    FakeCanHw_RxData[1] = DCM_SID_READ_DTC_INFO;
    FakeCanHw_RxData[2] = DCM_DTC_SUBFUNC_REPORT_BY_MASK;
    FakeCanHw_RxData[3] = DEM_STATUS_TEST_FAILED;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act 1): リクエスト受信 → Dcm 応答生成 → CanTp_Transmit(11バイト)
     * → First Frame 送信（WAIT_FC へ遷移）まで同期的に進む。 */
    Can_MainFunction_Read();

    ASSERT_EQ(CallCount_CanTp_Transmit, 1U);
    ASSERT_EQ(LastLength_CanTp_Transmit, 11U);
    EXPECT_EQ(LastData_CanTp_Transmit[0], 0x59U);
    EXPECT_EQ(LastData_CanTp_Transmit[1], DCM_DTC_SUBFUNC_REPORT_BY_MASK);
    EXPECT_EQ(LastData_CanTp_Transmit[2], DEM_STATUS_AVAILABILITY_MASK);

    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x10U);  // FF PCI（len=11<256）
    EXPECT_EQ(FakeCanHw_LastSendData[1], 11U);    // len 下位8bit

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

    /* 実行 (Act 3): 残り5バイトは Consecutive Frame 1本で運びきれる
     * （ceil((11-6)/7)=1）。 */
    CanTp_MainFunction();

    ASSERT_EQ(FakeCanHw_SendCount, 2U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x21U);  // CF PCI（SN=1）

    uint16 remaining = 11U - pos;
    uint8  copyLen   = (remaining > 7U) ? 7U : (uint8)remaining;
    for (uint8 i = 0U; i < copyLen; i++)
        reassembled[pos++] = FakeCanHw_LastSendData[1U + i];

    /* 評価 (Assert): CanTp が IDLE へ戻り、CAN フレームへ分割・送出された
     * 内容を結合すると Dcm が生成した元の11バイト UDS ペイロードと完全
     * 一致すること。 */
    ASSERT_EQ(pos, 11U);
    EXPECT_EQ(CanTp_IsTxBusy(), (boolean)0U);
    for (uint8 i = 0U; i < 11U; i++)
    {
        EXPECT_EQ(reassembled[i], LastData_CanTp_Transmit[i]) << "byte " << (unsigned)i;
    }
}

// ------------------------------------------------------------
// NG: statusMask バイトが無い（[0x19, 0x02] のみ、2バイト）リクエストは
// incorrectMessageLength (NRC 0x13) の否定応答になる（OK と同じファイルに
// 同居させる方針、[[feedback_test_file_one_scenario_per_file]]）。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID19_SF02_ReadDtcByMask_Test,
       ReadDtcByMask_NG_MissingStatusMaskProducesIncorrectMessageLengthResponseOnCanHw)
{
    /* 準備 (Arrange): statusMask を欠落させた [0x19, 0x02] を 0x7E0 の
     * 受信バッファへセットする（SF: 02 19 02）。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 2U;
    FakeCanHw_RxData[1] = DCM_SID_READ_DTC_INFO;
    FakeCanHw_RxData[2] = DCM_DTC_SUBFUNC_REPORT_BY_MASK;
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

}  // namespace
