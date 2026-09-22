/**
 * \file    Bsw_DcmStack_CanTpBusyChain_test.cpp
 * \brief   CanTp が実際にビジー（マルチフレーム送信の途中）な間の Dcm の
 *          振る舞いを、物理層（Can_Hw フェイク）を起点・終点とするフル
 *          コールチェーンで検証するテスト（GoogleTest / CMake native_chain_tests）。
 *
 * \details 特定の SID/サブファンクションに閉じない、Dcm 共通の横断的な
 *          機構が対象のため、Bsw_DcmStack_SID19_DispatchChain_test.cpp と
 *          同じ考え方で専用ファイルへ切り出す（2026-09、ユーザー指摘により
 *          新設）。以下2つの機構を検証する:
 *
 *            1. S3 セッションタイマー（[SWS_Dcm_00141]「CanTp 送信中は
 *               タイマが進まない」）
 *            2. `Dcm_ComIndication()` 自体の新規リクエスト無視
 *               （[SWS_Dcm_00557]、CanTp TX がビジーな間は新規要求を
 *               ディスパッチしない）
 *
 *          旧 Bsw_Dcm_ControlDTCSetting_test.cpp の `S3Timer_OK_*` と
 *          旧 Bsw_Dcm_ReadDtcInfo_test.cpp の `ComIndication_NG/OK_*` は、
 *          いずれも `Wrap_CanTp.h`（`FailFromCallCount_CanTp_IsTxBusy`）で
 *          CanTp のビジー状態を人工的に強制注入していたが、本ファイルでは
 *          SID 0x19/0x0A reportSupportedDTC（応答59バイト、マルチフレーム
 *          必須。Bsw_DcmStack_SID19_SF0A_ReadDtcSupportedChain_test.cpp と
 *          同じ理由で選定）を実際に送信させ、First Frame 送信後・
 *          Consecutive Frame 送信完了前という「CanTp が本当にビジーな状態」
 *          を物理チェーン経由で作り出す。これにより、Dcm 側のビジー判定
 *          ロジックが実体の `CanTp_IsTxBusy()`（`-Wl,--wrap` 経由で既定
 *          パススルー）から正しく通知を受け取れているかまで検証する
 *          （人工的なフォールトインジェクションでは、CanTp 自体の状態機械を
 *          経由しないため、配線そのものの正しさまでは検証できない）。
 *
 *          S3 タイマーのセッション状態確認は UDS 経由の読み出しサービスが
 *          無いため `Dcm_GetSesCtrlType()`（Dcm.h 直接 API）を用いる
 *          （他の DcmStack チェーンテストが `Dem_GetEventUdsStatus()` を
 *          直接呼んで検証するのと同じ考え方）。
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

class Bsw_DcmStack_CanTpBusyChain_Test : public ::testing::Test
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
        EnterExtendedSession();  // S3 タイマーは defaultSession では動かないため前提

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
        WrapCanTp_Reset();  // CallCount_CanTp_Transmit 等を以降の本題シナリオ用に0へ戻す
    }

    /* SID 0x19/0x0A reportSupportedDTC（応答59バイト）を Can_Hw から受信
     * させ、First Frame 送信まで進める。CanTp はこの時点で WAIT_FC
     * （本当にビジー）になる
     * （Bsw_DcmStack_SID19_SF0A_ReadDtcSupportedChain_test.cpp と同じ手順）。 */
    void StartRealMultiFrameTransferAndLeaveCanTpBusy()
    {
        FakeCanHw_Reset();
        FakeCanHw_RxId  = 0x7E0U;
        FakeCanHw_RxDlc = 8U;
        FakeCanHw_RxData[0] = 2U;
        FakeCanHw_RxData[1] = DCM_SID_READ_DTC_INFO;
        FakeCanHw_RxData[2] = DCM_DTC_SUBFUNC_REPORT_SUPPORTED;
        for (uint8 i = 3U; i < 8U; i++)
            FakeCanHw_RxData[i] = 0U;
        FakeCanHw_RxPendingCount = 1U;

        Can_MainFunction_Read();

        ASSERT_EQ(FakeCanHw_SendCount, 1U);
        ASSERT_EQ(FakeCanHw_LastSendData[0], 0x10U);  // First Frame（応答59バイト、SFに収まらない）
        ASSERT_EQ(CanTp_IsTxBusy(), (boolean)1U);      // 本当にビジーであることの前提確認
    }

    /* Flow Control（CTS, BS=0, STmin=0）を注入し、残り8本の Consecutive
     * Frame を CanTp_MainFunction() の反復呼び出しで排出しきる
     * （Bsw_DcmStack_SID19_SF0A_ReadDtcSupportedChain_test.cpp と同じ手順。
     * CanTp を本当に IDLE へ戻すために使う）。 */
    void FinishRealMultiFrameTransfer()
    {
        FakeCanHw_RxId  = 0x7E0U;
        FakeCanHw_RxDlc = 8U;
        FakeCanHw_RxData[0] = 0x30U;
        FakeCanHw_RxData[1] = 0x00U;
        FakeCanHw_RxData[2] = 0x00U;
        for (uint8 i = 3U; i < 8U; i++)
            FakeCanHw_RxData[i] = 0U;
        FakeCanHw_RxPendingCount = 1U;
        Can_MainFunction_Read();

        for (uint8 cf = 0U; cf < 8U; cf++)
            CanTp_MainFunction();

        ASSERT_EQ(CanTp_IsTxBusy(), (boolean)0U);  // 本当に IDLE へ戻ったことの確認
    }

    Can_ConfigType canConfig;
};

