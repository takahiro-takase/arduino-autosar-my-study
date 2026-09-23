/**
 * \file    Bsw_Dcm_ReadDtcInfo_test.cpp
 * \brief   UDS SID 0x19 ReadDTCInformation の単体テスト（GoogleTest /
 *          PlatformIO `[env:native_chain]`。2026-08新設時は専用環境`[env:native_dcm]`だったが、2026-09にnative_chainへ統合した）。
 *
 * \details 本プロジェクトで Dcm.c/Dem.c を対象とする初めてのユニット
 *          テスト。
 *          GitHub Issue #122（subFunc 0x0A reportSupportedDTC の追加要望）
 *          への対応をきっかけに新設した。
 *
 *          `Dcm_ComIndication()` に生の UDS バイト列を直接渡し、
 *          `CanTp_Transmit()`（`Wrap_CanTp.h` で応答ペイロードをキャプチャ）
 *          へ渡された応答を検証する、という「入口と出口だけを見る」
 *          ブラックボックステスト。`Dcm_ComIndication()` 自体は
 *          「CanTp が組み立てた生 UDS ペイロードを受け取る」入口のため、
 *          RX 側の CanTp/PduR/CanIf/Can は経由しない。TX 側は CanTp.c が
 *          実体でリンクされる（2026-09-22、Fake_CanTp.c から切り替え）が、
 *          その先の PduR/CanIf/Can は本 env では未初期化のため、物理送信は
 *          （テストの関心事ではなく）静かに失敗する。
 *
 *          既存 subFunc（0x01/0x02/0x04/0x06）は今回追加した 0x0A との
 *          対比・将来の回帰検知のため最小限のみカバーする。全 UDS サービス
 *          の網羅は本ファイルのスコープ外。
 */
#include <gtest/gtest.h>

extern "C" {
#include "Dcm.h"
#include "Dcm_Cfg.h"
#include "Dem.h"
#include "Wrap_CanTp.h"
#include "Fake_Millis.h"
#include "Fake_Det_Hw.h"
#include "Wrap_ComM.h"
}

namespace
{

class Bsw_Dcm_ReadDtcInfo_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        WrapCanTp_Reset();
        Suppressed_ComM_DcmDiagnostic = 1U;  // 本テストは通信管理(ComM/CanSM/Nm)が対象外
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制

        CanTp_Init(NULL);
        Dem_Init(NULL);
        Dcm_Init(NULL);

        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        WrapComM_Reset();  // 他のテストファイルへ影響を残さない
    }

    /** [0x19, subFunc, ...] を組み立てて Dcm_ComIndication() へ直接渡す。 */
    void SendReadDtcInfo(const uint8* payload, uint8 len)
    {
        PduInfoType pdu = { const_cast<uint8*>(payload), len };
        Dcm_ComIndication(0U, &pdu);
    }

    /** extendedSessionへ遷移し、requestSeed→sendKeyで実際にSecurityAccess
     *  Level1をUnlockする（seed/keyの計算式は本番コードと同じ
     *  `seed ^ DCM_SECURITY_KEY_MASK`）。Unlock成功をASSERTし、以降の
     *  応答を検証しやすいよう最後にWrapCanTp_Reset()する。 */
    void UnlockSecurityAccessLevel1()
    {
        uint8 sessionReq[2] = { DCM_SID_SESSION_CTRL, DCM_SESSION_EXTENDED };
        PduInfoType sessionPdu = { sessionReq, sizeof(sessionReq) };
        Dcm_ComIndication(0U, &sessionPdu);

        uint8 seedReq[2] = { DCM_SID_SECURITY_ACCESS, DCM_SEC_SUBFUNC_REQUEST_SEED };
        PduInfoType seedPdu = { seedReq, sizeof(seedReq) };
        Dcm_ComIndication(0U, &seedPdu);
        ASSERT_EQ(LastData_CanTp_Transmit[0], 0x67U);
        uint16 seed = (uint16)(((uint16)LastData_CanTp_Transmit[2] << 8U) | (uint16)LastData_CanTp_Transmit[3]);
        uint16 key  = (uint16)(seed ^ DCM_SECURITY_KEY_MASK);

        uint8 keyReq[4] = { DCM_SID_SECURITY_ACCESS, DCM_SEC_SUBFUNC_SEND_KEY,
                             (uint8)(key >> 8U), (uint8)(key & 0xFFU) };
        PduInfoType keyPdu = { keyReq, sizeof(keyReq) };
        Dcm_ComIndication(0U, &keyPdu);
        ASSERT_EQ(LastData_CanTp_Transmit[0], 0x67U) << "security unlock must succeed as a test precondition";

        WrapCanTp_Reset();
    }
};

