/**
 * \file    Bsw_DcmStack_SID85_ControlDTCSettingChain_test.cpp
 * \brief   UDS SID 0x85 ControlDTCSetting の、物理層（Can_Hw フェイク）を
 *          起点・終点とするフルコールチェーンテスト（GoogleTest /
 *          CMake native_chain_tests）。
 *
 * \details Bsw_DcmStack_SID19_SF01_ReadDtcCountChain_test.cpp 系列で確立した
 *          チェーンの型を適用する。リクエスト（2バイト）・応答（正応答2
 *          バイト／否定応答3バイト）とも Single Frame に収まるため、
 *          CanTp のマルチフレーム分割は関与しない。
 *
 *          0x85 は extendedSession 限定のため、
 *          Bsw_DcmStack_SID31_RoutineControlChain_test.cpp と同じ
 *          `EnterExtendedSession()` ヘルパーで事前にセッションを遷移させる。
 *          `SendControlDTCSetting(subFunc)` は [0x85, subFunc] という
 *          頻出パターンの送信専用ヘルパー（旧 Bsw_Dcm_ControlDTCSetting_test.cpp
 *          の同名ヘルパーをチェーン化したもの。subFunc は毎回明示的に渡す
 *          引数のため、どのリクエストが送られたか隠蔽されない）。DTC 記録の
 *          有効/無効は `Dem_GetEventUdsStatus()` を直接呼んで確認する
 *          （UDS 経由の読み出しサービスは対象外、旧ファイルと同じ手法）。
 *
 *          旧 Bsw_Dcm_ControlDTCSetting_test.cpp のうち、UDS要求データと
 *          外部データ（Dem状態・セッション状態）の組み合わせで確認できる
 *          11シナリオをここへ移植する。`S3Timer_OK_DoesNotTimeOutWhileCanTpTxBusy`/
 *          `S3Timer_OK_TimesOutNormallyOnceCanTpTxIdleAgain` の2件は
 *          `Wrap_CanTp.h` で CanTp のビジー状態を強制注入する境界フォールト
 *          インジェクションが本題であり、物理チェーンとは無関係なため、
 *          引き続き旧ファイル（Dcmレベルの境界テスト）に残す。
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

class Bsw_DcmStack_SID85_ControlDTCSettingChain_Test : public ::testing::Test
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

    /* [0x10, 0x03] extendedDiagnosticSession を実際に Can_Hw から受信させ、
     * 物理応答が正応答であることまで確認する前提確立専用のヘルパー
     * （Bsw_DcmStack_SID31_RoutineControlChain_test.cpp と同じ理由）。 */
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
    }

    /* [0x85, subFunc] を Can_Hw から受信させ、チェーン全体を駆動する
     * （頻出パターンのため専用ヘルパー化。subFunc は呼び出し側が明示的に
     * 渡すため隠蔽されない。ファイル冒頭コメント参照）。 */
    void SendControlDTCSetting(uint8 subFunc)
    {
        FakeCanHw_Reset();
        FakeCanHw_RxId  = 0x7E0U;
        FakeCanHw_RxDlc = 8U;
        FakeCanHw_RxData[0] = 2U;
        FakeCanHw_RxData[1] = DCM_SID_CONTROL_DTC_SETTING;
        FakeCanHw_RxData[2] = subFunc;
        for (uint8 i = 3U; i < 8U; i++)
            FakeCanHw_RxData[i] = 0U;
        FakeCanHw_RxPendingCount = 1U;

        Can_MainFunction_Read();
    }

    /* DEM_EVENT_CAN_BUSOFF (DEM_DEBOUNCE_LIMIT_CAN_BUSOFF=1、1回の報告で
     * 即確定) を FAILED 報告する。DTC 記録が有効なら testFailed/confirmedDTC
     * ビットが立つはず。UDS 読み出しサービスを経由せず Dem を直接呼ぶ
     * （旧ファイルと同じ手法。ファイル冒頭コメント参照）。 */
    static void ReportBusOffFailed()
    {
        (void)Dem_SetEventStatus(DEM_EVENT_CAN_BUSOFF, DEM_EVENT_STATUS_FAILED);
    }

    static uint8 BusOffStatus()
    {
        Dem_UdsStatusByteType status = 0U;
        (void)Dem_GetEventUdsStatus(DEM_EVENT_CAN_BUSOFF, &status);
        return status;
    }

    Can_ConfigType canConfig;
};

