/**
 * \file    CanIf_Transmit_test.cpp
 * \brief   CanIf_Transmit() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 CanIf_test.cpp）。
 *
 *          分割前の共通の説明は、旧 CanIf_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "CanIf.h"
#include "CanSM.h"
#include "Can.h"
#include "Can_Hw.h"
#include "Fake_Can_Hw.h"
#include "Fake_Det_Hw.h"
#include "Wrap_CanIf.h"
#include "Wrap_Can.h"
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

const CanIf_TxPduConfigType kTestTxPdu = {
    /* UpperLayerTxPduId */ 0U,
    /* CanId */             0x100U,
    /* Dlc */               8U,
    /* Hth */               0U,
    /* TxConfirmFct */      NULL
};

const CanIf_RxPduConfigType kTestRxPdu = {
    /* CanId */                0x200U,
    /* Hrh */                  0U,
    /* UpperLayerRxPduId */    0U,
    /* Dlc */                  1U,
    /* RxIndicationFct */      NULL,
    /* ReadRxPduDataEnabled */ 0U  // CanIf_ReadRxPduData_NG_NotOptedIn がこの既定値に依存する
};

const CanIf_ConfigType kTestCanIfConfig = {
    &kTestTxPdu, 1U,
    &kTestRxPdu, 1U
};

/* トリガ送信に対応する TX PDU（TriggerTransmitFct あり）。関数の実体は呼ばれない（CanIf_TriggerTransmit() 自体を wrap で差し替えるため）。 */
Std_ReturnType DummyUpperTriggerTransmit(PduIdType TxPduId, PduInfoType* PduInfoPtr)
{
    (void)TxPduId;
    (void)PduInfoPtr;
    return E_NOT_OK;
}

const CanIf_TxPduConfigType kTriggerTxPdu = {
    /* UpperLayerTxPduId */ 0U,
    /* CanId */             0x100U,
    /* Dlc */               8U,
    /* Hth */               0U,
    /* TxConfirmFct */      NULL,
    /* TriggerTransmitFct */ DummyUpperTriggerTransmit
};

const CanIf_ConfigType kTriggerCanIfConfig = {
    &kTriggerTxPdu, 1U,
    &kTestRxPdu,    1U
};

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* CanIf（上記 2 グループ以外の API）の単体テスト用フィクスチャ。
 * SetUp(): 以前は CanIf_RxIndication() が CanSM_RxIndication() を無条件に呼んでおり（2026-10 に廃止。共有 static の状態を揃えるため CanSM_Init は残している） CanSM_Init(NULL) を先に呼び、続けて CanIf_Init(&kTestCanIfConfig) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): CanIf_DeInit() と CanSM_DeInit() で未初期化へ戻す。 */
class CanIf_Transmit_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        WrapCanIf_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制

        CanSM_Init(NULL);
        CanIf_Init(&kTestCanIfConfig);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        CanIf_DeInit();
        CanSM_DeInit();
    }
};

/* トリガ送信の TX PDU の送信（CanIf_Transmit() の SduDataPtr = NULL）の単体テスト用フィクスチャ。
 * SetUp(): Can を、トリガ送信を有効にして Can_Init() し、CanIf を kTriggerCanIfConfig で初期化して、コントローラを STARTED、PDU モードを ONLINE にする
 *          （Can_Write() は実体。CanIf_TriggerTransmit() は wrap で、上位層が返すデータを差し込む）。
 * TearDown(): CanIf_DeInit() と CanSM_DeInit() で未初期化へ戻す。 */
class CanIf_Transmit_TriggerTransmit_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeCanHw_Reset();
        WrapCan_Reset();
        WrapCanIf_Reset();
        FakeDetHw_LogSuppressed = 1U;

        canConfig.filter.filterId = 0x0220U;
        canConfig.filter.mask     = 0x1FFFU;
        canConfig.csPin           = 10U;
        canConfig.intPin          = 2U;
        canConfig.baudrate        = 500000U;
        canConfig.crystalFreq     = CAN_CRYSTAL_16MHZ;
        canConfig.fdEnabled       = 0U;
        canConfig.triggerTransmitEnable = 1U;

        Can_Test_SetConfigPtr(NULL);
        Can_Init(&canConfig);
        CanSM_Init(NULL);
        CanIf_Init(&kTriggerCanIfConfig);
        (void)CanIf_SetControllerMode(0U, CAN_CS_STARTED);
        (void)CanIf_SetPduMode(0U, CANIF_ONLINE);   // 送信できる状態（CanIf_Init() 直後は OFFLINE）
        FakeDetHw_Reset();
        FakeDetHw_LogSuppressed = 0U;
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        WrapCan_Reset();
        CanIf_DeInit();
        CanSM_DeInit();
    }

    Can_ConfigType canConfig;
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(CanIf_Transmit_Test, CanIf_Transmit_NG_InvalidTxPduId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 sdu[8] = { 0 };
    PduInfoType info = { sdu, 1U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CanIf_Transmit(1U, &info);  // TxPduCount=1 のため範囲外

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_INVALID_TXPDUID);
}

