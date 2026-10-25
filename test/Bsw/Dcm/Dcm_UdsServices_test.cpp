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
#include "NvM_TestUtil.h"
#include "NvM.h"
#include "MemIf.h"
#include "Crypto.h"
#include "CryIf.h"
#include "Csm.h"
#include "KeyM.h"
#include "KeyM_Cfg.h"
#include "CanTp.h"
#include "Wrap_CanTp.h"
#include "Wrap_Dem.h"
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
        WrapDem_Reset();                     // 他のテストの Dem の故障注入（Dem_ClearDTC の戻り値など）を残さない
        CaptureEnabled_CanTp_Transmit = 1U;  // 応答を LastData_CanTp_Transmit へ複製する
        Suppressed_ComM_DcmDiagnostic = 1U;  // 本テストは通信管理(ComM/CanSM/CanNm)が対象外
        FakeRte_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制

        CanTp_Init(NULL);
        NvMTest_ColdBoot();   // 空の EEPROM から NvM を初期化する（Dcm_Init() が失敗回数を NvM から復元するため）
        Dem_Init(NULL);
        Dcm_Init(NULL);

        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        NvMTest_Shutdown();   // 次のテストへ NvM の初期化状態を残さない
        FakeDetHw_LogSuppressed = 1U;
        WrapComM_Reset();  // 他のテストファイルへ影響を残さない
        FakeRte_Reset();
        KeyM_Deinit();     // 鍵更新のテストが初期化した暗号系を、未初期化へ戻す（未初期化のときは何もしない）
        Crypto_Test_ResetInitState();
        CryIf_Test_ResetInitState();
        Csm_Test_ResetInitState();
    }

    /** 生の UDS 要求を Dcm へ直接渡す（WrapDcm_InjectRequest()）。 */
    static void Send(const uint8* payload, uint8 len)
    {
        (void)WrapDcm_InjectRequest(payload, len);
    }

    /** 鍵更新（0x2E 0x0108）が使う暗号系（Crypto・CryIf・Csm・KeyM）を初期化する。 */
    static void InitCryptoStack()
    {
        Crypto_Test_ResetInitState();
        CryIf_Test_ResetInitState();
        Csm_Test_ResetInitState();
        Crypto_Init();
        CryIf_Init();
        Csm_Init();
        KeyM_Init(NULL);
    }

    /** 鍵更新（0x2E 0x0108）の要求 [SID, DID(2), keyName, keyData(16)] を作って送る。 */
    static void SendKeyUpdate(uint8 keyName)
    {
        uint8 req[3U + DCM_DID_CRYPTO_KEY_UPDATE_LENGTH] = { DCM_SID_WRITE_DATA, (uint8)(DCM_DID_CRYPTO_KEY_UPDATE >> 8U), (uint8)(DCM_DID_CRYPTO_KEY_UPDATE & 0xFFU), keyName };
        for (uint8 i = 0U; i < 16U; i++) { req[4U + i] = (uint8)(0xA0U + i); }
        Send(req, sizeof(req));
    }

    /** SecurityAccess の requestSeed を送り、返ってきたシードを返す（正応答でなければ ASSERT 失敗）。 */
    static uint16 RequestSeed()
    {
        const uint8 seedReq[2] = { DCM_SID_SECURITY_ACCESS, DCM_SEC_SUBFUNC_REQUEST_SEED };
        Send(seedReq, sizeof(seedReq));
        EXPECT_EQ(LastData_CanTp_Transmit[0], 0x67U);
        return (uint16)(((uint16)LastData_CanTp_Transmit[2] << 8U) | (uint16)LastData_CanTp_Transmit[3]);
    }

    /** SecurityAccess の sendKey を送る。 */
    static void SendKey(uint16 key)
    {
        const uint8 keyReq[4] = { DCM_SID_SECURITY_ACCESS, DCM_SEC_SUBFUNC_SEND_KEY, (uint8)(key >> 8U), (uint8)(key & 0xFFU) };
        Send(keyReq, sizeof(keyReq));
    }

    /** requestSeed の後、間違った鍵（正しい鍵のビットを 1 つ反転）を送る。 */
    static void SendWrongKey()
    {
        const uint16 seed = RequestSeed();
        SendKey((uint16)((seed ^ DCM_SECURITY_KEY_MASK) ^ 0x0001U));
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
    const uint8 req[2] = { DCM_SID_ECU_RESET, 0x04U };   // hardReset(0x01)・keyOffOnReset(0x02)・softReset(0x03) 以外（enableRapidPowerShutdown）

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

// NG: DID の下位バイトが足りない（DID は 2 バイトずつ。長さが 1 + 2 × n でない）要求は NRC 0x13（[SWS_Dcm_01335]）。
TEST_F(Dcm_UdsServices_Test, Dcm_ReadDataById_NG_OddLengthRequestIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    const uint8 req[4] = { DCM_SID_READ_DATA, (uint8)(DCM_DID_ENGINE_SPEED >> 8U), (uint8)(DCM_DID_ENGINE_SPEED & 0xFFU), (uint8)(DCM_DID_COOLANT_TEMP >> 8U) };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_READ_DATA, DCM_NRC_INCORRECT_MESSAGE_LENGTH);
}

