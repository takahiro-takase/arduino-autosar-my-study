/**
 * \file    DcmStack_ComMModeEntered_DeferredTimeout_test.cpp
 * \brief   FULL_COM を待つ時間を過ぎた要求は、実行せずに捨てることの、フルコールチェーンテスト。
 *
 * \details [SWS_Dcm_01142]: 待ち時間は、DCM_FULLCOM_WAIT_MS まで。過ぎた要求は、応答を送れないまま実行しないよう、
 *          実行せずに捨てる（セッションの変更などが起きない）。後から FULL_COM になっても、実行されない。
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
#include "ComM.h"
#include "PduR.h"
#include "CanTp.h"
#include "Fake_PduR_PBCfg.h"
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
#include "Wrap_ComM.h"
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
// テスト専用の最小 CanIf/PduR 設定（DcmStack_SID19_SF01_ReadDtcCount_test.cpp
// と同一。ファイル冒頭コメント参照）。
// -----------------------------------------------------------------------

const CanIf_RxPduConfigType kTestCanIfRxPdu = {
    /* CanId */                0x7E0U,
    /* Hrh */                  0U,  /* Can_MainFunction_Read() が構築する Mailbox は常に Hoh=0 */
    /* UpperLayerRxPduId */    CANTP_RX_NPDU_ID,
    /* Dlc */                  8U,
    /* RxIndicationFct */      CanTp_RxIndication,
    /* ReadRxPduDataEnabled */ 0U
};

// CanTp は、診断応答を CanIf_Transmit(CANTP_CANIF_TX_PDU_ID, …) で直接送る（PduR を経由しない）。
// そのインデックスに診断応答の PDU を置く（手前の PDU は未使用）。
static_assert(CANTP_CANIF_TX_PDU_ID == 1U, "診断応答の PDU を、CanIf の TX テーブルの添字 1 に置く前提");

const CanIf_TxPduConfigType kTestCanIfTxPdus[2] = {
    { 0U, 0U, 0U, 0U, NULL },
    { /* UpperLayerTxPduId */ CANTP_TX_NPDU_ID,
      /* CanId */             0x7E8U,
      /* Dlc */               8U,
      /* Hth */               0U,
      /* TxConfirmFct */      CanTp_TxConfirmation }
};

const CanIf_ConfigType kTestCanIfConfig = {
    /* TxPduConfig */ kTestCanIfTxPdus,
    /* TxPduCount */  2U,
    /* RxPduConfig */ &kTestCanIfRxPdu,
    /* RxPduCount */  1U
};

// 診断の経路（CanTp ↔ Dcm）は、PduR の TP のルーティングで運ぶ。IF のルーティング（RxPaths/TxPaths）は持たない
// （CanIf と CanTp は、PduR を経由せず直接やり取りする）。
const PduR_PBConfigType kTestPduRConfig = {
    /* RxPaths */     NULL,
    /* RxPathCount */ 0U,
    /* TxPaths */     NULL,
    /* TxPathCount */ 0U,
    /* PBConfigId */ 0U,
    /* RoutingPathGroups */ NULL,
    /* RoutingPathGroupCount */ 0U,
    /* TpRxPaths */ Fake_PduR_TpRxPaths,
    /* TpRxPathCount */ FAKE_PDUR_TP_RX_PATH_COUNT,
    /* TpTxPaths */ Fake_PduR_TpTxPaths,
    /* TpTxPathCount */ FAKE_PDUR_TP_TX_PATH_COUNT
};

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* SID 0x3E TesterPresent のコールチェーンのテスト用フィクスチャ。CAN の受信（0x7E0）から応答の送信（0x7E8）までを実際に通して確認する。
 * SetUp(): 模擬 Can_Hw と wrap をリセットし、Can_Init() → CanIf_Init(&kTestCanIfConfig)（コントローラを STARTED、PDU を ONLINE にする）→ PduR_Init() → CanSM_Init(NULL)（CanIf_RxIndication() が呼ぶため）→ CanTp_Init(NULL) → Dem_Init(NULL) → Dcm_Init(NULL) の順に初期化する。
 * TearDown(): CanSM_DeInit() と CanIf_DeInit() で未初期化へ戻す。 */
class DcmStack_ComMModeEntered_DeferredTimeout_Test : public ::testing::Test
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
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制

        canConfig.filter.filterId = 0x0000U;
        canConfig.filter.mask     = 0x0000U;  // 全ID受理（Can_PBCfg.c の本番設定と同じ）
        canConfig.csPin           = 10U;
        canConfig.intPin          = 2U;
        canConfig.baudrate        = 500000U;
        canConfig.crystalFreq     = CAN_CRYSTAL_16MHZ;

        Can_Test_SetConfigPtr(NULL);  // 未初期化へ戻す（Can_Init() は初期化済みだと CAN_E_TRANSITION で拒否する）
        Can_Init(&canConfig);
        CanIf_Init(&kTestCanIfConfig);
        /* CanIf_Init() 直後は CanIf_ControllerMode[] が CAN_CS_STOPPED に
         * 巻き戻るため、CanIf_SetControllerMode() 経由で明示的に起動する
         * （ComStack_Signal_Rx_test.cpp と同じ理由）。 */
        CanIf_SetControllerMode(0U, CAN_CS_STARTED);
        /* 本テストは CanSM 経由で FULL_COM を確立しないため、CanIf_Init()
         * 直後の既定値 CANIF_OFFLINE のままだと応答側の CanIf_Transmit() が
         * 常に E_NOT_OK になってしまう（ComStack_Signal_Tx_test.cpp
         * と同じ理由）。 */
        CanIf_SetPduMode(0U, CANIF_ONLINE);
        PduR_Init(&kTestPduRConfig);
        /* 以前は CanIf_RxIndication() が CanSM_RxIndication() を無条件に呼んでおり（2026-10 に廃止、現在は呼ばない。共有 static の状態を揃えるため CanSM_Init は残している）、
         * CanSM 未初期化のままだと DET_E_UNINIT が毎回報告される
         * （ComStack_Signal_Rx_test.cpp と同じ理由。CanSM 自体の状態機械は
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

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(DcmStack_ComMModeEntered_DeferredTimeout_Test, NG_RequestIsDiscardedWhenFullComIsTooLate)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dcm_ComM_SilentComModeEntered(COMM_CHANNEL_0);
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 2U;
    FakeCanHw_RxData[1] = DCM_SID_SESSION_CTRL;
    FakeCanHw_RxData[2] = DCM_SESSION_EXTENDED;
    for (uint8 i = 3U; i < 8U; i++)
        FakeCanHw_RxData[i] = 0U;
    FakeCanHw_RxPendingCount = 1U;
    Can_MainFunction_Read();                         // FULL_COM 前に要求が届く

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    FakeMillis_Value += DCM_FULLCOM_WAIT_MS;         // FULL_COM にならないまま、待ち時間が過ぎる
    Dcm_MainFunction();                              // 要求を捨てる
    Dcm_ComM_FullComModeEntered(COMM_CHANNEL_0);     // その後に FULL_COM になっても、
    Dcm_MainFunction();                              // 捨てた要求は実行されない

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    Dcm_SesCtrlType session = DCM_SESSION_EXTENDED;
    ASSERT_EQ(Dcm_GetSesCtrlType(&session), E_OK);
    EXPECT_EQ(session, DCM_SESSION_DEFAULT);         // 要求は実行されない
    EXPECT_EQ(FakeCanHw_SendCount, 0U);              // 応答も送信されない
}

}  // namespace