// ------------------------------------------------------------
// Dem_GetDTCStatusAvailabilityMask（Dcm.c が DEM_STATUS_AVAILABILITY_MASK に
// 直接アクセスしていたレイヤ違反を解消するために追加。上記 SID 0x19 各テストの
// TxBuf[2] 検証が間接的な回帰検知になっているため、ここでは API 自体の
// 直接呼び出しのみ検証する）
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, GetDTCStatusAvailabilityMask_OK_ReturnsConfiguredMask)
{
    uint8 mask = 0U;

    Std_ReturnType ret = Dem_GetDTCStatusAvailabilityMask(DCM_DEM_CLIENT_ID, &mask);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(mask, DEM_STATUS_AVAILABILITY_MASK);
}

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, GetDTCStatusAvailabilityMask_NG_NullPointerReturnsError)
{
    Std_ReturnType ret = Dem_GetDTCStatusAvailabilityMask(DCM_DEM_CLIENT_ID, NULL);

    EXPECT_EQ(ret, E_NOT_OK);
}

// ------------------------------------------------------------
// Dem_EnableDTCSetting/Dem_DisableDTCSetting（実仕様の ClientId 引数・
// Std_ReturnType 戻り値へシグネチャを合わせた際の直接呼び出し検証。
// UDS SID 0x85 経由の挙動は Bsw_Dcm_ControlDTCSetting_test.cpp が担当）
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, EnableDTCSetting_OK_ReturnsOk)
{
    EXPECT_EQ(Dem_EnableDTCSetting(DCM_DEM_CLIENT_ID), E_OK);
}

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, DisableDTCSetting_OK_ReturnsOk)
{
    EXPECT_EQ(Dem_DisableDTCSetting(DCM_DEM_CLIENT_ID), E_OK);
}

// ------------------------------------------------------------
// Dem_GetFaultDetectionCounter（UDS SID 0x19 subFunc 0x14
// reportDTCFaultDetectionCounter 用に新設。[SWS_Dem_00415]により内部の
// Dem_DebounceCounter[]（-limit〜+limit）を-128〜127へ線形写像して返す。
// DEM_EVENT_ENGINE_OVERHEAT の limit は 2 (DEM_DEBOUNCE_LIMIT_ENGINE_OVERHEAT)。
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, GetFaultDetectionCounter_OK_ReturnsZeroForFreshEvent)
{
    sint8 fdc = 0x7F;  /* 未更新を検出できる初期値 */

    Std_ReturnType ret = Dem_GetFaultDetectionCounter(DEM_EVENT_ENGINE_OVERHEAT, &fdc);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(fdc, 0);  /* カウンタ0の写像は0(正負どちらの式でも0) */
}

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, GetFaultDetectionCounter_OK_ReflectsDebounceCounterAfterFailedReport)
{
    (void)Dem_SetEventStatus(DEM_EVENT_ENGINE_OVERHEAT, DEM_EVENT_STATUS_FAILED);

    sint8 fdc = 0;
    Std_ReturnType ret = Dem_GetFaultDetectionCounter(DEM_EVENT_ENGINE_OVERHEAT, &fdc);

    EXPECT_EQ(ret, E_OK);
    /* 中立(0)から FAILED 方向へ1回分だけ進んだ生カウンタ1を、
     * limit=2 で線形写像: (1*127)/2 = 63 (整数除算)。 */
    EXPECT_EQ(fdc, 63);
}

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, GetFaultDetectionCounter_NG_InvalidEventIdReturnsError)
{
    sint8 fdc = 0;

    Std_ReturnType ret = Dem_GetFaultDetectionCounter((Dem_EventIdType)DEM_EVENT_COUNT, &fdc);

    EXPECT_EQ(ret, E_NOT_OK);
}

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, GetFaultDetectionCounter_NG_NullPointerReturnsError)
{
    Std_ReturnType ret = Dem_GetFaultDetectionCounter(DEM_EVENT_ENGINE_OVERHEAT, NULL);

    EXPECT_EQ(ret, E_NOT_OK);
}