// NG: DID の数が DCM_MAX_DID_TO_READ を超える要求は NRC 0x13（[SWS_Dcm_01335]）。
TEST_F(Dcm_UdsServices_Test, Dcm_ReadDataById_NG_TooManyDidsIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 req[1U + (2U * (DCM_MAX_DID_TO_READ + 1U))] = { DCM_SID_READ_DATA };
    for (uint8 i = 0U; i < (DCM_MAX_DID_TO_READ + 1U); i++)
    {
        req[1U + (2U * i)] = (uint8)(DCM_DID_COOLANT_TEMP >> 8U);
        req[2U + (2U * i)] = (uint8)(DCM_DID_COOLANT_TEMP & 0xFFU);
    }

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_READ_DATA, DCM_NRC_INCORRECT_MESSAGE_LENGTH);
}

// NG: 要求した DID が全て未対応なら NRC 0x31（[SWS_Dcm_00438]）。
TEST_F(Dcm_UdsServices_Test, Dcm_ReadDataById_NG_AllDidsUnsupportedIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    const uint8 req[5] = { DCM_SID_READ_DATA, 0xFFU, 0xFFU, 0xFFU, 0xFEU };   // 表に無い DID が 2 つ

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_READ_DATA, DCM_NRC_REQUEST_OUT_OF_RANGE);
}

// NG: defaultSession で読めない DID（SECURE_INFO は extendedSession 限定）だけを要求すると、NRC 0x31（[SWS_Dcm_00434]）。
TEST_F(Dcm_UdsServices_Test, Dcm_ReadDataById_NG_DidNotReadableInCurrentSessionIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    const uint8 req[3] = { DCM_SID_READ_DATA, (uint8)(DCM_DID_SECURE_INFO >> 8U), (uint8)(DCM_DID_SECURE_INFO & 0xFFU) };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_READ_DATA, DCM_NRC_REQUEST_OUT_OF_RANGE);
}

// NG: extendedSession でも、SecurityAccess がロック中なら SECURE_INFO は NRC 0x33（[SWS_Dcm_00435]）。
TEST_F(Dcm_UdsServices_Test, Dcm_ReadDataById_NG_SecureDidWhileLockedIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();
    const uint8 req[3] = { DCM_SID_READ_DATA, (uint8)(DCM_DID_SECURE_INFO >> 8U), (uint8)(DCM_DID_SECURE_INFO & 0xFFU) };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_READ_DATA, DCM_NRC_SECURITY_ACCESS_DENIED);
}

// NG: ロック中の SECURE_INFO が 1 つでも含まれると、他の DID が読めても要求全体を NRC 0x33 で拒否する（[SWS_Dcm_00435]）。
TEST_F(Dcm_UdsServices_Test, Dcm_ReadDataById_NG_LockedSecureDidRejectsWholeRequest)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();
    const uint8 req[5] = { DCM_SID_READ_DATA, (uint8)(DCM_DID_ENGINE_SPEED >> 8U), (uint8)(DCM_DID_ENGINE_SPEED & 0xFFU), (uint8)(DCM_DID_SECURE_INFO >> 8U), (uint8)(DCM_DID_SECURE_INFO & 0xFFU) };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_READ_DATA, DCM_NRC_SECURITY_ACCESS_DENIED);
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

TEST_F(Dcm_UdsServices_Test, Dcm_SessionControl_NG_MissingSubFunctionIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    const uint8 req[1] = { DCM_SID_SESSION_CTRL };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_SESSION_CTRL, DCM_NRC_INCORRECT_MESSAGE_LENGTH);
}

TEST_F(Dcm_UdsServices_Test, Dcm_EcuReset_NG_MissingSubFunctionIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    const uint8 req[1] = { DCM_SID_ECU_RESET };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_ECU_RESET, DCM_NRC_INCORRECT_MESSAGE_LENGTH);
}

TEST_F(Dcm_UdsServices_Test, Dcm_TesterPresent_NG_MissingSubFunctionIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    const uint8 req[1] = { DCM_SID_TESTER_PRESENT };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_TESTER_PRESENT, DCM_NRC_INCORRECT_MESSAGE_LENGTH);
}

TEST_F(Dcm_UdsServices_Test, Dcm_TesterPresent_NG_NonZeroSubFunctionIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    const uint8 req[2] = { DCM_SID_TESTER_PRESENT, 0x01U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_TESTER_PRESENT, DCM_NRC_SUB_FUNC_NOT_SUPPORTED);
}

TEST_F(Dcm_UdsServices_Test, Dcm_ProcessRequest_NG_UnknownServiceIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    const uint8 req[1] = { 0x99U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(0x99U, DCM_NRC_SERVICE_NOT_SUPPORTED);
}

TEST_F(Dcm_UdsServices_Test, Dcm_ClearDtc_NG_WithoutSecurityAccessIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();
    const uint8 req[4] = { DCM_SID_CLEAR_DTC, 0xFFU, 0xFFU, 0xFFU };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_CLEAR_DTC, DCM_NRC_SECURITY_ACCESS_DENIED);
}

