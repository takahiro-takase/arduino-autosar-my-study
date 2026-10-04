/**
 * \file    Bsw_DcmStack_ActiveDiagnosticWake_test.cpp
 * \brief   診断要求ごとの ComM_DCM_ActiveDiagnostic() 通知
 *          （[SWS_Dcm_01376]/[SWS_Dcm_01374]/[SWS_Dcm_01377]）のフルコール
 *          チェーンテスト（GoogleTest / CMake native_chain_tests）。
 *
 * \details Dcm.c/CanTp.c/PduR.c/CanIf.c/Can.c に加え、ComM.c/CanSM.c/CanNm.c/Nm.c の
 *          実体をリンクし、物理層（Can_Hw フェイク）を起点・終点として検証する。
 *
 *          背景: 以前の Dcm は拡張セッションに入るときだけ ComM へ診断アクティブを
 *          通知していたため、CanNm の協調スリープ中（Prepare Bus-Sleep Mode、
 *          チャネルは SILENT_COM で CanIf が TX を拒否）にデフォルトセッションの
 *          診断要求が届くと、要求は処理されるが応答が CanIf で拒否されて
 *          無言で失われていた（実機で 0x19/02 が一度だけ無応答になった事象）。
 *
 *          1. Dcm_ComIndication_OK_DefaultSessionRequestDuringSilentComWakesChannelAndResponds:
 *             SILENT_COM 中の TesterPresent 要求で協調スリープが取り消されて
 *             FULL_COM へ復帰し、応答が Can_Hw まで到達すること。
 *          2. Dcm_ComIndication_OK_DefaultSessionRequestNotifiesActiveThenInactive:
 *             通常（FULL_COM）の要求では ActiveDiagnostic/InactiveDiagnostic が
 *             1 回ずつ呼ばれ、チャネルモードを乱さないこと。
 *          3. Dcm_ComIndication_OK_ExtendedSessionKeepsDiagnosticActive:
 *             拡張セッションへ入った後は InactiveDiagnostic が呼ばれず、
 *             アクティブ状態が維持されること。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Can.h"
#include "Can_Hw.h"
#include "CanIf.h"
#include "CanSM.h"
#include "CanNm.h"
#include "Nm.h"
#include "ComM.h"
#include "PduR.h"
#include "CanTp.h"
#include "CanTp_Cfg.h"
#include "Dcm.h"
#include "Dcm_Cfg.h"
#include "Dem.h"
#include "Fake_Can_Hw.h"
#include "Fake_Det_Hw.h"
#include "Fake_Millis.h"
#include "Fake_Bsw_EcuM.h"
#include "Wrap_Can.h"
#include "Wrap_CanIf.h"
#include "Wrap_PduR.h"
#include "Wrap_CanTp.h"
#include "Wrap_ComM.h"
#include "Wrap_BswM.h"
#include "Wrap_Dem.h"
}

/* ======================================================================
 * Definitions
 * ====================================================================== */

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/* ======================================================================
 * Global Variables
 * ====================================================================== */

