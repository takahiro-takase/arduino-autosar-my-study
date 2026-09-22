/**
 * \file    Bsw_DcmStack_SID19_SF01_ReadDtcCount_test.cpp
 * \brief   UDS SID 0x19/0x01 reportNumberOfDTCByStatusMask の、物理層（Can_Hw
 *          フェイク）を起点・終点とするフルコールチェーンテスト（GoogleTest /
 *          CMake native_chain_tests）。
 *
 * \details 他の `Bsw_Dcm_*_test.cpp` は `Dcm_ComIndication()` に生の UDS
 *          バイト列を直接渡し `Wrap_CanTp.h` で応答をキャプチャする
 *          「入口と出口だけを見る」ブラックボックステストだが、本ファイルは
 *          その外側 — CAN物理フレームの受信（`Can_Hw`）から
 *          Can → CanIf → PduR → CanTp → Dcm という実装経路と、応答の
 *          Dcm → CanTp → PduR → CanIf → Can → `Can_Hw` という復路を、
 *          全モジュール実体リンク（すべて `-Wl,--wrap` 経由で既定パス
 *          スルー）で 1本のコールチェーンとして検証する「DcmStack」系列の
 *          最初のテスト（2026-09、ユーザーとの相談の上、まず本シナリオで
 *          チェーンの型を確立してから、後続で SID 0x19/0x0A
 *          （マルチフレーム応答）へ拡張する方針）。
 *
 *          あえて本シナリオ（リクエスト3バイト・応答6バイト、共に
 *          Single Frame で収まる）を最初に選んだ理由: CanTp の SF/FF+CF
 *          分割・Flow Control 待ちを絡めずに、まず「Can_Hw 注入 →
 *          Dcm が正しく処理 → Can_Hw から正しい応答が出る」という
 *          チェーンの配線（Can_ConfigType/CanIf_ConfigType/PduR_PBConfigType
 *          のテスト専用ローカル設定、Init() の呼び出し順序）自体を検証する
 *          ため。マルチフレームの取り扱い（Flow Control 注入・
 *          CanTp_MainFunction() の反復呼び出しによる CF 送出）は
 *          別ファイル（SID 0x19/0x0A 用）で扱う。
 *
 *          診断 PDU の CAN ID・PDU ID 番号は本番の `Can_PBCfg.c`/
 *          `CanIf_PBCfg.c`/`PduR_PBCfg.c`（CanId=0x7E0 request / 0x7E8
 *          response、CanTp_Cfg.h の CANTP_RX_SDU_ID=0/CANTP_PDUR_TX_SDU_ID=1）
 *          と同じ値を使うが、他の native_chain フルチェーンテスト
 *          （Bsw_ComStack_Rx_test.cpp 等）と同じ流儀で、本番の
 *          `Can_Config`/`CanIf_Config`/`PduR_Config` は使わず対象PDUのみに
 *          絞ったテスト専用ローカル設定を組む（本番PBCfgの変更から独立させ、
 *          既存の全チェーンテストと作法を揃えるため）。
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
// テスト専用の最小 CanIf/PduR 設定。診断 PDU（CanId=0x7E0 request /
// 0x7E8 response）のみを配線する。CAN ID・PDU ID は本番 PBCfg
// （Can/CanIf/PduR_PBCfg.c、CanTp_Cfg.h）と同じ値（ファイル冒頭コメント参照）。
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

class Bsw_DcmStack_SID19_SF01_ReadDtcCount_Test : public ::testing::Test
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
TEST_F(Bsw_DcmStack_SID19_SF01_ReadDtcCount_Test,
       ReadDtcCount_OK_RequestFromCanHwProducesExpectedSingleFrameResponseOnCanHw)
{
    /* 準備 (Arrange): [0x19, 0x01, statusMask=testFailedのみ] を 0x7E0 の
     * 受信バッファへセットする（SF: 03 19 01 01）。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 3U;
    FakeCanHw_RxData[1] = DCM_SID_READ_DTC_INFO;
    FakeCanHw_RxData[2] = DCM_DTC_SUBFUNC_REPORT_COUNT;
    FakeCanHw_RxData[3] = DEM_STATUS_TEST_FAILED;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act): Os から周期的に呼ばれる Can_MainFunction_Read() を1回
     * 駆動する。Can_Hw 受信 → Can → CanIf → PduR → CanTp → Dcm →
     * （復路）CanTp → PduR → CanIf → Can → Can_Hw 送信、まで同期的に進む。 */
    Can_MainFunction_Read();

    /* 評価 (Assert): 応答が実際に Can_Hw（物理層）まで到達したことを確認する。
     * UDS 応答本体 [0x59, 0x01, DEM_STATUS_AVAILABILITY_MASK, formatId, countHi, countLo]
     * （Bsw_Dcm_ReadDtcInfo_test.cpp の同シナリオと同じ期待値）が
     * Single Frame（PCI=0x06 + 6バイト）として 0x7E8 へ送信されるはず。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendDlc, 8U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x06U);  // SF PCI（UDSペイロード長=6）
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x59U);  // 肯定応答 SID (0x19+0x40)
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_DTC_SUBFUNC_REPORT_COUNT);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DEM_STATUS_AVAILABILITY_MASK);
    EXPECT_EQ(FakeCanHw_LastSendData[6], 0U);  // countL（testFailed の DTC は0件）

    /* CanTp/CanIf/Can の各層が実際にコールチェーンを1回ずつ通過したことも
     * 合わせて確認する（実体リンクされた各モジュールを経由したことの傍証）。 */
    EXPECT_EQ(CallCount_CanTp_RxIndication, 1U);
    EXPECT_EQ(CallCount_CanTp_Transmit, 1U);
}