TEST_F(Dcm_UdsServices_Test, Dcm_ClearDtc_NG_UnknownDtcGroupIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    UnlockSecurityAccessLevel1();
    const uint8 req[4] = { DCM_SID_CLEAR_DTC, 0x12U, 0x34U, 0x56U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_CLEAR_DTC, DCM_NRC_REQUEST_OUT_OF_RANGE);
}

// NG: Dem_ClearDTC() が DEM_WRONG_DTCORIGIN を返したら、NRC 0x31（[SWS_Dcm_01408]）。
TEST_F(Dcm_UdsServices_Test, Dcm_ClearDtc_NG_DemWrongDtcOriginIsRequestOutOfRange)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    UnlockSecurityAccessLevel1();
    FailFromCallCount_Dem_ClearDTC = 1U;   // 最初の Dem_ClearDTC() から、指定した値を返させる
    ForcedReturn_Dem_ClearDTC      = DEM_WRONG_DTCORIGIN;
    const uint8 req[4] = { DCM_SID_CLEAR_DTC, 0xFFU, 0xFFU, 0xFFU };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_CLEAR_DTC, DCM_NRC_REQUEST_OUT_OF_RANGE);
}

// NG: Dem_ClearDTC() が DEM_CLEAR_FAILED を返したら、NRC 0x22（[SWS_Dcm_00707]）。
TEST_F(Dcm_UdsServices_Test, Dcm_ClearDtc_NG_DemClearFailedIsConditionsNotCorrect)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    UnlockSecurityAccessLevel1();
    FailFromCallCount_Dem_ClearDTC = 1U;   // 最初の Dem_ClearDTC() から、指定した値を返させる
    ForcedReturn_Dem_ClearDTC      = DEM_CLEAR_FAILED;
    const uint8 req[4] = { DCM_SID_CLEAR_DTC, 0xFFU, 0xFFU, 0xFFU };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_CLEAR_DTC, DCM_NRC_CONDITIONS_NOT_CORRECT);
}

// NG: Dem_ClearDTC() が DEM_CLEAR_BUSY を返したら、NRC 0x22（[SWS_Dcm_00966]）。
TEST_F(Dcm_UdsServices_Test, Dcm_ClearDtc_NG_DemClearBusyIsConditionsNotCorrect)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    UnlockSecurityAccessLevel1();
    FailFromCallCount_Dem_ClearDTC = 1U;   // 最初の Dem_ClearDTC() から、指定した値を返させる
    ForcedReturn_Dem_ClearDTC      = DEM_CLEAR_BUSY;
    const uint8 req[4] = { DCM_SID_CLEAR_DTC, 0xFFU, 0xFFU, 0xFFU };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_CLEAR_DTC, DCM_NRC_CONDITIONS_NOT_CORRECT);
}

// NG: Dem_ClearDTC() が DEM_CLEAR_MEMORY_ERROR を返したら、NRC 0x72（[SWS_Dcm_01060]）。
TEST_F(Dcm_UdsServices_Test, Dcm_ClearDtc_NG_DemClearMemoryErrorIsGeneralProgrammingFailure)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    UnlockSecurityAccessLevel1();
    FailFromCallCount_Dem_ClearDTC = 1U;   // 最初の Dem_ClearDTC() から、指定した値を返させる
    ForcedReturn_Dem_ClearDTC      = DEM_CLEAR_MEMORY_ERROR;
    const uint8 req[4] = { DCM_SID_CLEAR_DTC, 0xFFU, 0xFFU, 0xFFU };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_CLEAR_DTC, DCM_NRC_GENERAL_PROGRAMMING_FAILURE);
}

// NG: Dem_ClearDTC() が E_NOT_OK を返したら（Dem の未初期化など。仕様に対応が無い）、NRC 0x22（簡略化）。
TEST_F(Dcm_UdsServices_Test, Dcm_ClearDtc_NG_DemNotOkIsConditionsNotCorrect)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    UnlockSecurityAccessLevel1();
    FailFromCallCount_Dem_ClearDTC = 1U;   // 最初の Dem_ClearDTC() から、指定した値を返させる
    ForcedReturn_Dem_ClearDTC      = E_NOT_OK;
    const uint8 req[4] = { DCM_SID_CLEAR_DTC, 0xFFU, 0xFFU, 0xFFU };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_CLEAR_DTC, DCM_NRC_CONDITIONS_NOT_CORRECT);
}

TEST_F(Dcm_UdsServices_Test, Dcm_WriteDataById_NG_WithoutSecurityAccessIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();
    const uint8 req[11] = { DCM_SID_WRITE_DATA, 0x01U, 0x04U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_WRITE_DATA, DCM_NRC_SECURITY_ACCESS_DENIED);
}

TEST_F(Dcm_UdsServices_Test, Dcm_WriteDataById_NG_MissingDidIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    UnlockSecurityAccessLevel1();
    const uint8 req[2] = { DCM_SID_WRITE_DATA, 0x01U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_WRITE_DATA, DCM_NRC_INCORRECT_MESSAGE_LENGTH);
}

TEST_F(Dcm_UdsServices_Test, Dcm_WriteDataById_NG_DataLengthDiffersFromDidLengthIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    UnlockSecurityAccessLevel1();
    const uint8 req[4] = { DCM_SID_WRITE_DATA, 0x01U, 0x04U, 0U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_WRITE_DATA, DCM_NRC_INCORRECT_MESSAGE_LENGTH);
}

