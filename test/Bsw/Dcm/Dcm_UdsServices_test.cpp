/**
 * \file    Dcm_UdsServices_test.cpp
 * \brief   Dcm の UDS サービスのエラー応答と一部の正常系の単体テスト
 * \details 生の UDS 要求を Dcm へ直接渡し（WrapDcm_InjectRequest()）、CanTp へ渡された応答を確認する。ECUReset の未対応サブ機能、ReadData/WriteData/IoControl の
 *          対応しない DID、RequestDownload のアドレス・サイズの長さと上限、RoutineControl（EngineHealthCheck）の合否判定を確かめる。
 *          センサ値は Fake_Rte.h の FakeRte_EngineSpeed/FakeRte_CoolantTemp で指定する。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Dcm.h"
#include "Dcm_Cfg.h"
#include "Dem.h"
#include "CanTp.h"
#include "Wrap_CanTp.h"
#include "Wrap_Dcm.h"
#include "Wrap_ComM.h"
#include "Fake_Rte.h"
#include "Fake_Millis.h"
#include "Fake_Det_Hw.h"
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


/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* Dcm の UDS サービス（SID 0x11・0x22・0x2E・0x2F・0x31・0x34） の単体テスト用フィクスチャ。
 * SetUp(): CanTp・Dem・Dcm を初期化し、CanTp への送信（応答）を LastData_CanTp_Transmit へ記録させる。通信管理（ComM など）は対象外にする。
 *          Send() は生の UDS 要求を Dcm へ直接渡す補助関数、ExpectNegative() は直近の応答が否定応答であることを確認する補助関数。
 *          UnlockSecurityAccessLevel1() は extendedSession へ遷移して SecurityAccess Level1 をアンロックする補助関数。
 * TearDown(): ComM と Fake_Rte の指定を元に戻す。 */
class Dcm_UdsServices_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        WrapCanTp_Reset();
        CaptureEnabled_CanTp_Transmit = 1U;  // 応答を LastData_CanTp_Transmit へ複製する
        Suppressed_ComM_DcmDiagnostic = 1U;  // 本テストは通信管理(ComM/CanSM/CanNm)が対象外
        FakeRte_Reset();
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
        FakeRte_Reset();
    }

    /** 生の UDS 要求を Dcm へ直接渡す（WrapDcm_InjectRequest()）。 */
    static void Send(const uint8* payload, uint8 len)
    {
        (void)WrapDcm_InjectRequest(payload, len);
    }

    /** 直近の応答が、否定応答 [0x7F, sid, nrc] であることを確認する。 */
    static void ExpectNegative(uint8 sid, uint8 nrc)
    {
        EXPECT_EQ(LastLength_CanTp_Transmit, 3U);
        EXPECT_EQ(LastData_CanTp_Transmit[0], DCM_SID_NEGATIVE_RESP);
        EXPECT_EQ(LastData_CanTp_Transmit[1], sid);
        EXPECT_EQ(LastData_CanTp_Transmit[2], nrc);
    }

    /** extendedSession へ遷移する。 */
    static void EnterExtendedSession()
    {
        const uint8 req[2] = { DCM_SID_SESSION_CTRL, DCM_SESSION_EXTENDED };
        Send(req, sizeof(req));
    }

    /** extendedSession へ遷移し、requestSeed→sendKey で SecurityAccess Level1 をアンロックする
     *  （鍵は本番コードと同じ `seed ^ DCM_SECURITY_KEY_MASK`）。成功を ASSERT し、最後に応答の記録を消す。 */
    void UnlockSecurityAccessLevel1()
    {
        EnterExtendedSession();
        const uint8 seedReq[2] = { DCM_SID_SECURITY_ACCESS, DCM_SEC_SUBFUNC_REQUEST_SEED };
        Send(seedReq, sizeof(seedReq));
        ASSERT_EQ(LastData_CanTp_Transmit[0], 0x67U);
        const uint16 seed = (uint16)(((uint16)LastData_CanTp_Transmit[2] << 8U) | (uint16)LastData_CanTp_Transmit[3]);
        const uint16 key  = (uint16)(seed ^ DCM_SECURITY_KEY_MASK);
        const uint8 keyReq[4] = { DCM_SID_SECURITY_ACCESS, DCM_SEC_SUBFUNC_SEND_KEY,
                                  (uint8)(key >> 8U), (uint8)(key & 0xFFU) };
        Send(keyReq, sizeof(keyReq));
        ASSERT_EQ(LastData_CanTp_Transmit[0], 0x67U) << "security unlock must succeed as a test precondition";
        WrapCanTp_Reset();
        CaptureEnabled_CanTp_Transmit = 1U;
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Dcm_UdsServices_Test, Dcm_EcuReset_NG_UnsupportedSubFunctionIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    const uint8 req[2] = { DCM_SID_ECU_RESET, 0x02U };   // hardReset(0x01)・softReset(0x03) 以外（keyOffOnReset）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_ECU_RESET, DCM_NRC_SUB_FUNC_NOT_SUPPORTED);
}