namespace
{

// -----------------------------------------------------------------------
// テスト専用の最小 CanIf/PduR 設定（診断 0x7E0/0x7E8 と CanNm の TX PDU のみ）。
// CANNM_CANIF_TX_PDU_ID（CanNm_Cfg.h）は添字 2 のため、添字 0 = 診断応答、
// 添字 1 = ダミー、添字 2 = NM フレームとする。
// -----------------------------------------------------------------------

const CanIf_RxPduConfigType kTestCanIfRxPdu = {
    /* CanId */                0x7E0U,
    /* Hrh */                  0U,
    /* UpperLayerRxPduId */    0U,
    /* Dlc */                  8U,
    /* RxIndicationFct */      PduR_CanIfRxIndication,
    /* ReadRxPduDataEnabled */ 0U
};

const CanIf_TxPduConfigType kTestCanIfTxPdus[3] = {
    { /* UpperLayerTxPduId */ 0U,
      /* CanId */             0x7E8U,
      /* Dlc */               8U,
      /* Hth */               0U,
      /* TxConfirmFct */      PduR_CanIfTxConfirmation },
    { 0U, 0U, 0U, 0U, NULL },
    { /* UpperLayerTxPduId */ CANNM_CANIF_TX_PDU_ID,
      /* CanId */             0x400U,
      /* Dlc */               CANNM_DLC,
      /* Hth */               0U,
      /* TxConfirmFct */      CanNm_TxConfirmation }
};

const CanIf_ConfigType kTestCanIfConfig = {
    /* TxPduConfig */ kTestCanIfTxPdus,
    /* TxPduCount */  3U,
    /* RxPduConfig */ &kTestCanIfRxPdu,
    /* RxPduCount */  1U
};

const PduR_RxDestType kTestPduRRxDest = {
    /* Module */    PDUR_MODULE_CANTP,
    /* DestPduId */ CANTP_RX_SDU_ID,
    /* RxIndFct */  CanTp_RxIndication
};

const PduR_RxRoutingPathType kTestPduRRxPath = {
    /* SrcPduId */  0U,
    /* Dests */     &kTestPduRRxDest,
    /* DestCount */ 1U
};

const PduR_TxRoutingPathType kTestPduRTxPath = {
    /* SrcPduId */             CANTP_PDUR_TX_SDU_ID,
    /* CanIfTxPduId */         0U,
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

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

class Bsw_DcmStack_ActiveDiagnosticWake_Test : public ::testing::Test
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
        WrapComM_Reset();
        WrapBswM_Reset();
        WrapDem_Reset();
        FakeEcuM_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制

        canConfig.filter.filterId = 0x0000U;
        canConfig.filter.mask     = 0x0000U;
        canConfig.csPin           = 10U;
        canConfig.intPin          = 2U;
        canConfig.baudrate        = 500000U;
        canConfig.crystalFreq     = CAN_CRYSTAL_16MHZ;

        Can_Init(&canConfig);
        CanIf_Init(&kTestCanIfConfig);
        PduR_Init(&kTestPduRConfig);
        CanSM_Init(NULL);
        ComM_Init(NULL);
        ComM_CommunicationAllowed(COMM_CHANNEL_0, TRUE);  // 実 EcuM_Init() と同じく起動時に許可
        CanNm_Init(NULL);
        Nm_Init(NULL);
        CanTp_Init(NULL);
        Dem_Init(NULL);
        Dcm_Init(NULL);

        /* FULL_COM を確立し、CanNm が Repeat Message State を抜けて Normal Operation State に
         * なるまで進める（ComM_RequestComMode() は CanSM 経由でコントローラを起動する）。 */
        ASSERT_EQ(ComM_RequestComMode(COMM_USER_0, COMM_FULL_COMMUNICATION), E_OK);
        FakeMillis_Value += CANNM_REPEAT_MESSAGE_MS + 100UL;
        CanNm_MainFunction();

        FakeCanHw_Reset();
        WrapComM_Reset();  // 通知回数は各テストの Act 区間だけを対象にする
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;  // DeInit() のログを抑制
        CanNm_DeInit();
        ComM_DeInit();
        CanSM_DeInit();
        CanIf_DeInit();
    }

    /** NO_COM を要求し、CanNm が Prepare Bus-Sleep Mode（チャネルは SILENT_COM）へ
     *  到達するまで進める。 */
    void DriveToSilentCom()
    {
        ASSERT_EQ(ComM_RequestComMode(COMM_USER_0, COMM_NO_COMMUNICATION), E_OK);
        for (int i = 0; i < 15; i++)
        {
            CanNm_StateType state;
            CanNm_ModeType  mode;
            ASSERT_EQ(CanNm_GetState(CANNM_MAIN_NETWORK_HANDLE, &state, &mode), E_OK);
            if (state == CANNM_STATE_PREPARE_BUS_SLEEP)
                break;
            FakeMillis_Value += CANNM_CYCLE_MS;
            CanNm_MainFunction();
        }
        ComM_ModeType comMode = COMM_FULL_COMMUNICATION;
        ASSERT_EQ(ComM_GetCurrentComMode(COMM_USER_0, &comMode), E_OK);
        ASSERT_EQ(comMode, static_cast<ComM_ModeType>(COMM_SILENT_COMMUNICATION));
    }

