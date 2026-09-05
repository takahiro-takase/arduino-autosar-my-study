/**
 * \file    Bsw_Dcm_CommunicationControl_test.cpp
 * \brief   UDS SID 0x28 CommunicationControl の単体テスト（GoogleTest /
 *          PlatformIO `[env:native_dcm]`）。
 *
 * \details 2026-09-05、シグネチャ準拠サーベイで
 *          `Dcm_HandleCommunicationControl()`/`Dcm_CommControlReset()` が
 *          `Com_SetCommunicationEnabled()`/`Nm_EnableCommunication()`/
 *          `Nm_DisableCommunication()` を直接呼んでいたレイヤ違反を是正し、
 *          `BswM_Dcm_CommunicationMode_CurrentState()`（BswM_fake.h で
 *          スパイに差し替え）経由へ変更した際に新設。
 *
 *          Bsw_Dcm_ControlDTCSetting_test.cpp と同じ「Dcm_ComIndication() に
 *          生の UDS バイト列を直接渡し、応答と副作用（本テストでは
 *          BswM_fake の記録）を検証する」ブラックボックステスト方式。
 *          BswM.c 自体（ルールエンジン本体）はこの env にリンクされないため、
 *          「Dcm が正しい Dcm_CommunicationModeType 値で BswM を呼んだか」の
 *          みを検証し、Com/Nm への実際の反映（BswM_ApplyDcmCommMode()）は
 *          対象外とする（platformio.ini [env:native_dcm] コメント参照）。
 */
#include <gtest/gtest.h>

extern "C" {
#include "Dcm.h"
#include "Dcm_Cfg.h"
#include "Dem.h"
#include "CanTp_fake.h"
#include "BswM_fake.h"
#include "Hal_Millis_fake.h"
#include "Hal_Det_Hw_fake.h"
}

namespace
{

class Bsw_Dcm_CommunicationControl_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        FakeCanTp_Reset();
        FakeBswM_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制

        Dem_Init(NULL);
        Dcm_Init(NULL);
        EnterExtendedSession();  // 0x28 は extendedSession 限定

        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
    }

    /** UDS ペイロードを Dcm_ComIndication() へ直接渡す。 */
    void Send(const uint8* payload, uint8 len)
    {
        PduInfoType pdu = { const_cast<uint8*>(payload), len };
        Dcm_ComIndication(0U, &pdu);
    }

    /** [0x10, 0x03] extendedDiagnosticSession へ遷移する。 */
    void EnterExtendedSession()
    {
        uint8 req[2] = { DCM_SID_SESSION_CTRL, DCM_SESSION_EXTENDED };
        Send(req, sizeof(req));
        ASSERT_EQ(FakeCanTp_TxBuf[0], 0x50U);  // 正応答確認（前提が崩れていないこと）
        FakeCanTp_Reset();
    }

    /** [0x28, controlType, communicationType] を送る。 */
    void SendCommunicationControl(uint8 controlType, uint8 communicationType)
    {
        uint8 req[3] = { DCM_SID_COMM_CONTROL, controlType, communicationType };
        Send(req, sizeof(req));
    }
};

// ------------------------------------------------------------
// Dcm_CommunicationModeType への変換（controlType + (communicationType-1)*4）
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_CommunicationControl_Test, OK_EnableRxTxNormalMapsToDcmEnableRxTxNorm)
{
    SendCommunicationControl(0x00U /* enableRxAndTx */, 0x01U /* normal */);

    EXPECT_EQ(FakeCanTp_TxBuf[0], 0x68U);
    EXPECT_EQ(FakeCanTp_TxBuf[1], 0x00U);
    EXPECT_EQ(FakeBswM_CallCount, 1U);
    EXPECT_EQ(FakeBswM_LastMode, DCM_ENABLE_RX_TX_NORM);
}

