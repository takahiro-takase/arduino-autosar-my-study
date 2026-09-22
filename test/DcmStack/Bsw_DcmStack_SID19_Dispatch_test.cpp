/**
 * \file    Bsw_DcmStack_SID19_Dispatch_test.cpp
 * \brief   UDS SID 0x19 ReadDTCInformation の、特定のサブファンクションに
 *          閉じない「SID→サブファンクション振り分け」自体を検証する、物理層
 *          （Can_Hw フェイク）を起点・終点とするフルコールチェーンテスト
 *          （GoogleTest / CMake native_chain_tests）。
 *
 * \details Bsw_DcmStack_SID19_SF01/SF02/SF04/SF06/SF0A/SF14_*_test.cpp
 *          はそれぞれ1つの既知サブファンクションに閉じているため、
 *          「未対応のサブファンクション」や「サブファンクションバイトすら
 *          無いリクエスト」といった、どのサブファンクション固有ファイルにも
 *          属さないディスパッチ層のシナリオの置き場所が無かった
 *          （2026-09、ユーザー指摘により新設）。これらもUDS要求データ
 *          （サブファンクションバイトの有無・値）だけで作れるシナリオである
 *          ため、チェーンテスト側で検証する
 *          （[[feedback_test_file_one_scenario_per_file]]の精神に合わせ、
 *          特定サブファンクションに紐付かない横断的なシナリオのみを
 *          本ファイルへ集約する）。
 *
 *          今後、他のマルチサブファンクション SID（0x2F IOControl、
 *          0x31 RoutineControl 等）で同種のディスパッチ層テストが必要に
 *          なった場合も、`Bsw_DcmStack_SID<xx>_Dispatch_test.cpp` という
 *          同じ命名規則を使う。
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

class Bsw_DcmStack_SID19_Dispatch_Test : public ::testing::Test
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
         * （Bsw_ComStack_Rx_test.cpp と同じ理由）。 */
        CanIf_SetControllerMode(0U, CAN_CS_STARTED);
        /* 本テストは CanSM 経由で FULL_COM を確立しないため、CanIf_Init()
         * 直後の既定値 CANIF_OFFLINE のままだと応答側の CanIf_Transmit() が
         * 常に E_NOT_OK になってしまう（Bsw_ComStack_Tx_SendSignal_test.cpp
         * と同じ理由）。 */
        CanIf_SetPduMode(0U, CANIF_ONLINE);
        PduR_Init(&kTestPduRConfig);
        /* CanIf_RxIndication() は無条件に CanSM_RxIndication() を呼ぶため、
         * CanSM 未初期化のままだと DET_E_UNINIT が毎回報告される
         * （Bsw_ComStack_Rx_test.cpp と同じ理由。CanSM 自体の状態機械は
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
// NG: 未対応のサブファンクション（0x55、既知のどの値とも離れた値を使い、
// 将来 0x0B 等が追加されても意図せず衝突しないようにする）は
// subFunctionNotSupported (NRC 0x12) になる。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID19_Dispatch_Test,
       ReadDtcInfo_NG_UnsupportedSubFuncProducesSubFuncNotSupportedResponseOnCanHw)
{
    /* 準備 (Arrange): [0x19, 0x55] を 0x7E0 の受信バッファへセットする
     * （SF: 02 19 55）。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 2U;
    FakeCanHw_RxData[1] = DCM_SID_READ_DTC_INFO;
    FakeCanHw_RxData[2] = 0x55U;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert): 否定応答 [0x7F, 0x19, 0x12] が Can_Hw まで到達すること。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendDlc, 8U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x03U);  // SF PCI（UDSペイロード長=3）
    EXPECT_EQ(FakeCanHw_LastSendData[1], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_SID_READ_DTC_INFO);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DCM_NRC_SUB_FUNC_NOT_SUPPORTED);
}

// ------------------------------------------------------------
// NG: サブファンクションバイトすら無い（[0x19] のみ、1バイト）リクエストは
// incorrectMessageLength (NRC 0x13) になる（[SWS_Dcm_00696]、DSD submodule
// は要求長が最小長未満ならNRC 0x13を返す）。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID19_Dispatch_Test,
       ReadDtcInfo_NG_MissingSubFuncProducesIncorrectMessageLengthResponseOnCanHw)
{
    /* 準備 (Arrange): [0x19] のみを 0x7E0 の受信バッファへセットする
     * （SF: 01 19）。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 1U;
    FakeCanHw_RxData[1] = DCM_SID_READ_DTC_INFO;
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