// ------------------------------------------------------------
// DTC 1件が登録済みの場合、countL が実際の件数を反映することを確認する。
// Dem.c は実体でリンクされ Dem_SetEventStatus() も既定パススルーで
// wrap 済みのため、新規 Wrap 関数は不要（Bsw_Dcm_ReadDtcInfo_test.cpp と
// 同じ手段で DTC 件数を操作できる）。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID19_SF01_ReadDtcCount_Test,
       ReadDtcCount_OK_OneRegisteredDtcMatchingStatusMaskIsReflectedInCountOnCanHw)
{
    /* 準備 (Arrange): DEM_EVENT_ENGINE_OVERHEAT を FAILED（testFailed ビット
     * が立つ）にしてから、[0x19, 0x01, statusMask=testFailedのみ] を 0x7E0 の
     * 受信バッファへセットする（SF: 03 19 01 01）。DEM_DEBOUNCE_LIMIT_ENGINE_OVERHEAT
     * =2（Dem_Cfg.h参照）のため、確定させるには2回呼ぶ必要がある
     * （Bsw_Dcm_ReadDtcInfo_test.cpp の同パターン参照）。 */
    (void)Dem_SetEventStatus(DEM_EVENT_ENGINE_OVERHEAT, DEM_EVENT_STATUS_FAILED);
    (void)Dem_SetEventStatus(DEM_EVENT_ENGINE_OVERHEAT, DEM_EVENT_STATUS_FAILED);

    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 3U;
    FakeCanHw_RxData[1] = DCM_SID_READ_DTC_INFO;
    FakeCanHw_RxData[2] = DCM_DTC_SUBFUNC_REPORT_COUNT;
    FakeCanHw_RxData[3] = DEM_STATUS_TEST_FAILED;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert): countL が 1 件を反映して Can_Hw まで到達すること。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendDlc, 8U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x06U);  // SF PCI（UDSペイロード長=6）
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x59U);  // 肯定応答 SID (0x19+0x40)
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_DTC_SUBFUNC_REPORT_COUNT);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DEM_STATUS_AVAILABILITY_MASK);
    EXPECT_EQ(FakeCanHw_LastSendData[6], 1U);  // countL（DTC 1件）
}

// ------------------------------------------------------------
// statusMask=0xFF は DEM_STATUS_NOT_COMPLETED_SINCE_CLEAR ビットを含むため、
// Dem_Init() 直後（1件も FAILED していない状態）でも全 DEM_EVENT_COUNT 件に
// ヒットする。0x0A reportSupportedDTC が「状態に関わらず全件」返すのとは
// 異なり、0x01/0x02 はあくまで statusMask による絞り込みであることを示す
// シナリオ（Bsw_Dcm_ReadDtcInfo_test.cpp の同シナリオ参照）。UDS要求データ
// （statusMask）と外部データ（Dem の初期状態）の組み合わせで確認できる内容
// のため、チェーンテスト側に追加する。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID19_SF01_ReadDtcCount_Test,
       ReadDtcCount_OK_StatusMask0xFFMatchesNotCompletedSinceClearOnCanHw)
{
    /* 準備 (Arrange): Dem_SetEventStatus() を一切呼ばない（Dem_Init() 直後の
     * 初期状態のまま）で [0x19, 0x01, statusMask=0xFF] を 0x7E0 の受信
     * バッファへセットする（SF: 03 19 01 FF）。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 3U;
    FakeCanHw_RxData[1] = DCM_SID_READ_DTC_INFO;
    FakeCanHw_RxData[2] = DCM_DTC_SUBFUNC_REPORT_COUNT;
    FakeCanHw_RxData[3] = 0xFFU;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert): countL が DEM_EVENT_COUNT 件全てを反映して Can_Hw まで
     * 到達すること。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendDlc, 8U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x06U);  // SF PCI（UDSペイロード長=6）
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x59U);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_DTC_SUBFUNC_REPORT_COUNT);
    EXPECT_EQ(FakeCanHw_LastSendData[6], (uint8)DEM_EVENT_COUNT);  // countL
}

// ------------------------------------------------------------
// NG: statusMask バイトが無い（[0x19, 0x01] のみ、2バイト）リクエストは
// incorrectMessageLength (NRC 0x13) の否定応答になり、それも正常応答と
// 同じ経路で Can_Hw まで届くことを確認する（Bsw_Dcm_ReadDtcInfo_test.cpp の
// 同シナリオと同じ期待値）。OK と同じファイルに同居させる方針
// （[[feedback_test_file_one_scenario_per_file]]、NGは分岐元OKと同居）。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID19_SF01_ReadDtcCount_Test,
       ReadDtcCount_NG_MissingStatusMaskProducesIncorrectMessageLengthResponseOnCanHw)
{
    /* 準備 (Arrange): statusMask を欠落させた [0x19, 0x01] を 0x7E0 の
     * 受信バッファへセットする（SF: 02 19 01）。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 2U;
    FakeCanHw_RxData[1] = DCM_SID_READ_DTC_INFO;
    FakeCanHw_RxData[2] = DCM_DTC_SUBFUNC_REPORT_COUNT;
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
