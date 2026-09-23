/**
 * \file    Bsw_DcmStack_SID19_SF0A_ReadDtcSupported_test.cpp
 * \brief   UDS SID 0x19/0x0A reportSupportedDTC の、物理層（Can_Hw フェイク）
 *          を起点・終点とするフルコールチェーンテスト（GoogleTest /
 *          CMake native_chain_tests）。
 *
 * \details Bsw_DcmStack_SID19_SF01_ReadDtcCount_test.cpp でチェーンの型
 *          （Can_ConfigType/CanIf_ConfigType/PduR_PBConfigType のテスト専用
 *          ローカル設定、Init() の呼び出し順序）を確立した続き。本ファイルは
 *          応答が `3 + DEM_EVENT_COUNT*4 = 59` バイト（7バイト超）になる
 *          reportSupportedDTC を対象とし、CanTp のマルチフレーム送信
 *          （FF → WAIT_FC → Flow Control 受信 → SEND_CF → CanTp_MainFunction()
 *          の反復呼び出しによる CF 送出）を実際の CAN フレーム単位で検証する。
 *
 *          手順の要点:
 *            1. [0x19, 0x0A] を Single Frame として 0x7E0 から受信させる
 *               （リクエスト自体は SID19_SF01 と同じ SF）。
 *            2. Dcm の応答生成 → CanTp_Transmit(59バイト) は SF に収まらない
 *               ため First Frame を送信して WAIT_FC へ遷移する（この FF 送信
 *               は手順1の Can_MainFunction_Read() 呼び出し内で同期的に
 *               完了する）。
 *            3. テスター役として Flow Control（CTS, BS=0, STmin=0）を 0x7E0
 *               から追加で受信させる（Can_Hw への2回目の注入）。CanTp は
 *               これを受けて SEND_CF へ遷移するが、この時点ではまだ CF は
 *               送信しない（CanTp.c 参照）。
 *            4. `CanTp_MainFunction()` を Os の周期呼び出しに見立てて8回
 *               （59バイト中 FF が運ぶ6バイトを除く53バイトを7バイトずつ、
 *               ceil(53/7)=8 フレーム）呼び、その都度 Can_Hw へ送信された
 *               Consecutive Frame を蓄積する。STmin=0 のため、待ち時間の
 *               シミュレーション（FakeMillis 操作）は不要。
 *            5. FF+全CFの実ペイロードを結合し、`Wrap_CanTp.h` が境界で
 *               キャプチャした `LastData_CanTp_Transmit`（Dcm が生成した
 *               元の59バイト UDS ペイロード）と完全一致することを確認する。
 *               これにより「Dcm が正しいバイト列を生成したか」
 *               （Bsw_Dcm_ReadDtcInfo_test.cpp が既に検証済み）とは別に、
 *               「CanTp が実際の CAN フレームへ正しく分割・送出したか」を
 *               本テストで新たに検証する。
 *
 *          CanIf/PduR のテスト専用ローカル設定・CAN ID/PDU ID の考え方は
 *          Bsw_DcmStack_SID19_SF01_ReadDtcCount_test.cpp と同一（重複
 *          定義。既存の全チェーンテストと同じ「1 OKシナリオ=1ファイル」の
 *          方針に合わせ、設定をファイルごとに複製する）。
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

class Bsw_DcmStack_SID19_SF0A_ReadDtcSupported_Test : public ::testing::Test
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
TEST_F(Bsw_DcmStack_SID19_SF0A_ReadDtcSupported_Test,
       ReadDtcSupported_OK_MultiFrameResponseReassemblesToExpectedPayloadOnCanHw)
{
    /* 準備 (Act 1): [0x19, 0x0A] を 0x7E0 の受信バッファへセットする
     * （SF: 02 19 0A）。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 2U;
    FakeCanHw_RxData[1] = DCM_SID_READ_DTC_INFO;
    FakeCanHw_RxData[2] = DCM_DTC_SUBFUNC_REPORT_SUPPORTED;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act 1): リクエスト受信 → Dcm 応答生成 → CanTp_Transmit(59バイト)
     * → First Frame 送信（WAIT_FC へ遷移）まで同期的に進む。 */
    Can_MainFunction_Read();

    /* 評価 (Assert 1): Dcm が生成した UDS ペイロード自体は
     * Bsw_Dcm_ReadDtcInfo_test.cpp と同じ期待値（境界は Wrap_CanTp.h で
     * キャプチャ）。 */
    ASSERT_EQ(CallCount_CanTp_Transmit, 1U);
    ASSERT_EQ(LastLength_CanTp_Transmit, (uint8)(3U + DEM_EVENT_COUNT * 4U));  // 59
    EXPECT_EQ(LastData_CanTp_Transmit[0], 0x59U);
    EXPECT_EQ(LastData_CanTp_Transmit[1], DCM_DTC_SUBFUNC_REPORT_SUPPORTED);
    EXPECT_EQ(LastData_CanTp_Transmit[2], DEM_STATUS_AVAILABILITY_MASK);

    /* First Frame が Can_Hw まで到達したことを確認し、内容を退避する
     * （次の送信で FakeCanHw_LastSendData が上書きされるため）。
     * FF PCI: 0x10 | (len>>8) 、続けて len&0xFF、その後6バイトの実データ。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendDlc, 8U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x10U);  // FF PCI（len=59<256のため上位ニブルのみ）
    EXPECT_EQ(FakeCanHw_LastSendData[1], 59U);    // len 下位8bit

    uint8 reassembled[64] = { 0U };
    uint8 pos = 0U;
    for (uint8 i = 0U; i < 6U; i++)
        reassembled[pos++] = FakeCanHw_LastSendData[2U + i];

    /* 準備 (Arrange 2): テスター役として Flow Control（CTS, BS=0, STmin=0）を
     * 0x7E0 から追加で受信させる（SF ではなく生の8バイトフレームを直接送る）。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 0x30U;  // FC PCI（fs=CTS）
    FakeCanHw_RxData[1] = 0x00U;  // BlockSize=0（無制限）
    FakeCanHw_RxData[2] = 0x00U;  // STmin=0
    for (uint8 i = 3U; i < 8U; i++)
        FakeCanHw_RxData[i] = 0U;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act 2): FC 受信により CanTp は SEND_CF へ遷移するが、この時点
     * ではまだ CF は送信しない（CanTp.c 参照）。 */
    Can_MainFunction_Read();
    ASSERT_EQ(FakeCanHw_SendCount, 1U);  // FC 受信自体は Can_Hw への送信を生まない

    /* 実行 (Act 3): Os から周期的に呼ばれる CanTp_MainFunction() を、59バイト
     * 中の残り53バイトを7バイトずつ運ぶ Consecutive Frame の数（ceil(53/7)=8）
     * だけ繰り返し駆動し、その都度 Can_Hw へ送信された内容を蓄積する。 */
    for (uint8 cf = 0U; cf < 8U; cf++)
    {
        CanTp_MainFunction();

        ASSERT_EQ(FakeCanHw_SendCount, (uint32)(2U + cf))
            << "CF #" << (unsigned)(cf + 1U) << " が送信されていない";
        EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
        EXPECT_EQ(FakeCanHw_LastSendData[0], (uint8)(0x20U | ((cf + 1U) & 0x0FU)))
            << "CF #" << (unsigned)(cf + 1U) << " の SN 不一致";

        uint16 remaining = 59U - pos;
        uint8  copyLen   = (remaining > 7U) ? 7U : (uint8)remaining;
        for (uint8 i = 0U; i < copyLen; i++)
            reassembled[pos++] = FakeCanHw_LastSendData[1U + i];
    }

    /* 評価 (Assert 2): 8本の CF 送信で全53バイトを運び終え、CanTp は
     * IDLE（ビジーでない）へ戻っていること。 */
    ASSERT_EQ(pos, 59U);
    EXPECT_EQ(FakeCanHw_SendCount, 9U);  // FF 1 + CF 8
    EXPECT_EQ(CanTp_IsTxBusy(), (boolean)0U);

    /* 評価 (Assert 3): 実際に CAN フレームへ分割・送出された内容を結合すると、
     * Dcm が生成した元の59バイト UDS ペイロード（Assert 1 で確認済み）と
     * 完全一致すること。「CanTp が正しく分割・送出したか」を検証する
     * 本テスト最大の目的。 */
    for (uint8 i = 0U; i < 59U; i++)
    {
        EXPECT_EQ(reassembled[i], LastData_CanTp_Transmit[i]) << "byte " << (unsigned)i;
    }
}

}  // namespace