TEST_F(Dcm_UdsServices_Test, Dcm_IoControl_NG_MissingControlOptionIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();
    const uint8 req[3] = { DCM_SID_IO_CONTROL, 0x01U, 0x05U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_IO_CONTROL, DCM_NRC_INCORRECT_MESSAGE_LENGTH);
}

TEST_F(Dcm_UdsServices_Test, Dcm_IoControl_NG_ReturnControlWithExtraByteIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();
    const uint8 req[5] = { DCM_SID_IO_CONTROL, 0x01U, 0x05U, DCM_IOCTRL_RETURN_CONTROL_TO_ECU, 0U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_IO_CONTROL, DCM_NRC_INCORRECT_MESSAGE_LENGTH);
}

TEST_F(Dcm_UdsServices_Test, Dcm_IoControl_NG_FreezeWithExtraByteIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();
    const uint8 req[5] = { DCM_SID_IO_CONTROL, 0x01U, 0x05U, DCM_IOCTRL_FREEZE_CURRENT_STATE, 0U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_IO_CONTROL, DCM_NRC_INCORRECT_MESSAGE_LENGTH);
}

TEST_F(Dcm_UdsServices_Test, Dcm_IoControl_NG_ShortTermAdjustmentWithoutStateIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();
    const uint8 req[4] = { DCM_SID_IO_CONTROL, 0x01U, 0x05U, DCM_IOCTRL_SHORT_TERM_ADJUSTMENT };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_IO_CONTROL, DCM_NRC_INCORRECT_MESSAGE_LENGTH);
}

TEST_F(Dcm_UdsServices_Test, Dcm_IoControl_NG_UnknownControlOptionIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();
    const uint8 req[4] = { DCM_SID_IO_CONTROL, 0x01U, 0x05U, 0x7FU };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_IO_CONTROL, DCM_NRC_REQUEST_OUT_OF_RANGE);
}

TEST_F(Dcm_UdsServices_Test, Dcm_CommunicationControl_NG_MissingControlTypeIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();
    const uint8 req[1] = { DCM_SID_COMM_CONTROL };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_COMM_CONTROL, DCM_NRC_INCORRECT_MESSAGE_LENGTH);
}

TEST_F(Dcm_UdsServices_Test, Dcm_ControlDtcSetting_NG_MissingSettingTypeIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();
    const uint8 req[1] = { DCM_SID_CONTROL_DTC_SETTING };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_CONTROL_DTC_SETTING, DCM_NRC_INCORRECT_MESSAGE_LENGTH);
}

TEST_F(Dcm_UdsServices_Test, Dcm_ControlDtcSetting_NG_DemFailureIsReportedAsConditionsNotCorrect)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();
    Dem_Test_ResetInitState();   // Dem が受け付けない（Dem_DisableDTCSetting() が失敗する）
    const uint8 req[2] = { DCM_SID_CONTROL_DTC_SETTING, DCM_DTCSETTING_OFF };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_CONTROL_DTC_SETTING, DCM_NRC_CONDITIONS_NOT_CORRECT);
}

TEST_F(Dcm_UdsServices_Test, Dcm_SecurityAccess_NG_MissingSubFunctionIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();
    const uint8 req[1] = { DCM_SID_SECURITY_ACCESS };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_SECURITY_ACCESS, DCM_NRC_INCORRECT_MESSAGE_LENGTH);
}

TEST_F(Dcm_UdsServices_Test, Dcm_SecurityAccess_NG_UnsupportedSubFunctionIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();
    const uint8 req[2] = { DCM_SID_SECURITY_ACCESS, 0x03U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_SECURITY_ACCESS, DCM_NRC_SUB_FUNC_NOT_SUPPORTED);
}

TEST_F(Dcm_UdsServices_Test, Dcm_SecurityAccess_NG_KeyWithoutSeedIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();
    const uint8 req[4] = { DCM_SID_SECURITY_ACCESS, DCM_SEC_SUBFUNC_SEND_KEY, 0U, 0U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_SECURITY_ACCESS, DCM_NRC_REQUEST_SEQUENCE_ERROR);
}

TEST_F(Dcm_UdsServices_Test, Dcm_RoutineControl_NG_UnsupportedSubFunctionIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();
    const uint8 req[4] = { DCM_SID_ROUTINE_CONTROL, 0x04U, (uint8)(DCM_RID_ENGINE_HEALTH_CHECK >> 8U), (uint8)(DCM_RID_ENGINE_HEALTH_CHECK & 0xFFU) };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_ROUTINE_CONTROL, DCM_NRC_SUB_FUNC_NOT_SUPPORTED);
}

TEST_F(Dcm_UdsServices_Test, Dcm_RoutineControl_NG_ResultsWithoutStartIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();
    const uint8 req[4] = { DCM_SID_ROUTINE_CONTROL, DCM_ROUTINE_SUBFUNC_REQUEST_RESULTS, (uint8)(DCM_RID_ENGINE_HEALTH_CHECK >> 8U), (uint8)(DCM_RID_ENGINE_HEALTH_CHECK & 0xFFU) };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_ROUTINE_CONTROL, DCM_NRC_REQUEST_SEQUENCE_ERROR);
}