TEST_F(Dcm_UdsServices_Test, Dcm_ReadDataById_NG_UnknownDidIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    const uint8 req[3] = { DCM_SID_READ_DATA, 0xFFU, 0xFFU };   // 表に無い DID

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_READ_DATA, DCM_NRC_REQUEST_OUT_OF_RANGE);
}

TEST_F(Dcm_UdsServices_Test, Dcm_ReadDataById_NG_DidWithoutReadFunctionIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    const uint8 req[3] = { DCM_SID_READ_DATA, (uint8)(DCM_DID_RUN_LAMP >> 8U), (uint8)(DCM_DID_RUN_LAMP & 0xFFU) };   // 0x2F 専用で、読み出し関数を持たない DID

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_READ_DATA, DCM_NRC_REQUEST_OUT_OF_RANGE);
}

TEST_F(Dcm_UdsServices_Test, Dcm_WriteDataById_NG_DidWithoutWriteFunctionIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    UnlockSecurityAccessLevel1();
    const uint8 req[4] = { DCM_SID_WRITE_DATA, (uint8)(DCM_DID_VIN >> 8U), (uint8)(DCM_DID_VIN & 0xFFU), 0x00U };   // 読み取り専用の DID

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_WRITE_DATA, DCM_NRC_REQUEST_OUT_OF_RANGE);
}

TEST_F(Dcm_UdsServices_Test, Dcm_IoControl_NG_DidWithoutIoControlIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();
    const uint8 req[4] = { DCM_SID_IO_CONTROL, (uint8)(DCM_DID_ENGINE_SPEED >> 8U), (uint8)(DCM_DID_ENGINE_SPEED & 0xFFU), DCM_IOCTRL_RETURN_CONTROL_TO_ECU };   // ランプの出力制御ではない DID

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_IO_CONTROL, DCM_NRC_REQUEST_OUT_OF_RANGE);
}

TEST_F(Dcm_UdsServices_Test, Dcm_RequestDownload_NG_AddressLengthBelowMinIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    UnlockSecurityAccessLevel1();
    const uint8 req[3] = { DCM_SID_REQUEST_DOWNLOAD, DCM_TRANSFER_DATA_FORMAT_RAW, 0x10U };   // アドレスの長さが 0 バイト（下限 1 未満）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_REQUEST_DOWNLOAD, DCM_NRC_REQUEST_OUT_OF_RANGE);
}

TEST_F(Dcm_UdsServices_Test, Dcm_RequestDownload_NG_AddressLengthAboveMaxIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    UnlockSecurityAccessLevel1();
    const uint8 req[3] = { DCM_SID_REQUEST_DOWNLOAD, DCM_TRANSFER_DATA_FORMAT_RAW, 0x15U };   // アドレスの長さが 5 バイト（上限 4 超え）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_REQUEST_DOWNLOAD, DCM_NRC_REQUEST_OUT_OF_RANGE);
}

TEST_F(Dcm_UdsServices_Test, Dcm_RequestDownload_NG_SizeLengthBelowMinIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    UnlockSecurityAccessLevel1();
    const uint8 req[3] = { DCM_SID_REQUEST_DOWNLOAD, DCM_TRANSFER_DATA_FORMAT_RAW, 0x01U };   // サイズの長さが 0 バイト（下限 1 未満）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_REQUEST_DOWNLOAD, DCM_NRC_REQUEST_OUT_OF_RANGE);
}

TEST_F(Dcm_UdsServices_Test, Dcm_RequestDownload_NG_SizeLengthAboveMaxIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    UnlockSecurityAccessLevel1();
    const uint8 req[3] = { DCM_SID_REQUEST_DOWNLOAD, DCM_TRANSFER_DATA_FORMAT_RAW, 0x51U };   // サイズの長さが 5 バイト（上限 4 超え）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_REQUEST_DOWNLOAD, DCM_NRC_REQUEST_OUT_OF_RANGE);
}

TEST_F(Dcm_UdsServices_Test, Dcm_RequestDownload_NG_MemorySizeZeroIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    UnlockSecurityAccessLevel1();
    const uint8 req[5] = { DCM_SID_REQUEST_DOWNLOAD, DCM_TRANSFER_DATA_FORMAT_RAW, 0x11U, 0x00U, 0x00U };   // アドレス 1 バイト、サイズ 1 バイト = 0

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_REQUEST_DOWNLOAD, DCM_NRC_REQUEST_OUT_OF_RANGE);
}