    /** 0x7E0 へ SF の診断要求（UDS ペイロード 2 バイト）を受信させ、Can_MainFunction_Read() を実行する。 */
    static void ReceiveRequest(uint8 sid, uint8 sub)
    {
        FakeCanHw_RxId  = 0x7E0U;
        FakeCanHw_RxDlc = 8U;
        FakeCanHw_RxData[0] = 2U;
        FakeCanHw_RxData[1] = sid;
        FakeCanHw_RxData[2] = sub;
        for (uint8 i = 3U; i < 8U; i++)
            FakeCanHw_RxData[i] = 0U;
        FakeCanHw_RxPendingCount = 1U;
        Can_MainFunction_Read();
    }

    Can_ConfigType canConfig;
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

// ------------------------------------------------------------
// OK: SILENT_COM（CanNm の Prepare Bus-Sleep Mode 中）にデフォルトセッションの
// TesterPresent が届くと、ComM が協調スリープを取り消してチャネルを FULL_COM へ
// 戻し、応答 [0x7E, 0x00] が Can_Hw まで到達する（以前は応答が無言で失われた）。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_ActiveDiagnosticWake_Test,
       OK_DefaultSessionRequestDuringSilentComWakesChannelAndResponds)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    DriveToSilentCom();
    FakeCanHw_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    // [0x3E, 0x00] TesterPresent（デフォルトセッションで許可）。
    ReceiveRequest(DCM_SID_TESTER_PRESENT, 0x00U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    // チャネルが FULL_COM へ復帰していること。
    ComM_ModeType comMode = COMM_NO_COMMUNICATION;
    ASSERT_EQ(ComM_GetCurrentComMode(COMM_USER_0, &comMode), E_OK);
    EXPECT_EQ(comMode, static_cast<ComM_ModeType>(COMM_FULL_COMMUNICATION));

    /* 復帰時の NM フレーム（0x400）に続き、診断応答（0x7E8）が最後に送信される。 */
    ASSERT_EQ(FakeCanHw_SendCount, 2U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x02U);  // SF PCI（UDSペイロード長=2）
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x7EU);  // TesterPresent 正応答 SID
    EXPECT_EQ(FakeCanHw_LastSendData[2], 0x00U);
}

// ------------------------------------------------------------
// OK: 通常（FULL_COM）のデフォルトセッション要求は、ActiveDiagnostic/
// InactiveDiagnostic を 1 回ずつ呼び、チャネルモードを乱さない。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_ActiveDiagnosticWake_Test,
       OK_DefaultSessionRequestNotifiesActiveThenInactive)
{
    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ReceiveRequest(DCM_SID_TESTER_PRESENT, 0x00U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_ComM_DCM_ActiveDiagnostic, 1U);
    EXPECT_EQ(CallCount_ComM_DCM_InactiveDiagnostic, 1U);

    ComM_ModeType comMode = COMM_NO_COMMUNICATION;
    ASSERT_EQ(ComM_GetCurrentComMode(COMM_USER_0, &comMode), E_OK);
    EXPECT_EQ(comMode, static_cast<ComM_ModeType>(COMM_FULL_COMMUNICATION));

    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x7EU);
}

// ------------------------------------------------------------
// OK: 拡張セッションへ入る要求では、要求受理時と入った時にアクティブが通知され、
// 処理後に InactiveDiagnostic は呼ばれない（拡張セッションの間はアクティブを維持する）。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_ActiveDiagnosticWake_Test,
       OK_ExtendedSessionKeepsDiagnosticActive)
{
    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    // [0x10, 0x03] extendedDiagnosticSession。
    ReceiveRequest(DCM_SID_SESSION_CTRL, DCM_SESSION_EXTENDED);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_GE(CallCount_ComM_DCM_ActiveDiagnostic, 2U);
    EXPECT_EQ(CallCount_ComM_DCM_InactiveDiagnostic, 0U);

    Dcm_SesCtrlType session = 0U;
    ASSERT_EQ(Dcm_GetSesCtrlType(&session), E_OK);
    EXPECT_EQ(session, DCM_SESSION_EXTENDED);
}

}  // namespace
