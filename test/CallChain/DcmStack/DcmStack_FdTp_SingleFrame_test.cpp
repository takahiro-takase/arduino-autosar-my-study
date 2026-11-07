/**
 * \file    DcmStack_FdTp_SingleFrame_test.cpp
 * \brief   CAN FD のフレームで届いた UDS 要求（SF）に、FD のフレーム（SF）で応答する、フルコールチェーンテスト
 *          （GoogleTest / CMake native_chain_tests）。
 *
 * \details DcmStack_SID3E_TesterPresent_test.cpp と同じ型。要求は、12 バイトの FD フレームに入れた
 *          ReadDataByIdentifier（VIN、DID 0xF190）の SF（PCI は 0x00 + SF_DL。「エスケープ」形式）。
 *          応答は 20 バイトで、クラシックなら FF + CF だが、FD の SF（PCI 2 バイト + データ 20 バイト = 22 を、
 *          有効な長さ 24 へ切り上げ、残りは 0xAA）に収まるため、1 フレームで返る。
 *          要求がクラシックのときの動作は、DcmStack_SID22_ReadDataById_test.cpp が確認している。
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
      /* Dlc */               64U,   // 診断応答は、相手が FD のとき、最大 64 バイトで送る（CANTP_FD_FRAME_LEN）
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

/* FD の SF 要求のコールチェーンのテスト用フィクスチャ（SID 0x3E のものと同じ型。CanIf の診断応答の DLC は 64、CAN FD 有効）。CAN の受信（0x7E0）から応答の送信（0x7E8）までを実際に通して確認する。
 * SetUp(): 模擬 Can_Hw と wrap をリセットし、Can_Init() → CanIf_Init(&kTestCanIfConfig)（コントローラを STARTED、PDU を ONLINE にする）→ PduR_Init() → CanSM_Init(NULL)（CanIf_RxIndication() が呼ぶため）→ CanTp_Init(NULL) → Dem_Init(NULL) → Dcm_Init(NULL) の順に初期化する。
 * TearDown(): CanSM_DeInit() と CanIf_DeInit() で未初期化へ戻す。 */
class DcmStack_FdTp_SingleFrame_Test : public ::testing::Test
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
        canConfig.fdEnabled       = 1U;

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

}  // namespace

/* ======================================================================
 * Test Functions
 * ====================================================================== */

// ------------------------------------------------------------
// OK: FD の SF（VIN の読み出し要求）に、FD の SF（24 バイトのフレーム）で、正応答を返す。
// ------------------------------------------------------------
TEST_F(DcmStack_FdTp_SingleFrame_Test, OK_FdSingleFrameRequestGetsFdSingleFrameResponse)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // FD の SF: 00 03 22 F1 90 + パディング（12 バイトのフレーム）
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 12U;
    FakeCanHw_RxData[0] = 0x00U;
    FakeCanHw_RxData[1] = 3U;
    FakeCanHw_RxData[2] = DCM_SID_READ_DATA;
    FakeCanHw_RxData[3] = (uint8)(DCM_DID_VIN >> 8U);
    FakeCanHw_RxData[4] = (uint8)(DCM_DID_VIN & 0xFFU);
    for (uint8 i = 5U; i < 12U; i++)
        FakeCanHw_RxData[i] = CANTP_PADDING_BYTE;
    FakeCanHw_RxPendingCount = 1U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_MainFunction_Read();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_EQ(FakeCanHw_SendCount, 1U);                   // 応答は 1 フレームだけ
    EXPECT_EQ(FakeCanHw_LastSendId, 0x7E8U);
    EXPECT_EQ(FakeCanHw_LastSendDlc, 24U);                // PCI 2 + データ 20 = 22 を、有効な長さ 24 へ
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x00U);          // FD の SF: エスケープの PCI
    EXPECT_EQ(FakeCanHw_LastSendData[1], 20U);            // SF_DL
    EXPECT_EQ(FakeCanHw_LastSendData[2], DCM_SID_READ_DATA + 0x40U);   // 正応答 0x62
    EXPECT_EQ(FakeCanHw_LastSendData[3], (uint8)(DCM_DID_VIN >> 8U));
    EXPECT_EQ(FakeCanHw_LastSendData[4], (uint8)(DCM_DID_VIN & 0xFFU));
    for (uint8 i = 22U; i < 24U; i++)
        EXPECT_EQ(FakeCanHw_LastSendData[i], CANTP_PADDING_BYTE);   // 余りは CANTP_PADDING_BYTE
}