TEST_F(CanIf_Transmit_Test, CanIf_Transmit_NG_NullPduInfoPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CanIf_Transmit(0U, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_POINTER);
}

TEST_F(CanIf_Transmit_Test, CanIf_Transmit_NG_NullSduDataPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PduInfoType info = { NULL, 1U };   // PduInfoPtr は有効、SduDataPtr だけ NULL

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CanIf_Transmit(0U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_POINTER);
}

TEST_F(CanIf_Transmit_Test, CanIf_Transmit_NG_SduLengthAboveDlc)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 sdu[9] = { 0 };
    PduInfoType info = { sdu, 9U };   // 設定の Dlc（8）を超える
    const uint32 writeBefore = CallCount_Can_Write;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanIf_Transmit(0U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(CallCount_Can_Write, writeBefore);   // Can ドライバへ渡さない
}

TEST_F(CanIf_Transmit_Test, CanIf_Transmit_NG_PduModeNotOnline)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(CanIf_SetPduMode(0U, CANIF_TX_OFFLINE), E_OK);   // 送信を止める
    uint8 sdu[8] = { 0 };
    PduInfoType info = { sdu, 8U };
    const uint32 writeBefore = CallCount_Can_Write;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanIf_Transmit(0U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(CallCount_Can_Write, writeBefore);   // Can ドライバへ渡さない
}


TEST_F(CanIf_Transmit_Test, CanIf_Transmit_NG_UninitializedReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 sdu[8] = { 0 };
    PduInfoType info = { sdu, 8U };
    CanIf_DeInit();   // 未初期化にする

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanIf_Transmit(0U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);   // 未初期化は DET へ報告する（[SWS_CANIF_00901]）
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_UNINIT);
    EXPECT_EQ(FakeDetHw_LastApiId, CANIF_API_ID_TRANSMIT);
}

TEST_F(CanIf_Transmit_TriggerTransmit_Test, CanIf_Transmit_OK_NullSduDataPtrIsPassedToCanForTriggerTransmitPdu)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PduInfoType info = { NULL, 0U };   // トリガ送信の TX PDU は、SduDataPtr = NULL を受け付ける（[SWS_CANIF_00882]）
    FailFromCallCount_CanIf_TriggerTransmit = 1U;
    ForcedReturn_CanIf_TriggerTransmit      = E_OK;
    ForcedSduLength_CanIf_TriggerTransmit   = 2U;
    ForcedSduData_CanIf_TriggerTransmit[0]  = 0xC1U;
    ForcedSduData_CanIf_TriggerTransmit[1]  = 0xC2U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanIf_Transmit(0U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(CallCount_CanIf_TriggerTransmit, 1U);          // NULL のまま Can_Write() へ渡り、Can がデータを取りに来た
    EXPECT_EQ(LastPduId_CanIf_TriggerTransmit, 0U);          // 取りに来た PDU は、送信を要求した TxPduId
    EXPECT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendDlc, 2U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0xC1U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0xC2U);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(CanIf_Transmit_TriggerTransmit_Test, CanIf_Transmit_NG_NullSduDataPtrWhenTriggerTransmitFails)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PduInfoType info = { NULL, 0U };
    FailFromCallCount_CanIf_TriggerTransmit = 1U;
    ForcedReturn_CanIf_TriggerTransmit      = E_NOT_OK;   // 上位層がデータを渡せない

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanIf_Transmit(0U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);   // [SWS_Can_00506] を経て、CanIf_Transmit() も E_NOT_OK
    EXPECT_EQ(FakeCanHw_SendCount, 0U);
}

TEST_F(CanIf_Transmit_TriggerTransmit_Test, CanIf_Transmit_OK_NonNullSduDataPtrIsSentAsIs)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 sdu[2] = { 0x11U, 0x22U };
    PduInfoType info = { sdu, 2U };   // トリガ送信の PDU でも、データを渡されたら、取りに行かず、そのまま送る

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanIf_Transmit(0U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(CallCount_CanIf_TriggerTransmit, 0U);
    EXPECT_EQ(FakeCanHw_LastSendDlc, 2U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x11U);
}

}  // namespace