TEST_F(Dcm_UdsServices_Test, Dcm_RequestDownload_NG_MemorySizeAboveMaxIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    UnlockSecurityAccessLevel1();
    const uint16 tooLarge = (uint16)(DCM_TRANSFER_MAX_SIZE + 1UL);
    const uint8 req[7] = { DCM_SID_REQUEST_DOWNLOAD, DCM_TRANSFER_DATA_FORMAT_RAW, 0x22U, 0x00U, 0x00U,
                           (uint8)(tooLarge >> 8U), (uint8)(tooLarge & 0xFFU) };   // アドレス 2 バイト、サイズ 2 バイト = 上限 + 1

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_REQUEST_DOWNLOAD, DCM_NRC_REQUEST_OUT_OF_RANGE);
}

TEST_F(Dcm_UdsServices_Test, Dcm_IoControl_OK_ShortTermAdjustmentToOffIsAccepted)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();
    const uint8 req[5] = { DCM_SID_IO_CONTROL, (uint8)(DCM_DID_RUN_LAMP >> 8U), (uint8)(DCM_DID_RUN_LAMP & 0xFFU), DCM_IOCTRL_SHORT_TERM_ADJUSTMENT, 0x00U };   // 消灯（0）を指定

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(LastLength_CanTp_Transmit, 5U);
    EXPECT_EQ(LastData_CanTp_Transmit[0], 0x6FU);
    EXPECT_EQ(LastData_CanTp_Transmit[3], DCM_IOCTRL_SHORT_TERM_ADJUSTMENT);
    EXPECT_EQ(LastData_CanTp_Transmit[4], 0x00U);   // 指定した消灯のレベルを返す
}

TEST_F(Dcm_UdsServices_Test, Dcm_RoutineControl_NG_EngineSpeedAboveLimitFailsHealthCheck)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();
    FakeRte_EngineSpeed = DCM_ROUTINE_MAX_ENGINE_SPEED + 1U;
    const uint8 startReq[4] = { DCM_SID_ROUTINE_CONTROL, DCM_ROUTINE_SUBFUNC_START, (uint8)(DCM_RID_ENGINE_HEALTH_CHECK >> 8U), (uint8)(DCM_RID_ENGINE_HEALTH_CHECK & 0xFFU) };
    Send(startReq, sizeof(startReq));
    const uint8 resultReq[4] = { DCM_SID_ROUTINE_CONTROL, DCM_ROUTINE_SUBFUNC_REQUEST_RESULTS, (uint8)(DCM_RID_ENGINE_HEALTH_CHECK >> 8U), (uint8)(DCM_RID_ENGINE_HEALTH_CHECK & 0xFFU) };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    FakeMillis_Value += DCM_ROUTINE_DURATION_MS;   // 実行時間が経過する
    Dcm_MainFunction();
    Send(resultReq, sizeof(resultReq));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(LastLength_CanTp_Transmit, 6U);
    EXPECT_EQ(LastData_CanTp_Transmit[0], 0x71U);
    EXPECT_EQ(LastData_CanTp_Transmit[4], DCM_ROUTINE_RESULT_COMPLETED);
    EXPECT_EQ(LastData_CanTp_Transmit[5], DCM_ROUTINE_FAIL);   // 回転数が上限を超えている
}

TEST_F(Dcm_UdsServices_Test, Dcm_RoutineControl_NG_CoolantTempAboveLimitFailsHealthCheck)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();
    FakeRte_CoolantTemp = DCM_ROUTINE_MAX_COOLANT_TEMP + 1U;
    const uint8 startReq[4] = { DCM_SID_ROUTINE_CONTROL, DCM_ROUTINE_SUBFUNC_START, (uint8)(DCM_RID_ENGINE_HEALTH_CHECK >> 8U), (uint8)(DCM_RID_ENGINE_HEALTH_CHECK & 0xFFU) };
    Send(startReq, sizeof(startReq));
    const uint8 resultReq[4] = { DCM_SID_ROUTINE_CONTROL, DCM_ROUTINE_SUBFUNC_REQUEST_RESULTS, (uint8)(DCM_RID_ENGINE_HEALTH_CHECK >> 8U), (uint8)(DCM_RID_ENGINE_HEALTH_CHECK & 0xFFU) };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    FakeMillis_Value += DCM_ROUTINE_DURATION_MS;   // 実行時間が経過する
    Dcm_MainFunction();
    Send(resultReq, sizeof(resultReq));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(LastLength_CanTp_Transmit, 6U);
    EXPECT_EQ(LastData_CanTp_Transmit[0], 0x71U);
    EXPECT_EQ(LastData_CanTp_Transmit[4], DCM_ROUTINE_RESULT_COMPLETED);
    EXPECT_EQ(LastData_CanTp_Transmit[5], DCM_ROUTINE_FAIL);   // 冷却水温が上限を超えている
}