// ------------------------------------------------------------
// Dem_GetDTCOfEvent（DTCFormat引数欠落の是正。本プロジェクトはUDS 3-byte
// 形式のDTCのみ構成しているため、DEM_DTC_FORMAT_UDS以外はDEM_E_NO_DTC_AVAILABLE
// で拒否する）
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, GetDTCOfEvent_OK_ReturnsUdsDtcWhenFormatIsUds)
{
    uint32 dtc = 0U;

    Std_ReturnType ret = Dem_GetDTCOfEvent(DEM_EVENT_ENGINE_OVERHEAT, DEM_DTC_FORMAT_UDS, &dtc);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(dtc, static_cast<uint32>(DEM_DTC_ENGINE_OVERHEAT));
}

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, GetDTCOfEvent_NG_ObdFormatReturnsNoDtcAvailable)
{
    uint32 dtc = 0U;

    Std_ReturnType ret = Dem_GetDTCOfEvent(DEM_EVENT_ENGINE_OVERHEAT, DEM_DTC_FORMAT_OBD, &dtc);

    EXPECT_EQ(ret, DEM_E_NO_DTC_AVAILABLE);
}

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, GetDTCOfEvent_NG_J1939FormatReturnsNoDtcAvailable)
{
    uint32 dtc = 0U;

    Std_ReturnType ret = Dem_GetDTCOfEvent(DEM_EVENT_ENGINE_OVERHEAT, DEM_DTC_FORMAT_J1939, &dtc);

    EXPECT_EQ(ret, DEM_E_NO_DTC_AVAILABLE);
}

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, GetDTCOfEvent_NG_InvalidEventIdReturnsError)
{
    uint32 dtc = 0U;

    Std_ReturnType ret = Dem_GetDTCOfEvent((Dem_EventIdType)DEM_EVENT_COUNT, DEM_DTC_FORMAT_UDS, &dtc);

    EXPECT_EQ(ret, E_NOT_OK);
}

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, GetDTCOfEvent_NG_NullPointerReturnsError)
{
    Std_ReturnType ret = Dem_GetDTCOfEvent(DEM_EVENT_ENGINE_OVERHEAT, DEM_DTC_FORMAT_UDS, NULL);

    EXPECT_EQ(ret, E_NOT_OK);
}

// ------------------------------------------------------------
// Dcm_GetVin / DID 0xF190（VIN読み出し新設。実仕様ではDcmが呼び出す側の
// 関数だが、本プロジェクトは固定値を返す簡略実装。Dcm_Init()が起動時に
// 一度だけ呼びキャッシュし、UDS SID 0x22経由の応答はそのキャッシュから
// 返す）
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, GetVin_OK_ReturnsFixedSeventeenByteVin)
{
    uint8 vin[DCM_VIN_LENGTH] = { 0 };

    Std_ReturnType ret = Dcm_GetVin(vin);

    EXPECT_EQ(ret, E_OK);
    /* 全バイトが書き換わっている（0x00埋めのまま残っていない）ことのみ検証。
     * 具体的な文字列内容は固定値の実装詳細のため固定しない。 */
    uint8 nonZeroCount = 0U;
    for (uint8 i = 0U; i < DCM_VIN_LENGTH; i++)
        if (vin[i] != 0U)
            nonZeroCount++;
    EXPECT_EQ(nonZeroCount, DCM_VIN_LENGTH);
}

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, GetVin_NG_NullPointerReturnsError)
{
    Std_ReturnType ret = Dcm_GetVin(NULL);

    EXPECT_EQ(ret, E_NOT_OK);
}

// ReadDataById_OK_VinReturnsSeventeenBytesMatchingDcmGetVin /
// ReadDataById_NG_TooShortRequestReturnsIncorrectMessageLength /
// ReadDataById_NG_MultipleDidRequestReturnsIncorrectMessageLength は
// Bsw_DcmStack_SID22_ReadDataById_test.cpp へ移植済み（2026-09、同内容
// のため削除）。

