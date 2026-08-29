/**
 * \file    Bsw_Dcm_ControlDTCSetting_test.cpp
 * \brief   UDS SID 0x85 ControlDTCSetting の単体テスト（GoogleTest /
 *          PlatformIO `[env:native_dcm]`）。
 *
 * \details Bsw_Dcm_ReadDtcInfo_test.cpp と同じ「Dcm_ComIndication() に生の
 *          UDS バイト列を直接渡し、CanTp_Transmit()（CanTp_fake.h でキャプチャ）
 *          へ渡された応答を検証する」ブラックボックステスト方式。
 *
 *          0x85 は extendedSession 限定のため、SendExtendedSession() で
 *          事前にセッションを遷移させてから各テストを実行する。
 *          Dem_ReportErrorStatus() は Dem.c が本 env に実体でリンクされている
 *          ため直接呼び出し、DTC 記録の有効/無効が実際に Dem_GetStatusOfEvent()
 *          へ反映されるか（またはされないか）を確認する。
 */
#include <gtest/gtest.h>

extern "C" {
#include "Dcm.h"
#include "Dcm_Cfg.h"
#include "Dem.h"
#include "Dem_Cfg.h"
#include "CanTp_fake.h"
#include "Hal_Millis_fake.h"
#include "Hal_Det_Hw_fake.h"
}

namespace
{

class Bsw_Dcm_ControlDTCSetting_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        FakeCanTp_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制

        Dem_Init(NULL);
        Dcm_Init(NULL);

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

    /** [0x10, 0x03] extendedDiagnosticSession へ遷移する（0x85 の前提）。 */
    void EnterExtendedSession()
    {
        uint8 req[2] = { DCM_SID_SESSION_CTRL, DCM_SESSION_EXTENDED };
        Send(req, sizeof(req));
        ASSERT_EQ(FakeCanTp_TxBuf[0], 0x50U);  // 正応答確認（前提が崩れていないこと）
        FakeCanTp_Reset();
    }

    /** [0x85, subFunc] を送る。 */
    void SendControlDTCSetting(uint8 subFunc)
    {
        uint8 req[2] = { DCM_SID_CONTROL_DTC_SETTING, subFunc };
        Send(req, sizeof(req));
    }

    /** DEM_EVENT_CAN_BUSOFF (DEM_DEBOUNCE_LIMIT_CAN_BUSOFF=1、1回の報告で
     *  即確定) を FAILED 報告する。DTC 記録が有効なら testFailed/confirmedDTC
     *  ビットが立つはず。 */
    static void ReportBusOffFailed()
    {
        Dem_ReportErrorStatus(DEM_EVENT_CAN_BUSOFF, DEM_EVENT_STATUS_FAILED);
    }

    static uint8 BusOffStatus()
    {
        return Dem_GetStatusOfEvent(DEM_EVENT_CAN_BUSOFF);
    }
};

// ------------------------------------------------------------
// 正常系: on/off の受理と応答
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_ControlDTCSetting_Test, ControlDTCSetting_OK_OffIsAcceptedWithPositiveResponse)
{
    EnterExtendedSession();

    SendControlDTCSetting(DCM_DTCSETTING_OFF);

    /* 評価 (Assert): [0xC5, 0x02] */
    ASSERT_EQ(FakeCanTp_TransmitCount, 1U);
    ASSERT_EQ(FakeCanTp_TxLength, 2U);
    EXPECT_EQ(FakeCanTp_TxBuf[0], 0xC5U);
    EXPECT_EQ(FakeCanTp_TxBuf[1], DCM_DTCSETTING_OFF);
}

TEST_F(Bsw_Dcm_ControlDTCSetting_Test, ControlDTCSetting_OK_OnIsAcceptedWithPositiveResponse)
{
    EnterExtendedSession();

    SendControlDTCSetting(DCM_DTCSETTING_ON);

    /* 評価 (Assert): [0xC5, 0x01] */
    ASSERT_EQ(FakeCanTp_TransmitCount, 1U);
    ASSERT_EQ(FakeCanTp_TxLength, 2U);
    EXPECT_EQ(FakeCanTp_TxBuf[0], 0xC5U);
    EXPECT_EQ(FakeCanTp_TxBuf[1], DCM_DTCSETTING_ON);
}

// ------------------------------------------------------------
// DTC 記録の有効/無効が Dem へ実際に反映されること（本機能の核心）
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_ControlDTCSetting_Test, ControlDTCSetting_OK_OffSuppressesDtcRecordingUntilOn)
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
    FakeCanTp_Reset();
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

TEST_F(Bsw_Dcm_ControlDTCSetting_Test, ControlDTCSetting_OK_AutoReEnablesOnExplicitDefaultSessionRequest)
{
    EnterExtendedSession();
    SendControlDTCSetting(DCM_DTCSETTING_OFF);

    /* 実行 (Act): [0x10, 0x01] defaultSession へ明示的に戻る */
    FakeCanTp_Reset();
    uint8 req[2] = { DCM_SID_SESSION_CTRL, DCM_SESSION_DEFAULT };
    Send(req, sizeof(req));
    ASSERT_EQ(FakeCanTp_TxBuf[0], 0x50U);  // 正応答確認

    /* 評価 (Assert): 明示的に on を送っていないにも関わらず、defaultSession
     * への遷移だけで自動的に記録が再開される。 */
    ReportBusOffFailed();
    EXPECT_NE(BusOffStatus() & DEM_STATUS_TEST_FAILED, 0U);
}