// ============================================================
// S3 セッションタイマー（[SWS_Dcm_00141]）
// ============================================================

TEST_F(Bsw_DcmStack_CanTpBusyChain_Test,
       S3Timer_OK_DoesNotTimeOutWhileCanTpReallyBusyWithInFlightMultiFrameTransfer)
{
    /* 準備 (Arrange): SID 0x19/0x0A を送らせ、First Frame 送信直後（CanTp
     * が本当にビジー）の状態を作る。 */
    StartRealMultiFrameTransferAndLeaveCanTpBusy();

    /* 実行 (Act): CanTp が本当にビジーなまま S3 タイムアウト相当の時間が
     * 経過しても、[SWS_Dcm_00141] によりタイマは進まないはず。 */
    FakeMillis_Value += DCM_S3_TIMEOUT_MS + 1UL;
    Dcm_MainFunction();

    /* 評価 (Assert): defaultSession へ落ちていないこと（実体の
     * CanTp_IsTxBusy() から正しく通知を受け取れている傍証）。 */
    Dcm_SesCtrlType session = 0U;
    ASSERT_EQ(Dcm_GetSesCtrlType(&session), E_OK);
    EXPECT_EQ(session, DCM_SESSION_EXTENDED);

    /* 後始末: マルチフレーム送信を完了させておく（次テストへ影響しないよう
     * SetUp() 起点でリセットされるため必須ではないが、CanTp を IDLE へ戻して
     * おく方が状態として自然）。 */
    FinishRealMultiFrameTransfer();
}

TEST_F(Bsw_DcmStack_CanTpBusyChain_Test,
       S3Timer_OK_TimesOutNormallyOnceCanTpReallyIdleAgain)
{
    /* 準備 (Arrange): ビジー中はタイムアウトしないことを確認しつつ
     * （前のテストと同じ前提）、マルチフレーム送信を最後まで完了させて
     * CanTp を本当に IDLE へ戻す。 */
    StartRealMultiFrameTransferAndLeaveCanTpBusy();
    FakeMillis_Value += DCM_S3_TIMEOUT_MS + 1UL;
    Dcm_MainFunction();
    FinishRealMultiFrameTransfer();

    /* 実行 (Act): CanTp が本当に IDLE へ戻った後、改めて S3 タイムアウト分の
     * 時間を経過させる。 */
    Dcm_MainFunction();
    FakeMillis_Value += DCM_S3_TIMEOUT_MS + 1UL;
    Dcm_MainFunction();

    /* 評価 (Assert): 通常通り defaultSession へ落ちていること。 */
    Dcm_SesCtrlType session = 0U;
    ASSERT_EQ(Dcm_GetSesCtrlType(&session), E_OK);
    EXPECT_EQ(session, DCM_SESSION_DEFAULT);
}