// ------------------------------------------------------------
// Dcm_GetSesCtrlType/Dcm_GetSecurityLevel（Dcm.c 内部のモジュール状態
// Dcm_CurrentSession/Dcm_SecurityLevel を読み出すだけの新規 getter API）
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, GetSesCtrlType_OK_ReturnsDefaultSessionAfterInit)
{
    Dcm_SesCtrlType session = 0xFFU;  /* 未更新を検出できる初期値 */

    Std_ReturnType ret = Dcm_GetSesCtrlType(&session);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(session, DCM_SESSION_DEFAULT);
}

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, GetSesCtrlType_OK_ReflectsExtendedSessionAfterRequest)
{
    uint8 req[2] = { DCM_SID_SESSION_CTRL, DCM_SESSION_EXTENDED };
    SendReadDtcInfo(req, sizeof(req));
    ASSERT_EQ(LastData_CanTp_Transmit[0], 0x50U);  // 正応答確認（前提が崩れていないこと）

    Dcm_SesCtrlType session = 0U;
    Std_ReturnType ret = Dcm_GetSesCtrlType(&session);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(session, DCM_SESSION_EXTENDED);
}

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, GetSesCtrlType_NG_NullPointerReturnsError)
{
    EXPECT_EQ(Dcm_GetSesCtrlType(NULL), E_NOT_OK);
}

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, GetSecurityLevel_OK_ReturnsLockedByDefault)
{
    Dcm_SecLevelType level = 0xFFU;  /* 未更新を検出できる初期値 */

    Std_ReturnType ret = Dcm_GetSecurityLevel(&level);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(level, 0U);  /* Locked */
}

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, GetSecurityLevel_NG_NullPointerReturnsError)
{
    EXPECT_EQ(Dcm_GetSecurityLevel(NULL), E_NOT_OK);
}

// ------------------------------------------------------------
// Dcm_GetActiveProtocol（[SWS_Dcm_00340]、2026-09-05 新設。単一プロトコル・
// 単一コネクション構成のため、固定値を返すだけの実装であることを検証する）
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, GetActiveProtocol_OK_ReturnsFixedUdsOnCanValues)
{
    Dcm_ProtocolType protocol = 0xFFU;
    uint16 connectionId       = 0xFFFFU;
    uint16 testerAddress      = 0xFFFFU;

    Std_ReturnType ret = Dcm_GetActiveProtocol(&protocol, &connectionId, &testerAddress);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(protocol, DCM_UDS_ON_CAN);
    EXPECT_EQ(connectionId, DCM_CONNECTION_ID);
    EXPECT_EQ(testerAddress, DCM_TESTER_SOURCE_ADDRESS);
}

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, GetActiveProtocol_NG_NullActiveProtocolTypeReturnsError)
{
    uint16 connectionId  = 0U;
    uint16 testerAddress = 0U;

    EXPECT_EQ(Dcm_GetActiveProtocol(NULL, &connectionId, &testerAddress), E_NOT_OK);
}

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, GetActiveProtocol_NG_NullConnectionIdReturnsError)
{
    Dcm_ProtocolType protocol = 0U;
    uint16 testerAddress      = 0U;

    EXPECT_EQ(Dcm_GetActiveProtocol(&protocol, NULL, &testerAddress), E_NOT_OK);
}

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, GetActiveProtocol_NG_NullTesterSourceAddressReturnsError)
{
    Dcm_ProtocolType protocol = 0U;
    uint16 connectionId       = 0U;

    EXPECT_EQ(Dcm_GetActiveProtocol(&protocol, &connectionId, NULL), E_NOT_OK);
}

// ------------------------------------------------------------
// Dcm_ResetToDefaultSession（[SWS_Dcm_00520]。既存の3箇所（明示的な0x10
// defaultSession要求・S3タイムアウト・0x11 ECUReset後）に重複していた
// セッションリセット処理列を集約した新規公開API。3箇所からの呼び出しの
// 回帰は Bsw_Dcm_ControlDTCSetting_test.cpp の AutoReEnables* 系テストが
// 引き続き担当する。ここでは API 自体の直接呼び出しのみ検証する）
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, ResetToDefaultSession_OK_ReturnsSessionToDefault)
{
    uint8 req[2] = { DCM_SID_SESSION_CTRL, DCM_SESSION_EXTENDED };
    SendReadDtcInfo(req, sizeof(req));
    ASSERT_EQ(LastData_CanTp_Transmit[0], 0x50U);  // 前提: extendedSessionへ遷移済み
    Dcm_SesCtrlType sessionBefore = 0U;
    ASSERT_EQ(Dcm_GetSesCtrlType(&sessionBefore), E_OK);
    ASSERT_EQ(sessionBefore, DCM_SESSION_EXTENDED);

    Std_ReturnType ret = Dcm_ResetToDefaultSession();

    Dcm_SesCtrlType sessionAfter = 0xFFU;  /* 未更新を検出できる初期値 */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(Dcm_GetSesCtrlType(&sessionAfter), E_OK);
    EXPECT_EQ(sessionAfter, DCM_SESSION_DEFAULT);
}