// ------------------------------------------------------------
// 正常系: on/off の受理と応答
// ------------------------------------------------------------

TEST_F(Bsw_DcmStack_SID85_ControlDTCSettingChain_Test,
       ControlDTCSetting_OK_OffIsAcceptedWithPositiveResponseOnCanHw)
{
    EnterExtendedSession();

    SendControlDTCSetting(DCM_DTCSETTING_OFF);

    /* 評価 (Assert): [0xC5, 0x02] が Can_Hw まで到達すること。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x02U);  // SF PCI（UDSペイロード長=2）
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0xC5U);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_DTCSETTING_OFF);
}

TEST_F(Bsw_DcmStack_SID85_ControlDTCSettingChain_Test,
       ControlDTCSetting_OK_OnIsAcceptedWithPositiveResponseOnCanHw)
{
    EnterExtendedSession();

    SendControlDTCSetting(DCM_DTCSETTING_ON);

    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x02U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0xC5U);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_DTCSETTING_ON);
}

// ------------------------------------------------------------
// DTC 記録の有効/無効が Dem へ実際に反映されること（本機能の核心）
// ------------------------------------------------------------

TEST_F(Bsw_DcmStack_SID85_ControlDTCSettingChain_Test,
       ControlDTCSetting_OK_OffSuppressesDtcRecordingUntilOnOnCanHw)
{
    EnterExtendedSession();

    /* 実行 (Act): off にしてから、通常なら即確定するはずの FAILED を報告する */
    SendControlDTCSetting(DCM_DTCSETTING_OFF);
    ReportBusOffFailed();

    /* 評価 (Assert): 記録無効化中のため testFailed/confirmedDTC ビットとも
     * 立っていない（DEM_STATUS_NOT_COMPLETED_SINCE_CLEAR 等の初期ビットのみ）。 */
    EXPECT_EQ(BusOffStatus() & DEM_STATUS_TEST_FAILED, 0U);
    EXPECT_EQ(BusOffStatus() & DEM_STATUS_CONFIRMED, 0U);

    /* 実行 (Act): on に戻してから同じ報告をする */
    SendControlDTCSetting(DCM_DTCSETTING_ON);
    ReportBusOffFailed();

    /* 評価 (Assert): 再有効化後は通常どおり即確定する
     * (DEM_DEBOUNCE_LIMIT_CAN_BUSOFF=1)。 */
    EXPECT_NE(BusOffStatus() & DEM_STATUS_TEST_FAILED, 0U);
    EXPECT_NE(BusOffStatus() & DEM_STATUS_CONFIRMED, 0U);
}

// ------------------------------------------------------------
// defaultSession への遷移で自動的に on へ復帰すること（SWS_Dcm_00751）
// ------------------------------------------------------------

TEST_F(Bsw_DcmStack_SID85_ControlDTCSettingChain_Test,
       ControlDTCSetting_OK_AutoReEnablesOnExplicitDefaultSessionRequestOnCanHw)
{
    EnterExtendedSession();
    SendControlDTCSetting(DCM_DTCSETTING_OFF);

    /* 実行 (Act): [0x10, 0x01] defaultSession へ明示的に戻る */
    FakeCanHw_Reset();
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 2U;
    FakeCanHw_RxData[1] = DCM_SID_SESSION_CTRL;
    FakeCanHw_RxData[2] = DCM_SESSION_DEFAULT;
    FakeCanHw_RxPendingCount = 1U;
    Can_MainFunction_Read();
    ASSERT_EQ(FakeCanHw_LastSendData[1], 0x50U);  // 正応答確認

    /* 評価 (Assert): 明示的に on を送っていないにも関わらず、defaultSession
     * への遷移だけで自動的に記録が再開される。 */
    ReportBusOffFailed();
    EXPECT_NE(BusOffStatus() & DEM_STATUS_TEST_FAILED, 0U);
}

