/**
 * \file    Bsw_Dcm_CommunicationControl_test.cpp
 * \brief   UDS SID 0x28 CommunicationControl の単体テスト（GoogleTest /
 *          PlatformIO `[env:native_chain]`。2026-08新設時は専用環境
 *          `[env:native_dcm]`だったが、2026-09にDem実体リンク化に続けて
 *          本envへ統合した）。
 *
 * \details 2026-09-05、シグネチャ準拠サーベイで
 *          `Dcm_HandleCommunicationControl()`/`Dcm_CommControlReset()` が
 *          `Com_SetCommunicationEnabled()`/`Nm_EnableCommunication()`/
 *          `Nm_DisableCommunication()` を直接呼んでいたレイヤ違反を是正し、
 *          `BswM_Dcm_CommunicationMode_CurrentState()`（`Wrap_BswM.h`で
 *          呼び出し回数・引数をキャプチャ）経由へ変更した際に新設。
 *
 *          Bsw_Dcm_ControlDTCSetting_test.cpp と同じ「Dcm_ComIndication() に
 *          生の UDS バイト列を直接渡し、応答と副作用（本テストでは
 *          Wrap_BswM の記録）を検証する」ブラックボックステスト方式。
 *          BswM.c 自体（ルールエンジン本体）は実体でリンクされる
 *          （2026-09-22、Fake_Bsw_BswM.c から切り替え）が、`BswM_Init()` を
 *          本テストでは呼ばないため `BswM_Cfg==NULL` ガードでルール評価自体は
 *          一切走らない。よって「Dcm が正しい Dcm_CommunicationModeType 値で
 *          BswM を呼んだか」のみを検証し、Com/Nm への実際の反映
 *          （`BswM_ApplyDcmCommMode()`）は対象外とする。
 */
#include <gtest/gtest.h>

extern "C" {
#include "Dcm.h"
#include "Dcm_Cfg.h"
#include "Dem.h"
#include "Wrap_CanTp.h"
#include "Wrap_BswM.h"
#include "Wrap_ComM.h"
#include "Fake_Millis.h"
#include "Fake_Det_Hw.h"
}

namespace
{

class Bsw_Dcm_CommunicationControl_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        WrapCanTp_Reset();
        WrapBswM_Reset();
        Suppressed_ComM_DcmDiagnostic = 1U;  // 本テストは通信管理(ComM/CanSM/Nm)が対象外
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制

        CanTp_Init(NULL);
        Dem_Init(NULL);
        Dcm_Init(NULL);
        EnterExtendedSession();  // 0x28 は extendedSession 限定

        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        WrapComM_Reset();  // 他のテストファイルへ影響を残さない
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
        ASSERT_EQ(LastData_CanTp_Transmit[0], 0x50U);  // 正応答確認（前提が崩れていないこと）
        WrapCanTp_Reset();
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

    EXPECT_EQ(LastData_CanTp_Transmit[0], 0x68U);
    EXPECT_EQ(LastData_CanTp_Transmit[1], 0x00U);
    EXPECT_EQ(CallCount_BswM_Dcm_CommunicationMode_CurrentState, 1U);
    EXPECT_EQ(LastRequestedMode_BswM_Dcm_CommunicationMode_CurrentState, DCM_ENABLE_RX_TX_NORM);
}

TEST_F(Bsw_Dcm_CommunicationControl_Test, OK_DisableRxTxNmMapsToDcmDisableRxTxNm)
{
    SendCommunicationControl(0x03U /* disableRxAndTx */, 0x02U /* NM */);

    EXPECT_EQ(LastData_CanTp_Transmit[0], 0x68U);
    EXPECT_EQ(LastData_CanTp_Transmit[1], 0x03U);
    EXPECT_EQ(CallCount_BswM_Dcm_CommunicationMode_CurrentState, 1U);
    EXPECT_EQ(LastRequestedMode_BswM_Dcm_CommunicationMode_CurrentState, DCM_DISABLE_RX_TX_NM);
}

TEST_F(Bsw_Dcm_CommunicationControl_Test, OK_EnableRxDisableTxNormAndNmMapsToDcmEnableRxDisableTxNormNm)
{
    SendCommunicationControl(0x01U /* enableRxAndDisableTx */, 0x03U /* normal + NM */);

    EXPECT_EQ(LastData_CanTp_Transmit[0], 0x68U);
    EXPECT_EQ(LastData_CanTp_Transmit[1], 0x01U);
    EXPECT_EQ(CallCount_BswM_Dcm_CommunicationMode_CurrentState, 1U);
    EXPECT_EQ(LastRequestedMode_BswM_Dcm_CommunicationMode_CurrentState, DCM_ENABLE_RX_DISABLE_TX_NORM_NM);
}