// ------------------------------------------------------------
// subFunc 0x0A/0x14/0x01 の OK シナリオは、物理層 Can_Hw までの検証を含む
// Bsw_DcmStack_SID19_SF0A_ReadDtcSupported_test.cpp /
// Bsw_DcmStack_SID19_SF14_ReadDtcFaultDetectionCounter_test.cpp /
// Bsw_DcmStack_SID19_SF01_ReadDtcCount_test.cpp へ移植済み（2026-09、
// 同内容のため本ファイルからは削除）。
// ------------------------------------------------------------

// ReadDtcInfo_NG_UnsupportedSubFuncReturnsNegativeResponse /
// ReadDtcInfo_NG_TooShortRequestReturnsNegativeResponse は
// Bsw_DcmStack_SID19_Dispatch_test.cpp へ移植済み（2026-09、同内容の
// ため削除）。

// ReadDtcCount_NG_TooShortRequestReturnsIncorrectMessageLength /
// ReadDtcByMask_NG_TooShortRequestReturnsIncorrectMessageLength は
// Bsw_DcmStack_SID19_SF01/SF02_*_test.cpp の NG シナリオへ移植済み
// （2026-09、同内容のため削除）。

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, ReadDtcCount_NG_ExtraByteReturnsIncorrectMessageLength)
{
    /* 準備 (Arrange): statusMask の後に余分な1バイト ([0x19, 0x01, mask, 0x00]、
     * 3バイト厳密一致のため上限超過。2026-09 追加: 以前は下限のみ判定していた
     * ため黙って受理していた） */
    uint8 req[4] = { DCM_SID_READ_DTC_INFO, DCM_DTC_SUBFUNC_REPORT_COUNT, 0x00U, 0x00U };

    SendReadDtcInfo(req, sizeof(req));

    ASSERT_EQ(CallCount_CanTp_Transmit, 1U);
    ASSERT_EQ(LastLength_CanTp_Transmit, 3U);
    EXPECT_EQ(LastData_CanTp_Transmit[0], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(LastData_CanTp_Transmit[2], DCM_NRC_INCORRECT_MESSAGE_LENGTH);
}

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, ReadDtcSupported_NG_ExtraByteReturnsIncorrectMessageLength)
{
    /* 準備 (Arrange): 追加パラメータなしの subFunc に余分な1バイト
     * ([0x19, 0x0A, 0x00]、2バイト厳密一致。2026-09 追加: 以前は udsLen を
     * 一切見ておらず何バイト付けても黙って受理していた） */
    uint8 req[3] = { DCM_SID_READ_DTC_INFO, DCM_DTC_SUBFUNC_REPORT_SUPPORTED, 0x00U };

    SendReadDtcInfo(req, sizeof(req));

    ASSERT_EQ(CallCount_CanTp_Transmit, 1U);
    ASSERT_EQ(LastLength_CanTp_Transmit, 3U);
    EXPECT_EQ(LastData_CanTp_Transmit[0], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(LastData_CanTp_Transmit[2], DCM_NRC_INCORRECT_MESSAGE_LENGTH);
}

// ReadDtcSnapshot_NG_TooShortRequestReturnsIncorrectMessageLength /
// ReadDtcExtendedData_NG_TooShortRequestReturnsIncorrectMessageLength は
// Bsw_DcmStack_SID19_SF04/SF06_*_test.cpp の NG シナリオへ移植済み
// （2026-09、同内容のため削除）。

// ------------------------------------------------------------
// DTCSnapshotRecordNumber/DTCExtDataRecordNumber の 0xFF(全レコード要求)を
// 唯一のレコードへのエイリアスとして受理する是正(2026-09、[SWS_Dcm_00441])。
// 以前は 0x01 との厳密一致のみ受理し、0xFF は NRC 0x31 で拒否していた。
// OK シナリオ（recordNumber=0xFF が実レコード番号0x01と同じ応答になること）と
// ReadDtcExtendedData_NG_NeverFailedDtcReturnsRequestOutOfRange は
// Bsw_DcmStack_SID19_SF04/SF06_*_test.cpp へ移植済み（同内容のため削除）。
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, ReadDtcSnapshot_NG_UnsupportedRecordNumberStillRejected)
{
    /* 準備 (Arrange): 0x01/0xFF以外のrecordNumber(0x02)は依然として拒否される
     * ことを確認する(0xFF追加が「何でも受理」への後退でないことの回帰)。 */
    (void)Dem_SetEventStatus(DEM_EVENT_ENGINE_OVERHEAT, DEM_EVENT_STATUS_FAILED);

    uint8 req[6] = { DCM_SID_READ_DTC_INFO, DCM_DTC_SUBFUNC_REPORT_SNAPSHOT,
                      0x00U, 0x01U, 0x01U, 0x02U };
    SendReadDtcInfo(req, sizeof(req));

    ASSERT_EQ(CallCount_CanTp_Transmit, 1U);
    ASSERT_EQ(LastLength_CanTp_Transmit, 3U);
    EXPECT_EQ(LastData_CanTp_Transmit[0], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(LastData_CanTp_Transmit[2], DCM_NRC_REQUEST_OUT_OF_RANGE);
}

// SessionControl_NG_ExtraByteReturnsIncorrectMessageLength (SID 0x10) /
// EcuReset_NG_ExtraByteReturnsIncorrectMessageLength (SID 0x11) /
// TesterPresent_NG_ExtraByteReturnsIncorrectMessageLength (SID 0x3E) /
// ClearDtc_NG_ExtraByteReturnsIncorrectMessageLength (SID 0x14) /
// SecuritySendKey_NG_ExtraByteReturnsIncorrectMessageLength /
// SecurityRequestSeed_NG_ExtraByteReturnsIncorrectMessageLength (SID 0x27) は
// Bsw_DcmStack_SID10/SID11/SID3E/SID14/SID27_*_test.cpp へ移植済み
// （2026-09、同内容のため削除）。

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, SessionControl_OK_SuppressPosRspBitSuppressesPositiveResponse)
{
    /* 準備 (Arrange): subFunc の bit7 (suppressPosRspMsgIndicationBit) を立てた
     * [0x10, 0x80|DCM_SESSION_EXTENDED]（[SWS_Dcm_00200]/[SWS_Dcm_00201]。
     * 2026-09 追加: 以前は読み取って捨てるだけで実際には抑制していなかった）。
     * セッション自体は正常に遷移するはずだが、正応答フレームは一切送信され
     * ないことを確認する。 */
    uint8 req[2] = { DCM_SID_SESSION_CTRL, (uint8)(0x80U | DCM_SESSION_EXTENDED) };

    PduInfoType pdu = { req, sizeof(req) };
    Dcm_ComIndication(0U, &pdu);

    EXPECT_EQ(CallCount_CanTp_Transmit, 0U);

    /* セッション遷移自体は抑制されていないことを Dcm_GetSesCtrlType() で確認 */
    Dcm_SesCtrlType sesCtrlType;
    ASSERT_EQ(Dcm_GetSesCtrlType(&sesCtrlType), E_OK);
    EXPECT_EQ(sesCtrlType, DCM_SESSION_EXTENDED);
}

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, TesterPresent_OK_SuppressPosRspBitSuppressesPositiveResponse)
{
    /* 準備 (Arrange): zeroSubFunction の bit7 を立てた [0x3E, 0x80]。
     * TesterPresent は副作用が S3 タイマ更新のみのため、正応答が送信され
     * ないことだけを確認すればよい。 */
    uint8 req[2] = { DCM_SID_TESTER_PRESENT, 0x80U };

    PduInfoType pdu = { req, sizeof(req) };
    Dcm_ComIndication(0U, &pdu);

    EXPECT_EQ(CallCount_CanTp_Transmit, 0U);
}

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, EcuReset_OK_SuppressPosRspBitAcceptsHardResetWithoutTransmitting)
{
    /* 準備 (Arrange): [0x11, 0x80|DCM_RESET_HARD]（本タスクで見つけた実害バグの
     * 直接的な回帰テスト: 以前は bit7 を一切マスクせず生バイトのまま subFunc
     * として比較していたため、本ビットを立てただけで hardReset/softReset の
     * どちらとも不一致になり誤って NRC 0x12 subFunctionNotSupported を返して
     * いた。是正後は bit7 を無視して正しく hardReset と認識しつつ、正応答は
     * 抑制されることを確認する）。 */
    uint8 req[2] = { DCM_SID_ECU_RESET, (uint8)(0x80U | DCM_RESET_HARD) };

    PduInfoType pdu = { req, sizeof(req) };
    Dcm_ComIndication(0U, &pdu);

    EXPECT_EQ(CallCount_CanTp_Transmit, 0U);
}

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, SessionControl_NG_SuppressPosRspBitDoesNotSuppressNegativeResponse)
{
    /* 準備 (Arrange): bit7 を立てた不正サブ機能 [0x10, 0x80|0x02]（存在しない
     * subFunc=0x02）。否定応答は suppressPosRspMsgIndicationBit の対象外
     * （Dcm_SendNegativeResponse() は Dcm_SuppressPosRsp を一切見ない）ため、
     * bit7 が立っていても NRC は必ず送信されることを確認する。 */
    uint8 req[2] = { DCM_SID_SESSION_CTRL, 0x82U };

    PduInfoType pdu = { req, sizeof(req) };
    Dcm_ComIndication(0U, &pdu);

    ASSERT_EQ(CallCount_CanTp_Transmit, 1U);
    ASSERT_EQ(LastLength_CanTp_Transmit, 3U);
    EXPECT_EQ(LastData_CanTp_Transmit[0], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(LastData_CanTp_Transmit[1], DCM_SID_SESSION_CTRL);
    EXPECT_EQ(LastData_CanTp_Transmit[2], DCM_NRC_SUB_FUNC_NOT_SUPPORTED);
}

// ------------------------------------------------------------
// SecurityAccess再ロック漏れの是正(2026-09)。[SWS_Dcm_00139]は
// defaultSession以外からdefaultSession以外への遷移(現在アクティブな
// セッションへの再遷移を含む)でもセキュリティレベルを再ロックすべきと
// 規定するが、以前はdefaultSessionへの遷移時のみ再ロックしていた。
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, SessionControl_OK_ReselectingSameSessionRelocksSecurity)
{
    /* 準備 (Arrange): extendedSessionへ遷移し、requestSeed→sendKeyで実際に
     * Unlockする。 */
    UnlockSecurityAccessLevel1();

    Dcm_SecLevelType levelAfterUnlock = 0U;
    ASSERT_EQ(Dcm_GetSecurityLevel(&levelAfterUnlock), E_OK);
    ASSERT_NE(levelAfterUnlock, 0U) << "must be unlocked as a test precondition";

    /* 実行 (Act): 同じextendedSessionを再度選択する
     * ([0x10, 0x03]、現在アクティブなセッションへの再遷移)。 */
    uint8 sessionReq[2] = { DCM_SID_SESSION_CTRL, DCM_SESSION_EXTENDED };
    PduInfoType sessionPdu = { sessionReq, sizeof(sessionReq) };
    Dcm_ComIndication(0U, &sessionPdu);

    /* 評価 (Assert): [SWS_Dcm_00139]通りセキュリティレベルがLockedへ
     * 戻っていること。 */
    Dcm_SecLevelType levelAfterReselect = 0xFFU;
    ASSERT_EQ(Dcm_GetSecurityLevel(&levelAfterReselect), E_OK);
    EXPECT_EQ(levelAfterReselect, 0U) << "re-selecting the same session must re-lock security";
}

// ComIndication_NG_IgnoresRequestWhileCanTpTxBusy /
// ComIndication_OK_ProcessesRequestOnceCanTpTxIdleAgain は
// Bsw_DcmStack_CanTpBusy_test.cpp（実際のマルチフレーム送信中の
// 真のビジー状態を使う版）へ移植済み（2026-09、旧S3Timer系と統合）。

}  // namespace