TEST_F(Bsw_DcmStack_SID85_ControlDTCSettingChain_Test,
       ControlDTCSetting_OK_AutoReEnablesOnS3TimeoutOnCanHw)
{
    EnterExtendedSession();
    SendControlDTCSetting(DCM_DTCSETTING_OFF);

    /* 実行 (Act): S3 タイムアウトで defaultSession へ自動遷移させる
     * (明示的な 0x10 要求を送らない経路。Dcm_MainFunction() は Os から
     * 周期的に呼ばれる関数であり、CAN イベントではないため Can_Hw 経由に
     * しない)。 */
    FakeMillis_Value += DCM_S3_TIMEOUT_MS + 1UL;
    Dcm_MainFunction();

    /* 評価 (Assert): S3 タイムアウト経由でも自動的に記録が再開される。 */
    ReportBusOffFailed();
    EXPECT_NE(BusOffStatus() & DEM_STATUS_TEST_FAILED, 0U);
}

TEST_F(Bsw_DcmStack_SID85_ControlDTCSettingChain_Test,
       ControlDTCSetting_OK_AutoReEnablesAfterEcuResetOnCanHw)
{
    EnterExtendedSession();
    SendControlDTCSetting(DCM_DTCSETTING_OFF);

    /* 実行 (Act): [0x11, 0x01] hardReset（本実装は実際のリセットは行わず
     * セッションを defaultSession へ戻すのみ）。 */
    FakeCanHw_Reset();
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 2U;
    FakeCanHw_RxData[1] = DCM_SID_ECU_RESET;
    FakeCanHw_RxData[2] = 0x01U;
    FakeCanHw_RxPendingCount = 1U;
    Can_MainFunction_Read();
    ASSERT_EQ(FakeCanHw_LastSendData[1], 0x51U);  // 正応答確認

    /* 評価 (Assert): ECUReset 経由でも自動的に記録が再開される。 */
    ReportBusOffFailed();
    EXPECT_NE(BusOffStatus() & DEM_STATUS_TEST_FAILED, 0U);
}

TEST_F(Bsw_DcmStack_SID85_ControlDTCSettingChain_Test,
       ControlDTCSetting_NG_DoesNotReEnableWhenNotDisabledOnCanHw)
{
    /* 準備 (Arrange): 一度も off にしていない状態で defaultSession へ戻る。 */
    EnterExtendedSession();
    FakeCanHw_Reset();
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 2U;
    FakeCanHw_RxData[1] = DCM_SID_SESSION_CTRL;
    FakeCanHw_RxData[2] = DCM_SESSION_DEFAULT;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert): 記録が有効なままであること（冗長呼び出しでないことの
     * 間接確認）。 */
    ReportBusOffFailed();
    EXPECT_NE(BusOffStatus() & DEM_STATUS_TEST_FAILED, 0U);
}

// ------------------------------------------------------------
// 異常系
// ------------------------------------------------------------