TEST_F(Bsw_Dcm_CommunicationControl_Test, OK_DisableRxEnableTxNormMapsToDcmDisableRxEnableTxNorm)
{
    SendCommunicationControl(0x02U /* disableRxAndEnableTx */, 0x01U /* normal */);

    EXPECT_EQ(LastRequestedMode_BswM_Dcm_CommunicationMode_CurrentState, DCM_DISABLE_RX_ENABLE_TX_NORM);
}

// ------------------------------------------------------------
// 異常系
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_CommunicationControl_Test, NG_UnsupportedControlTypeReturnsNegativeResponseWithoutCallingBswM)
{
    SendCommunicationControl(0x04U /* enableRxAndDisableTxWithEnhancedAddressInformation、非対応 */, 0x01U);

    EXPECT_EQ(LastData_CanTp_Transmit[0], 0x7FU);
    EXPECT_EQ(LastData_CanTp_Transmit[2], DCM_NRC_SUB_FUNC_NOT_SUPPORTED);
    EXPECT_EQ(CallCount_BswM_Dcm_CommunicationMode_CurrentState, 0U);
}

TEST_F(Bsw_Dcm_CommunicationControl_Test, NG_InvalidCommunicationTypeReturnsNegativeResponseWithoutCallingBswM)
{
    SendCommunicationControl(0x00U, 0x00U /* 0 は未定義 */);

    EXPECT_EQ(LastData_CanTp_Transmit[0], 0x7FU);
    EXPECT_EQ(LastData_CanTp_Transmit[2], DCM_NRC_REQUEST_OUT_OF_RANGE);
    EXPECT_EQ(CallCount_BswM_Dcm_CommunicationMode_CurrentState, 0U);
}

TEST_F(Bsw_Dcm_CommunicationControl_Test, NG_IncorrectLengthReturnsNegativeResponseWithoutCallingBswM)
{
    uint8 req[2] = { DCM_SID_COMM_CONTROL, 0x00U };
    Send(req, sizeof(req));

    EXPECT_EQ(LastData_CanTp_Transmit[0], 0x7FU);
    EXPECT_EQ(LastData_CanTp_Transmit[2], DCM_NRC_INCORRECT_MESSAGE_LENGTH);
    EXPECT_EQ(CallCount_BswM_Dcm_CommunicationMode_CurrentState, 0U);
}

TEST_F(Bsw_Dcm_CommunicationControl_Test, NG_UnsupportedControlTypeWithWrongLengthPrefersSubFuncNrc)
{
    /* [SWS_Dcm_00273]/[SWS_Dcm_00696]: サブ機能サポート確認は最小メッセージ長
     * 確認より先に行う処理順序（2026-09 是正）。controlType(uds[1])が
     * 不正かつ communicationType(uds[2]) が欠けている(udsLen=2<3)場合でも、
     * NRC 0x13(incorrectMessageLength)ではなく 0x12(subFunctionNotSupported)
     * を返すべき。 */
    uint8 req[2] = { DCM_SID_COMM_CONTROL, 0xFFU };
    Send(req, sizeof(req));

    EXPECT_EQ(LastData_CanTp_Transmit[0], 0x7FU);
    EXPECT_EQ(LastData_CanTp_Transmit[2], DCM_NRC_SUB_FUNC_NOT_SUPPORTED);
    EXPECT_EQ(CallCount_BswM_Dcm_CommunicationMode_CurrentState, 0U);
}

// ------------------------------------------------------------
// Dcm_CommControlReset()（defaultSession への遷移で通信を初期状態へ戻す）
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_CommunicationControl_Test, OK_ReturnToDefaultSessionResetsToEnableRxTxNormNm)
{
    SendCommunicationControl(0x03U /* disableRxAndTx */, 0x03U /* normal + NM */);
    ASSERT_EQ(LastRequestedMode_BswM_Dcm_CommunicationMode_CurrentState, DCM_DISABLE_RX_TX_NORM_NM);
    WrapBswM_Reset();

    uint8 req[2] = { DCM_SID_SESSION_CTRL, DCM_SESSION_DEFAULT };
    Send(req, sizeof(req));

    EXPECT_EQ(CallCount_BswM_Dcm_CommunicationMode_CurrentState, 1U);
    EXPECT_EQ(LastRequestedMode_BswM_Dcm_CommunicationMode_CurrentState, DCM_ENABLE_RX_TX_NORM_NM);
}

}  // namespace