TEST_F(Bsw_Dcm_ControlDTCSetting_Test, ControlDTCSetting_OK_AutoReEnablesOnS3Timeout)
{
    EnterExtendedSession();
    SendControlDTCSetting(DCM_DTCSETTING_OFF);

    /* 実行 (Act): S3 タイムアウトで defaultSession へ自動遷移させる
     * (明示的な 0x10 要求を送らない経路)。 */
    FakeMillis_Value += DCM_S3_TIMEOUT_MS + 1UL;
    Dcm_MainFunction();

    /* 評価 (Assert): S3 タイムアウト経由でも自動的に記録が再開される。 */
    ReportBusOffFailed();
    EXPECT_NE(BusOffStatus() & DEM_STATUS_TEST_FAILED, 0U);
}

TEST_F(Bsw_Dcm_ControlDTCSetting_Test, ControlDTCSetting_OK_AutoReEnablesAfterEcuReset)
{
    EnterExtendedSession();
    SendControlDTCSetting(DCM_DTCSETTING_OFF);

    /* 実行 (Act): [0x11, 0x01] hardReset（本実装は実際のリセットは行わず
     * セッションを defaultSession へ戻すのみ）。 */
    FakeCanTp_Reset();
    uint8 req[2] = { DCM_SID_ECU_RESET, 0x01U };
    Send(req, sizeof(req));
    ASSERT_EQ(FakeCanTp_TxBuf[0], 0x51U);  // 正応答確認

    /* 評価 (Assert): ECUReset 経由でも自動的に記録が再開される。 */
    ReportBusOffFailed();
    EXPECT_NE(BusOffStatus() & DEM_STATUS_TEST_FAILED, 0U);
}

TEST_F(Bsw_Dcm_ControlDTCSetting_Test, ControlDTCSetting_NG_DoesNotReEnableWhenNotDisabled)
{
    /* 準備 (Arrange): 一度も off にしていない状態で defaultSession へ戻る。
     * Dcm_DTCSettingDisabled が立っていないため Dem_EnableDTCSetting() は
     * 呼ばれないはずだが、既に有効なので外部から見た挙動に差はない
     * （冗長呼び出しでないことのみ、記録が有効なままであることで間接確認）。 */
    EnterExtendedSession();
    uint8 req[2] = { DCM_SID_SESSION_CTRL, DCM_SESSION_DEFAULT };
    Send(req, sizeof(req));

    ReportBusOffFailed();
    EXPECT_NE(BusOffStatus() & DEM_STATUS_TEST_FAILED, 0U);
}

// ------------------------------------------------------------
// 異常系
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_ControlDTCSetting_Test, ControlDTCSetting_NG_DefaultSessionRejectsWithNrc7F)
{
    /* 準備 (Arrange): Dcm_Init() 直後は defaultSession のまま */

    /* 実行 (Act) */
    SendControlDTCSetting(DCM_DTCSETTING_OFF);

    /* 評価 (Assert): [0x7F, 0x85, 0x7F serviceNotSupportedInActiveSession] */
    ASSERT_EQ(FakeCanTp_TransmitCount, 1U);
    ASSERT_EQ(FakeCanTp_TxLength, 3U);
    EXPECT_EQ(FakeCanTp_TxBuf[0], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(FakeCanTp_TxBuf[1], DCM_SID_CONTROL_DTC_SETTING);
    EXPECT_EQ(FakeCanTp_TxBuf[2], DCM_NRC_SERVICE_NOT_SUPPORTED_IN_SESSION);

    /* 記録も無効化されていないことを確認する（拒否された要求が副作用を
     * 持たないこと）。 */
    ReportBusOffFailed();
    EXPECT_NE(BusOffStatus() & DEM_STATUS_TEST_FAILED, 0U);
}

TEST_F(Bsw_Dcm_ControlDTCSetting_Test, ControlDTCSetting_NG_UnsupportedSubFuncReturnsNegativeResponse)
{
    EnterExtendedSession();

    /* 実行 (Act): 0x01/0x02 以外のサブ機能 */
    SendControlDTCSetting(0x03U);

    /* 評価 (Assert): [0x7F, 0x85, 0x12 subFunctionNotSupported] */
    ASSERT_EQ(FakeCanTp_TransmitCount, 1U);
    ASSERT_EQ(FakeCanTp_TxLength, 3U);
    EXPECT_EQ(FakeCanTp_TxBuf[0], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(FakeCanTp_TxBuf[2], DCM_NRC_SUB_FUNC_NOT_SUPPORTED);
}

TEST_F(Bsw_Dcm_ControlDTCSetting_Test, ControlDTCSetting_NG_ExtraOptionRecordReturnsIncorrectLength)
{
    EnterExtendedSession();

    /* 準備 (Arrange): [SWS_Dcm_01399] 相当。DTCSettingControlOptionRecord
     * (0xFFFFFF 以外) を付けて送る = udsLen が 2 を超える。 */
    uint8 req[5] = { DCM_SID_CONTROL_DTC_SETTING, DCM_DTCSETTING_OFF, 0x12U, 0x34U, 0x56U };

    /* 実行 (Act) */
    Send(req, sizeof(req));

    /* 評価 (Assert): [0x7F, 0x85, 0x13 incorrectMessageLength] */
    ASSERT_EQ(FakeCanTp_TransmitCount, 1U);
    ASSERT_EQ(FakeCanTp_TxLength, 3U);
    EXPECT_EQ(FakeCanTp_TxBuf[0], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(FakeCanTp_TxBuf[2], DCM_NRC_INCORRECT_MESSAGE_LENGTH);
}

}  // namespace