TEST_F(Dcm_UdsServices_Test, Dcm_RoutineControl_OK_ValuesWithinLimitsPassHealthCheck)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();
    FakeRte_EngineSpeed = DCM_ROUTINE_MAX_ENGINE_SPEED;
    FakeRte_CoolantTemp = DCM_ROUTINE_MAX_COOLANT_TEMP;
    const uint8 startReq[4] = { DCM_SID_ROUTINE_CONTROL, DCM_ROUTINE_SUBFUNC_START, (uint8)(DCM_RID_ENGINE_HEALTH_CHECK >> 8U), (uint8)(DCM_RID_ENGINE_HEALTH_CHECK & 0xFFU) };
    Send(startReq, sizeof(startReq));
    const uint8 resultReq[4] = { DCM_SID_ROUTINE_CONTROL, DCM_ROUTINE_SUBFUNC_REQUEST_RESULTS, (uint8)(DCM_RID_ENGINE_HEALTH_CHECK >> 8U), (uint8)(DCM_RID_ENGINE_HEALTH_CHECK & 0xFFU) };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    FakeMillis_Value += DCM_ROUTINE_DURATION_MS;   // 実行時間が経過する
    Dcm_MainFunction();
    Send(resultReq, sizeof(resultReq));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(LastLength_CanTp_Transmit, 6U);
    EXPECT_EQ(LastData_CanTp_Transmit[0], 0x71U);
    EXPECT_EQ(LastData_CanTp_Transmit[4], DCM_ROUTINE_RESULT_COMPLETED);
    EXPECT_EQ(LastData_CanTp_Transmit[5], DCM_ROUTINE_PASS);   // 上限ちょうどは合格
}

TEST_F(Dcm_UdsServices_Test, Dcm_CopyTxData_OK_NonRetryStateContinuesFromCurrentPosition)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    /* CanTp への送信を成功したことにし、確認（Dcm_TpTxConfirmation）は返さない。Dcm は応答（20 バイト）の送信が進行中の状態で止まる。 */
    FailFromCallCount_CanTp_Transmit    = 0U;
    ForcedReturn_CanTp_Transmit         = E_OK;
    SuppressConfirmation_CanTp_Transmit = 1U;
    const uint8 req[3] = { DCM_SID_READ_DATA, (uint8)(DCM_DID_VIN >> 8U), (uint8)(DCM_DID_VIN & 0xFFU) };
    Send(req, sizeof(req));
    uint8 buf[7] = { 0U };
    PduInfoType info = { buf, 7U };
    const RetryInfoType retry = { TP_DATACONF, 5U };   // 再送ではない（送信済みの確認）。再送の指示なら戻る量（5）は無視される
    PduLengthType available = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const BufReq_ReturnType ret = Dcm_CopyTxData(0U, &info, &retry, &available);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, BUFREQ_OK);                    // 再送の指示ではないので、送信位置は戻さない（まだ 0 なので、戻そうとすれば拒否される）
    EXPECT_EQ(buf[0], 0x62U);                     // 応答の先頭（正応答の SID）からコピーする
    EXPECT_EQ(available, 13U);                    // 応答 20 バイトのうち、残りは 13 バイト
}

TEST_F(Dcm_UdsServices_Test, Dcm_RoutineControl_OK_ResultsRequestedBeforeDurationReportsRunning)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();
    const uint8 startReq[4] = { DCM_SID_ROUTINE_CONTROL, DCM_ROUTINE_SUBFUNC_START, (uint8)(DCM_RID_ENGINE_HEALTH_CHECK >> 8U), (uint8)(DCM_RID_ENGINE_HEALTH_CHECK & 0xFFU) };
    Send(startReq, sizeof(startReq));
    const uint8 resultReq[4] = { DCM_SID_ROUTINE_CONTROL, DCM_ROUTINE_SUBFUNC_REQUEST_RESULTS, (uint8)(DCM_RID_ENGINE_HEALTH_CHECK >> 8U), (uint8)(DCM_RID_ENGINE_HEALTH_CHECK & 0xFFU) };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    FakeMillis_Value += DCM_ROUTINE_DURATION_MS - 1UL;   // 実行時間にまだ達していない
    Dcm_MainFunction();
    Send(resultReq, sizeof(resultReq));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(LastLength_CanTp_Transmit, 5U);
    EXPECT_EQ(LastData_CanTp_Transmit[0], 0x71U);
    EXPECT_EQ(LastData_CanTp_Transmit[4], DCM_ROUTINE_RESULT_RUNNING);   // まだ実行中（合否は未確定）
}

}  // namespace