// ============================================================
// Dcm_ComIndication() 自体の新規リクエスト無視（[SWS_Dcm_00557]）
// ============================================================

TEST_F(Bsw_DcmStack_CanTpBusyChain_Test,
       ComIndication_NG_IgnoresNewRequestWhileCanTpReallyBusyWithInFlightMultiFrameTransfer)
{
    /* 準備 (Arrange): SID 0x19/0x0A を送らせ、First Frame 送信直後（CanTp
     * が本当にビジー）の状態を作る。 */
    StartRealMultiFrameTransferAndLeaveCanTpBusy();
    ASSERT_EQ(CallCount_CanTp_Transmit, 1U);

    /* 実行 (Act): CanTp が本当にビジーなまま、TesterPresent（副作用の無い
     * 単純なSID）[0x3E, 0x00] を追加で受信させる。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 2U;
    FakeCanHw_RxData[1] = DCM_SID_TESTER_PRESENT;
    FakeCanHw_RxData[2] = 0x00U;
    for (uint8 i = 3U; i < 8U; i++)
        FakeCanHw_RxData[i] = 0U;
    FakeCanHw_RxPendingCount = 1U;
    Can_MainFunction_Read();

    /* 評価 (Assert): [SWS_Dcm_00557] ディスパッチ自体が行われず、新たな
     * 送信も一切発生しないこと（CanTp_Transmit() 呼び出し回数・Can_Hw への
     * 送信回数とも進行中の FF 送信1回のまま変化しない）。 */
    EXPECT_EQ(CallCount_CanTp_Transmit, 1U);
    EXPECT_EQ(FakeCanHw_SendCount, 1U);

    /* 後始末: マルチフレーム送信を完了させておく。 */
    FinishRealMultiFrameTransfer();
}

TEST_F(Bsw_DcmStack_CanTpBusyChain_Test,
       ComIndication_OK_ProcessesRequestOnceCanTpReallyIdleAgainOnCanHw)
{
    /* 準備 (Arrange): ビジー中に届いた要求は無視されることを確認した後、
     * マルチフレーム送信を完了させて CanTp を本当に IDLE へ戻す。 */
    StartRealMultiFrameTransferAndLeaveCanTpBusy();

    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 2U;
    FakeCanHw_RxData[1] = DCM_SID_TESTER_PRESENT;
    FakeCanHw_RxData[2] = 0x00U;
    for (uint8 i = 3U; i < 8U; i++)
        FakeCanHw_RxData[i] = 0U;
    FakeCanHw_RxPendingCount = 1U;
    Can_MainFunction_Read();
    ASSERT_EQ(CallCount_CanTp_Transmit, 1U) << "must be ignored while busy (test precondition)";

    FinishRealMultiFrameTransfer();
    FakeCanHw_Reset();  // 後続の送信結果を素直に見るためリセット

    /* 実行 (Act): CanTp TX がアイドルへ戻った後、同じ要求を再送する。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 2U;
    FakeCanHw_RxData[1] = DCM_SID_TESTER_PRESENT;
    FakeCanHw_RxData[2] = 0x00U;
    for (uint8 i = 3U; i < 8U; i++)
        FakeCanHw_RxData[i] = 0U;
    FakeCanHw_RxPendingCount = 1U;
    Can_MainFunction_Read();

    /* 評価 (Assert): 通常通り正応答 [0x7E, 0x00] が Can_Hw まで到達すること。 */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x02U);  // SF PCI（UDSペイロード長=2）
    EXPECT_EQ(FakeCanHw_LastSendData[1], (uint8)(DCM_SID_TESTER_PRESENT + 0x40U));
    EXPECT_EQ(FakeCanHw_LastSendData[2], 0x00U);
}

}  // namespace
