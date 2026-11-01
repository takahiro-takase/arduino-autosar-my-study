/**
 * \file    CanIf_TriggerTransmit_test.cpp
 * \brief   CanIf_TriggerTransmit() の単体テスト
 * \details 上位層の TriggerTransmit への転送（[SWS_CANIF_00885]）、翻訳した PDU ID、対応しない TX PDU（[SWS_CANIF_00888]）、
 *          範囲外の TxPduId、NULL ポインタ、未初期化を確認する。
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

// 上位層の TriggerTransmit（偽物）の記録。
uint32         gUpperCallCount;
PduIdType      gUpperPduId;
PduInfoType*   gUpperPduInfo;
Std_ReturnType gUpperReturn;

Std_ReturnType FakeUpperTriggerTransmit(PduIdType TxPduId, PduInfoType* PduInfoPtr)
{
    gUpperCallCount++;
    gUpperPduId   = TxPduId;
    gUpperPduInfo = PduInfoPtr;
    return gUpperReturn;
}

// TX PDU 0: トリガ送信に対応（上位層の PDU ID は 7）。TX PDU 1: 対応しない。
const CanIf_TxPduConfigType kTestTxPdus[] = {
    { /* UpperLayerTxPduId */ 7U, /* CanId */ 0x100U, /* Dlc */ 8U, /* Hth */ 0U, /* TxConfirmFct */ NULL,
      /* TriggerTransmitFct */ FakeUpperTriggerTransmit },
    { /* UpperLayerTxPduId */ 8U, /* CanId */ 0x101U, /* Dlc */ 8U, /* Hth */ 0U, /* TxConfirmFct */ NULL,
      /* TriggerTransmitFct */ NULL }
};

const CanIf_RxPduConfigType kTestRxPdu = {
    /* CanId */                0x200U,
    /* Hrh */                  0U,
    /* UpperLayerRxPduId */    0U,
    /* Dlc */                  1U,
    /* RxIndicationFct */      NULL,
    /* ReadRxPduDataEnabled */ 0U
};

const CanIf_ConfigType kTestCanIfConfig = {
    kTestTxPdus, 2U,
    &kTestRxPdu, 1U
};

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* CanIf_TriggerTransmit() の単体テスト用フィクスチャ。
 * SetUp(): 上位層の記録を消し、CanSM_Init(NULL)（共有 static の状態を揃える）、CanIf_Init(&kTestCanIfConfig) を呼ぶ
 * （Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): CanIf_DeInit() と CanSM_DeInit() で未初期化へ戻す。 */
class CanIf_TriggerTransmit_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        gUpperCallCount = 0U;
        gUpperPduId     = 0xFFFFU;
        gUpperPduInfo   = NULL;
        gUpperReturn    = E_OK;
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

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(CanIf_TriggerTransmit_Test, CanIf_TriggerTransmit_OK_ForwardsToUpperLayerWithTranslatedPduId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 sdu[8] = { 0 };
    PduInfoType info = { sdu, 8U };
    gUpperReturn = E_OK;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanIf_TriggerTransmit(0U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(gUpperCallCount, 1U);
    EXPECT_EQ(gUpperPduId, 7U);          // UpperLayerTxPduId へ翻訳する（[SWS_CANIF_00885]）
    EXPECT_EQ(gUpperPduInfo, &info);     // PduInfoPtr は、そのまま渡す
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(CanIf_TriggerTransmit_Test, CanIf_TriggerTransmit_OK_ReturnsUpperLayerReturnValue)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 sdu[8] = { 0 };
    PduInfoType info = { sdu, 8U };
    gUpperReturn = E_NOT_OK;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanIf_TriggerTransmit(0U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);            // 上位層の戻り値を、そのまま返す
    EXPECT_EQ(gUpperCallCount, 1U);
}

TEST_F(CanIf_TriggerTransmit_Test, CanIf_TriggerTransmit_NG_PduNotConfiguredForTriggerTransmit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 sdu[8] = { 0 };
    PduInfoType info = { sdu, 8U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanIf_TriggerTransmit(1U, &info);   // TriggerTransmitFct が NULL の TX PDU

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);            // [SWS_CANIF_00888]
    EXPECT_EQ(gUpperCallCount, 0U);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);   // 設定どおりの動作なので、DET へは報告しない
}

TEST_F(CanIf_TriggerTransmit_Test, CanIf_TriggerTransmit_NG_InvalidTxPduId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 sdu[8] = { 0 };
    PduInfoType info = { sdu, 8U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanIf_TriggerTransmit(2U, &info);   // TxPduCount=2 のため範囲外

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_INVALID_TXPDUID);
    EXPECT_EQ(gUpperCallCount, 0U);
}

TEST_F(CanIf_TriggerTransmit_Test, CanIf_TriggerTransmit_NG_NullPduInfoPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanIf_TriggerTransmit(0U, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_POINTER);
    EXPECT_EQ(gUpperCallCount, 0U);
}

TEST_F(CanIf_TriggerTransmit_Test, CanIf_TriggerTransmit_NG_NullSduDataPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PduInfoType info = { NULL, 8U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanIf_TriggerTransmit(0U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_POINTER);
    EXPECT_EQ(gUpperCallCount, 0U);
}

TEST_F(CanIf_TriggerTransmit_Test, CanIf_TriggerTransmit_NG_UninitializedReturnsError)
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
    const Std_ReturnType ret = CanIf_TriggerTransmit(0U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(gUpperCallCount, 0U);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);   // 未初期化は DET へ報告する（[SWS_CANIF_00901]）
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_UNINIT);
    EXPECT_EQ(FakeDetHw_LastApiId, CANIF_API_ID_TRIGGER_TRANSMIT);
}

}  // namespace