TEST_F(Bsw_Dcm_CommunicationControl_Test, OK_DisableRxTxNmMapsToDcmDisableRxTxNm)
{
    SendCommunicationControl(0x03U /* disableRxAndTx */, 0x02U /* NM */);

    EXPECT_EQ(FakeCanTp_TxBuf[0], 0x68U);
    EXPECT_EQ(FakeCanTp_TxBuf[1], 0x03U);
    EXPECT_EQ(FakeBswM_CallCount, 1U);
    EXPECT_EQ(FakeBswM_LastMode, DCM_DISABLE_RX_TX_NM);
}

TEST_F(Bsw_Dcm_CommunicationControl_Test, OK_EnableRxDisableTxNormAndNmMapsToDcmEnableRxDisableTxNormNm)
{
    SendCommunicationControl(0x01U /* enableRxAndDisableTx */, 0x03U /* normal + NM */);

    EXPECT_EQ(FakeCanTp_TxBuf[0], 0x68U);
    EXPECT_EQ(FakeCanTp_TxBuf[1], 0x01U);
    EXPECT_EQ(FakeBswM_CallCount, 1U);
    EXPECT_EQ(FakeBswM_LastMode, DCM_ENABLE_RX_DISABLE_TX_NORM_NM);
}

TEST_F(Bsw_Dcm_CommunicationControl_Test, OK_DisableRxEnableTxNormMapsToDcmDisableRxEnableTxNorm)
{
    SendCommunicationControl(0x02U /* disableRxAndEnableTx */, 0x01U /* normal */);

    EXPECT_EQ(FakeBswM_LastMode, DCM_DISABLE_RX_ENABLE_TX_NORM);
}

// ------------------------------------------------------------
// 異常系
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_CommunicationControl_Test, NG_UnsupportedControlTypeReturnsNegativeResponseWithoutCallingBswM)
{
    SendCommunicationControl(0x04U /* enableRxAndDisableTxWithEnhancedAddressInformation、非対応 */, 0x01U);

    EXPECT_EQ(FakeCanTp_TxBuf[0], 0x7FU);
    EXPECT_EQ(FakeCanTp_TxBuf[2], DCM_NRC_SUB_FUNC_NOT_SUPPORTED);
    EXPECT_EQ(FakeBswM_CallCount, 0U);
}

TEST_F(Bsw_Dcm_CommunicationControl_Test, NG_InvalidCommunicationTypeReturnsNegativeResponseWithoutCallingBswM)
{
    SendCommunicationControl(0x00U, 0x00U /* 0 は未定義 */);

    EXPECT_EQ(FakeCanTp_TxBuf[0], 0x7FU);
    EXPECT_EQ(FakeCanTp_TxBuf[2], DCM_NRC_REQUEST_OUT_OF_RANGE);
    EXPECT_EQ(FakeBswM_CallCount, 0U);
}

TEST_F(Bsw_Dcm_CommunicationControl_Test, NG_IncorrectLengthReturnsNegativeResponseWithoutCallingBswM)
{
    uint8 req[2] = { DCM_SID_COMM_CONTROL, 0x00U };
    Send(req, sizeof(req));

    EXPECT_EQ(FakeCanTp_TxBuf[0], 0x7FU);
    EXPECT_EQ(FakeCanTp_TxBuf[2], DCM_NRC_INCORRECT_MESSAGE_LENGTH);
    EXPECT_EQ(FakeBswM_CallCount, 0U);
}

// ------------------------------------------------------------
// Dcm_CommControlReset()（defaultSession への遷移で通信を初期状態へ戻す）
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_CommunicationControl_Test, OK_ReturnToDefaultSessionResetsToEnableRxTxNormNm)
{
    SendCommunicationControl(0x03U /* disableRxAndTx */, 0x03U /* normal + NM */);
    ASSERT_EQ(FakeBswM_LastMode, DCM_DISABLE_RX_TX_NORM_NM);
    FakeBswM_Reset();

    uint8 req[2] = { DCM_SID_SESSION_CTRL, DCM_SESSION_DEFAULT };
    Send(req, sizeof(req));

    EXPECT_EQ(FakeBswM_CallCount, 1U);
    EXPECT_EQ(FakeBswM_LastMode, DCM_ENABLE_RX_TX_NORM_NM);
}

}  // namespace