TEST_F(Dcm_UdsServices_Test, Dcm_RequestDownload_NG_WithoutSecurityAccessIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();
    const uint8 req[5] = { DCM_SID_REQUEST_DOWNLOAD, DCM_TRANSFER_DATA_FORMAT_RAW, 0x11U, 0x00U, 0x10U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_REQUEST_DOWNLOAD, DCM_NRC_SECURITY_ACCESS_DENIED);
}

TEST_F(Dcm_UdsServices_Test, Dcm_RequestDownload_NG_MissingFormatByteIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    UnlockSecurityAccessLevel1();
    const uint8 req[2] = { DCM_SID_REQUEST_DOWNLOAD, DCM_TRANSFER_DATA_FORMAT_RAW };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_REQUEST_DOWNLOAD, DCM_NRC_INCORRECT_MESSAGE_LENGTH);
}

TEST_F(Dcm_UdsServices_Test, Dcm_RequestDownload_NG_LengthDiffersFromFormatIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    UnlockSecurityAccessLevel1();
    const uint8 req[4] = { DCM_SID_REQUEST_DOWNLOAD, DCM_TRANSFER_DATA_FORMAT_RAW, 0x11U, 0x00U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_REQUEST_DOWNLOAD, DCM_NRC_INCORRECT_MESSAGE_LENGTH);
}

TEST_F(Dcm_UdsServices_Test, Dcm_TransferData_NG_MissingBlockCounterIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    UnlockSecurityAccessLevel1();
    const uint8 download[5] = { DCM_SID_REQUEST_DOWNLOAD, DCM_TRANSFER_DATA_FORMAT_RAW, 0x11U, 0x00U, 0x10U };
    Send(download, sizeof(download));   // ダウンロードを始める
    ASSERT_EQ(LastData_CanTp_Transmit[0], 0x74U);
    const uint8 req[1] = { DCM_SID_TRANSFER_DATA };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_TRANSFER_DATA, DCM_NRC_INCORRECT_MESSAGE_LENGTH);
}

TEST_F(Dcm_UdsServices_Test, Dcm_SecurityAccess_NG_RepeatedWrongKeysLockOutFurtherRequests)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();
    SendWrongKey();
    ExpectNegative(DCM_SID_SECURITY_ACCESS, DCM_NRC_INVALID_KEY);   // 1 回目
    SendWrongKey();
    ExpectNegative(DCM_SID_SECURITY_ACCESS, DCM_NRC_INVALID_KEY);   // 2 回目
    SendWrongKey();   // 3 回目（上限 DCM_SECURITY_MAX_ATTEMPTS）
    ExpectNegative(DCM_SID_SECURITY_ACCESS, DCM_NRC_EXCEEDED_NUM_ATTEMPTS);
    const uint8 seedReq[2] = { DCM_SID_SECURITY_ACCESS, DCM_SEC_SUBFUNC_REQUEST_SEED };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(seedReq, sizeof(seedReq));   // 待機時間の途中で、シードを要求する

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_SECURITY_ACCESS, DCM_NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED);
}

TEST_F(Dcm_UdsServices_Test, Dcm_RoutineControl_NG_StartWhileRunningIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();
    const uint8 startReq[4] = { DCM_SID_ROUTINE_CONTROL, DCM_ROUTINE_SUBFUNC_START, (uint8)(DCM_RID_ENGINE_HEALTH_CHECK >> 8U), (uint8)(DCM_RID_ENGINE_HEALTH_CHECK & 0xFFU) };
    Send(startReq, sizeof(startReq));
    ASSERT_EQ(LastData_CanTp_Transmit[0], 0x71U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(startReq, sizeof(startReq));   // 実行中にもう一度開始する

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_ROUTINE_CONTROL, DCM_NRC_CONDITIONS_NOT_CORRECT);
}

TEST_F(Dcm_UdsServices_Test, Dcm_CopyTxData_NG_RetryBeyondStartOfResponseIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FailFromCallCount_CanTp_Transmit    = 0U;
    ForcedReturn_CanTp_Transmit         = E_OK;
    SuppressConfirmation_CanTp_Transmit = 1U;
    const uint8 req[3] = { DCM_SID_READ_DATA, (uint8)(DCM_DID_VIN >> 8U), (uint8)(DCM_DID_VIN & 0xFFU) };
    Send(req, sizeof(req));   // 応答（20 バイト）の送信が、まだ 1 バイトも進んでいない状態にする
    uint8 buf[7] = { 0U };
    PduInfoType info = { buf, 7U };
    const RetryInfoType retry = { TP_DATARETRY, 5U };   // 先頭より 5 バイト前へ戻せという指示

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    PduLengthType available = 0U;
    const BufReq_ReturnType ret = Dcm_CopyTxData(0U, &info, &retry, &available);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, BUFREQ_E_NOT_OK);   // 先頭より前へは戻れない
}