TEST_F(Bsw_DcmStack_SID85_ControlDTCSettingChain_Test,
       ControlDTCSetting_NG_DefaultSessionProducesServiceNotSupportedInSessionResponseOnCanHw)
{
    /* 準備 (Arrange): Dcm_Init() 直後は defaultSession のまま
     * （EnterExtendedSession() を呼ばない）。 */

    /* 実行 (Act) */
    SendControlDTCSetting(DCM_DTCSETTING_OFF);

    /* 評価 (Assert): [0x7F, 0x85, 0x7F serviceNotSupportedInActiveSession]
     * が Can_Hw まで到達すること。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x03U);  // SF PCI（UDSペイロード長=3）
    EXPECT_EQ(FakeCanHw_LastSendData[1], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_SID_CONTROL_DTC_SETTING);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DCM_NRC_SERVICE_NOT_SUPPORTED_IN_SESSION);

    /* 記録も無効化されていないことを確認する（拒否された要求が副作用を
     * 持たないこと）。 */
    ReportBusOffFailed();
    EXPECT_NE(BusOffStatus() & DEM_STATUS_TEST_FAILED, 0U);
}

TEST_F(Bsw_DcmStack_SID85_ControlDTCSettingChain_Test,
       ControlDTCSetting_NG_UnsupportedSubFuncProducesSubFuncNotSupportedResponseOnCanHw)
{
    EnterExtendedSession();

    /* 実行 (Act): 0x01/0x02 以外のサブ機能 */
    SendControlDTCSetting(0x03U);

    /* 評価 (Assert): [0x7F, 0x85, 0x12 subFunctionNotSupported] が Can_Hw
     * まで到達すること。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x03U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DCM_NRC_SUB_FUNC_NOT_SUPPORTED);
}

TEST_F(Bsw_DcmStack_SID85_ControlDTCSettingChain_Test,
       ControlDTCSetting_NG_ExtraOptionRecordProducesIncorrectMessageLengthResponseOnCanHw)
{
    EnterExtendedSession();

    /* 準備 (Arrange): [SWS_Dcm_01399] 相当。DTCSettingControlOptionRecord
     * (0xFFFFFF 以外) を付けた5バイト要求を 0x7E0 の受信バッファへセットする
     * （SF: 05 85 02 12 34 56）。 */
    FakeCanHw_Reset();
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 5U;
    FakeCanHw_RxData[1] = DCM_SID_CONTROL_DTC_SETTING;
    FakeCanHw_RxData[2] = DCM_DTCSETTING_OFF;
    FakeCanHw_RxData[3] = 0x12U;
    FakeCanHw_RxData[4] = 0x34U;
    FakeCanHw_RxData[5] = 0x56U;
    FakeCanHw_RxData[6] = 0U;
    FakeCanHw_RxData[7] = 0U;
    FakeCanHw_RxPendingCount = 1U;

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert): [0x7F, 0x85, 0x13 incorrectMessageLength] が Can_Hw
     * まで到達すること。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x03U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DCM_NRC_INCORRECT_MESSAGE_LENGTH);
}

TEST_F(Bsw_DcmStack_SID85_ControlDTCSettingChain_Test,
       ControlDTCSetting_NG_UnsupportedSubFuncWithExtraBytePrefersSubFuncNrcOnCanHw)
{
    EnterExtendedSession();

    /* [SWS_Dcm_00273]/[SWS_Dcm_00696]: サブ機能サポート確認は
     * [SWS_Dcm_01399] 代用の optionRecord 超過チェックより先に行う処理順序
     * （2026-09 是正）。subFunc(uds[1])が不正かつ optionRecord も付いている
     * (udsLen=3>2)場合でも、NRC 0x13(incorrectMessageLength)ではなく
     * 0x12(subFunctionNotSupported)を返すべき。 */
    FakeCanHw_Reset();
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 3U;
    FakeCanHw_RxData[1] = DCM_SID_CONTROL_DTC_SETTING;
    FakeCanHw_RxData[2] = 0xFFU;
    FakeCanHw_RxData[3] = 0x00U;
    for (uint8 i = 4U; i < 8U; i++)
        FakeCanHw_RxData[i] = 0U;
    FakeCanHw_RxPendingCount = 1U;

    Can_MainFunction_Read();

    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x03U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(FakeCanHw_LastSendData[3], DCM_NRC_SUB_FUNC_NOT_SUPPORTED);
}

}  // namespace