TEST_F(Dcm_UdsServices_Test, Dcm_ReadDtcInfo_NG_DemFailureIsReportedAsConditionsNotCorrect)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dem_Test_ResetInitState();   // Dem が受け付けない（可用性マスクの取得が失敗する）
    const uint8 countReq[3]  = { DCM_SID_READ_DTC_INFO, DCM_DTC_SUBFUNC_REPORT_COUNT, 0xFFU };
    const uint8 maskReq[3]   = { DCM_SID_READ_DTC_INFO, DCM_DTC_SUBFUNC_REPORT_BY_MASK, 0xFFU };
    const uint8 supportReq[2] = { DCM_SID_READ_DTC_INFO, DCM_DTC_SUBFUNC_REPORT_SUPPORTED };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(countReq, sizeof(countReq));
    const uint8 first = LastData_CanTp_Transmit[2];
    const uint32 firstLen = LastLength_CanTp_Transmit;
    Send(maskReq, sizeof(maskReq));
    const uint8 second = LastData_CanTp_Transmit[2];
    Send(supportReq, sizeof(supportReq));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(firstLen, 3U);
    EXPECT_EQ(first, DCM_NRC_CONDITIONS_NOT_CORRECT);   // 0x19 0x01
    EXPECT_EQ(second, DCM_NRC_CONDITIONS_NOT_CORRECT);  // 0x19 0x02
    ExpectNegative(DCM_SID_READ_DTC_INFO, DCM_NRC_CONDITIONS_NOT_CORRECT);   // 0x19 0x0A
}

TEST_F(Dcm_UdsServices_Test, Dcm_WriteDataById_NG_CryptoKeyUpdateWithoutKeyMIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    UnlockSecurityAccessLevel1();   // 暗号系（KeyM）は初期化しない

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    SendKeyUpdate(KEYM_CRYPTO_KEY_NAME_IMMOBILIZER_CMD);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_WRITE_DATA, DCM_NRC_REQUEST_OUT_OF_RANGE);   // KeyM_Start() が失敗する
}

TEST_F(Dcm_UdsServices_Test, Dcm_WriteDataById_NG_CryptoKeyUpdateOfUnknownKeyIsRejected)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    UnlockSecurityAccessLevel1();
    InitCryptoStack();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    SendKeyUpdate(0x7EU);   // KeyM に登録されていない鍵の名前

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_WRITE_DATA, DCM_NRC_REQUEST_OUT_OF_RANGE);   // KeyM_Update() が失敗する（開始した更新は終了して後始末する）
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

TEST_F(Dcm_UdsServices_Test, Dcm_ReadDataById_OK_EngineSpeedIsBigEndian)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FakeRte_EngineSpeed = 0x1234U;
    const uint8 req[3] = { DCM_SID_READ_DATA, (uint8)(DCM_DID_ENGINE_SPEED >> 8U), (uint8)(DCM_DID_ENGINE_SPEED & 0xFFU) };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(LastLength_CanTp_Transmit, 5U);
    EXPECT_EQ(LastData_CanTp_Transmit[0], 0x62U);
    EXPECT_EQ(LastData_CanTp_Transmit[3], 0x12U);   // 上位バイトが先（ビッグエンディアン）
    EXPECT_EQ(LastData_CanTp_Transmit[4], 0x34U);
}

TEST_F(Dcm_UdsServices_Test, Dcm_ReadDataById_OK_CoolantTempIsReturned)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FakeRte_CoolantTemp = 85U;
    const uint8 req[3] = { DCM_SID_READ_DATA, (uint8)(DCM_DID_COOLANT_TEMP >> 8U), (uint8)(DCM_DID_COOLANT_TEMP & 0xFFU) };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(LastLength_CanTp_Transmit, 4U);
    EXPECT_EQ(LastData_CanTp_Transmit[0], 0x62U);
    EXPECT_EQ(LastData_CanTp_Transmit[3], 85U);
}

TEST_F(Dcm_UdsServices_Test, Dcm_ReadDataById_OK_EngineStateIsReturned)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // Fake_Rte は、エンジン状態を常に 0 で返す
    const uint8 req[3] = { DCM_SID_READ_DATA, (uint8)(DCM_DID_ENGINE_STATE >> 8U), (uint8)(DCM_DID_ENGINE_STATE & 0xFFU) };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(LastLength_CanTp_Transmit, 4U);
    EXPECT_EQ(LastData_CanTp_Transmit[0], 0x62U);
    EXPECT_EQ(LastData_CanTp_Transmit[3], 0U);
}

// OK: 1 回の要求で複数の DID を読むと、要求の順に [0x62, (DID, データ) × n] を 1 つの応答で返す（EngineSpeed 2 バイト + CoolantTemp 1 バイト）。
TEST_F(Dcm_UdsServices_Test, Dcm_ReadDataById_OK_MultipleDidsAreReturnedInRequestOrder)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FakeRte_EngineSpeed = 0x1234U;
    FakeRte_CoolantTemp = 85U;
    const uint8 req[5] = { DCM_SID_READ_DATA, (uint8)(DCM_DID_ENGINE_SPEED >> 8U), (uint8)(DCM_DID_ENGINE_SPEED & 0xFFU), (uint8)(DCM_DID_COOLANT_TEMP >> 8U), (uint8)(DCM_DID_COOLANT_TEMP & 0xFFU) };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    const uint8 expected[] = { 0x62U, 0x01U, 0x01U, 0x12U, 0x34U, 0x01U, 0x02U, 85U };
    ASSERT_EQ(LastLength_CanTp_Transmit, sizeof(expected));
    for (uint8 i = 0U; i < sizeof(expected); i++)
    {
        EXPECT_EQ(LastData_CanTp_Transmit[i], expected[i]) << "byte " << (unsigned)i;
    }
}

// OK: 未対応の DID は応答に含めず、読める DID だけを返す（要求の DID が全て未対応のときだけ NRC 0x31。[SWS_Dcm_00438]）。
TEST_F(Dcm_UdsServices_Test, Dcm_ReadDataById_OK_UnsupportedDidIsSkippedWhenAnotherIsReadable)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FakeRte_CoolantTemp = 85U;
    const uint8 req[5] = { DCM_SID_READ_DATA, 0xFFU, 0xFFU, (uint8)(DCM_DID_COOLANT_TEMP >> 8U), (uint8)(DCM_DID_COOLANT_TEMP & 0xFFU) };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    const uint8 expected[] = { 0x62U, 0x01U, 0x02U, 85U };
    ASSERT_EQ(LastLength_CanTp_Transmit, sizeof(expected));
    for (uint8 i = 0U; i < sizeof(expected); i++)
    {
        EXPECT_EQ(LastData_CanTp_Transmit[i], expected[i]) << "byte " << (unsigned)i;
    }
}

// OK: defaultSession で読めない SECURE_INFO は、他の DID が読めるなら応答に含めない（[SWS_Dcm_00434]）。
TEST_F(Dcm_UdsServices_Test, Dcm_ReadDataById_OK_DidNotReadableInSessionIsSkippedWhenAnotherIsReadable)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FakeRte_CoolantTemp = 85U;
    const uint8 req[5] = { DCM_SID_READ_DATA, (uint8)(DCM_DID_SECURE_INFO >> 8U), (uint8)(DCM_DID_SECURE_INFO & 0xFFU), (uint8)(DCM_DID_COOLANT_TEMP >> 8U), (uint8)(DCM_DID_COOLANT_TEMP & 0xFFU) };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    const uint8 expected[] = { 0x62U, 0x01U, 0x02U, 85U };
    ASSERT_EQ(LastLength_CanTp_Transmit, sizeof(expected));
    for (uint8 i = 0U; i < sizeof(expected); i++)
    {
        EXPECT_EQ(LastData_CanTp_Transmit[i], expected[i]) << "byte " << (unsigned)i;
    }
}

// OK: extendedSession で SecurityAccess Level1 をアンロックすると、SECURE_INFO を読める（固定の 4 バイト "SEC!"）。
TEST_F(Dcm_UdsServices_Test, Dcm_ReadDataById_OK_SecureDidIsReadableAfterUnlock)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    UnlockSecurityAccessLevel1();
    const uint8 req[3] = { DCM_SID_READ_DATA, (uint8)(DCM_DID_SECURE_INFO >> 8U), (uint8)(DCM_DID_SECURE_INFO & 0xFFU) };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    const uint8 expected[] = { 0x62U, 0x01U, 0x09U, 0x53U, 0x45U, 0x43U, 0x21U };
    ASSERT_EQ(LastLength_CanTp_Transmit, sizeof(expected));
    for (uint8 i = 0U; i < sizeof(expected); i++)
    {
        EXPECT_EQ(LastData_CanTp_Transmit[i], expected[i]) << "byte " << (unsigned)i;
    }
}

// OK: 最大長の DID（VIN、17 バイト）を DCM_MAX_DID_TO_READ 個（同じ DID を重ねて指定）読んでも、応答バッファに収まり、指定した回数だけ返る。
TEST_F(Dcm_UdsServices_Test, Dcm_ReadDataById_OK_MaxDidsOfMaxLengthFitResponseBuffer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 req[1U + (2U * DCM_MAX_DID_TO_READ)] = { DCM_SID_READ_DATA };
    for (uint8 i = 0U; i < DCM_MAX_DID_TO_READ; i++)
    {
        req[1U + (2U * i)] = (uint8)(DCM_DID_VIN >> 8U);
        req[2U + (2U * i)] = (uint8)(DCM_DID_VIN & 0xFFU);
    }

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(req, sizeof(req));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    const uint8 unit = 2U + DCM_VIN_LENGTH;   // DID(2) + VIN
    ASSERT_EQ(LastLength_CanTp_Transmit, 1U + (DCM_MAX_DID_TO_READ * unit));
    EXPECT_EQ(LastData_CanTp_Transmit[0], 0x62U);
    for (uint8 n = 0U; n < DCM_MAX_DID_TO_READ; n++)
    {
        EXPECT_EQ(LastData_CanTp_Transmit[1U + (n * unit)], 0xF1U) << "did " << (unsigned)n;
        EXPECT_EQ(LastData_CanTp_Transmit[2U + (n * unit)], 0x90U) << "did " << (unsigned)n;
    }
}

TEST_F(Dcm_UdsServices_Test, Dcm_SecurityAccess_OK_SeedIsGrantedAgainAfterLockoutDelay)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();
    SendWrongKey();
    SendWrongKey();
    SendWrongKey();   // ロックアウトになる
    ExpectNegative(DCM_SID_SECURITY_ACCESS, DCM_NRC_EXCEEDED_NUM_ATTEMPTS);
    FakeMillis_Value += DCM_SECURITY_DELAY_MS;   // 待機時間が経過する

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const uint16 seed = RequestSeed();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(LastData_CanTp_Transmit[0], 0x67U);   // 再びシードを返す
    EXPECT_NE(seed, 0U);
}

TEST_F(Dcm_UdsServices_Test, Dcm_RoutineControl_OK_SessionChangeAbortsRunningRoutine)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();
    const uint8 startReq[4] = { DCM_SID_ROUTINE_CONTROL, DCM_ROUTINE_SUBFUNC_START, (uint8)(DCM_RID_ENGINE_HEALTH_CHECK >> 8U), (uint8)(DCM_RID_ENGINE_HEALTH_CHECK & 0xFFU) };
    Send(startReq, sizeof(startReq));   // 実行中にする
    const uint8 toDefault[2] = { DCM_SID_SESSION_CTRL, DCM_SESSION_DEFAULT };
    Send(toDefault, sizeof(toDefault));   // defaultSession へ戻す（実行中のルーチンは中止される）
    EnterExtendedSession();
    const uint8 resultReq[4] = { DCM_SID_ROUTINE_CONTROL, DCM_ROUTINE_SUBFUNC_REQUEST_RESULTS, (uint8)(DCM_RID_ENGINE_HEALTH_CHECK >> 8U), (uint8)(DCM_RID_ENGINE_HEALTH_CHECK & 0xFFU) };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(resultReq, sizeof(resultReq));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_ROUTINE_CONTROL, DCM_NRC_REQUEST_SEQUENCE_ERROR);   // 中止されているので、結果は無い
}

TEST_F(Dcm_UdsServices_Test, Dcm_RequestDownload_OK_SessionChangeAbortsTransfer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    UnlockSecurityAccessLevel1();
    const uint8 download[5] = { DCM_SID_REQUEST_DOWNLOAD, DCM_TRANSFER_DATA_FORMAT_RAW, 0x11U, 0x00U, 0x10U };
    Send(download, sizeof(download));
    ASSERT_EQ(LastData_CanTp_Transmit[0], 0x74U);   // 転送中にする
    const uint8 toDefault[2] = { DCM_SID_SESSION_CTRL, DCM_SESSION_DEFAULT };
    Send(toDefault, sizeof(toDefault));   // defaultSession へ戻す（転送は中止される）
    UnlockSecurityAccessLevel1();
    const uint8 transfer[3] = { DCM_SID_TRANSFER_DATA, 0x01U, 0xAAU };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(transfer, sizeof(transfer));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_TRANSFER_DATA, DCM_NRC_REQUEST_SEQUENCE_ERROR);   // 転送は中止されているので、ブロックを受け付けない
}

TEST_F(Dcm_UdsServices_Test, Dcm_MainFunction_OK_DefaultSessionHasNoS3Supervision)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FakeMillis_Value += 600000UL;   // どれだけ長く診断要求が無くても

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dcm_MainFunction();
    uint8 session = 0U;
    const Std_ReturnType ret = Dcm_GetSesCtrlType(&session);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_EQ(ret, E_OK);
    EXPECT_EQ(session, DCM_SESSION_DEFAULT);   // defaultSession のまま（S3 の監視は extendedSession だけ）
}

TEST_F(Dcm_UdsServices_Test, Dcm_SessionControl_OK_ReturningToDefaultToleratesDtcSettingReenableFailure)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();
    const uint8 off[2] = { DCM_SID_CONTROL_DTC_SETTING, DCM_DTCSETTING_OFF };
    Send(off, sizeof(off));   // DTC の記録を無効にする
    ASSERT_EQ(LastData_CanTp_Transmit[0], 0xC5U);
    Dem_Test_ResetInitState();   // 以降、Dem は有効化の要求を受け付けない
    const uint8 toDefault[2] = { DCM_SID_SESSION_CTRL, DCM_SESSION_DEFAULT };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(toDefault, sizeof(toDefault));   // defaultSession へ戻る（記録の自動再有効化は失敗する）

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(LastData_CanTp_Transmit[0], 0x50U);   // セッション切り替え自体は成功する
}

TEST_F(Dcm_UdsServices_Test, Dcm_WriteDataById_OK_CryptoKeyUpdateIsAccepted)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    UnlockSecurityAccessLevel1();
    InitCryptoStack();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    SendKeyUpdate(KEYM_CRYPTO_KEY_NAME_IMMOBILIZER_CMD);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(LastLength_CanTp_Transmit, 3U);
    EXPECT_EQ(LastData_CanTp_Transmit[0], 0x6EU);   // 正応答 [0x6E, DID(2)]
    EXPECT_EQ(LastData_CanTp_Transmit[1], (uint8)(DCM_DID_CRYPTO_KEY_UPDATE >> 8U));
    EXPECT_EQ(LastData_CanTp_Transmit[2], (uint8)(DCM_DID_CRYPTO_KEY_UPDATE & 0xFFU));
}

}  // namespace
