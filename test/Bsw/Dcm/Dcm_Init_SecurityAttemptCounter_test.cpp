/**
 * \file    Dcm_Init_SecurityAttemptCounter_test.cpp
 * \brief   Dcm_Init() が、SecurityAccess の連続失敗回数（AttemptCounter）を NvM から復元することの単体テスト
 * \details 失敗回数は、sendKey が失敗するたびに NvM（NVM_BLOCK_ID_DCM_SECURITY）へ書かれる
 *          （[SWS_Dcm_01155]）。再起動（NvM_Init() → Dcm_Init() のやり直し）後も、回数とロックアウトが
 *          残ることを、実体の NvM と模擬 Fee の EEPROM で確かめる（[SWS_Dcm_01154]/[SWS_Dcm_01156]/
 *          [SWS_Dcm_01357]）。生の UDS 要求を Dcm へ直接渡し（WrapDcm_InjectRequest()）、CanTp へ渡された
 *          応答を確認する。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Dcm.h"
#include "Dcm_Cfg.h"
#include "Dem.h"
#include "NvM.h"
#include "MemIf.h"
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

/* SecurityAccess の失敗回数の永続化を確かめるためのフィクスチャ。
 * SetUp(): 空の EEPROM から、MemIf_Init()・NvM_Init(NULL)・Dem_Init(NULL)・Dcm_Init(NULL) の順に初期化する。
 * Reboot(): NvM と Dcm を初期化し直す（EEPROM は残す）。電源断と同じく、保留中の書き込みを書き切る処理（NvM_WriteAll()）は呼ばない
 *           （応答の前に書き切るのは、Dcm の仕事）。
 * TearDown(): NvM の初期化状態を戻し、他のテストへ影響を残さない。 */
class Dcm_Init_SecurityAttemptCounter_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        WrapCanTp_Reset();
        CaptureEnabled_CanTp_Transmit = 1U;  // 応答を LastData_CanTp_Transmit へ複製する
        Suppressed_ComM_DcmDiagnostic = 1U;  // 本テストは通信管理(ComM/CanSM/CanNm)が対象外
        FakeRte_Reset();
        FakeDetHw_LogSuppressed = 1U;

        CanTp_Init(NULL);
        MemIf_Init();
        NvM_Init(NULL);
        Dem_Init(NULL);
        Dcm_Init(NULL);

        FakeDetHw_LogSuppressed = 0U;
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        NvM_Test_ResetInitState();
        WrapComM_Reset();
        FakeRte_Reset();
    }

    /** 生の UDS 要求を Dcm へ直接渡す（WrapDcm_InjectRequest()）。 */
    static void Send(const uint8* payload, uint8 len)
    {
        (void)WrapDcm_InjectRequest(payload, len);
    }

    /** 直近の応答が、指定の否定応答であることを確認する。 */
    static void ExpectNegative(uint8 sid, uint8 nrc)
    {
        EXPECT_EQ(LastLength_CanTp_Transmit, 3U);
        EXPECT_EQ(LastData_CanTp_Transmit[0], DCM_SID_NEGATIVE_RESP);
        EXPECT_EQ(LastData_CanTp_Transmit[1], sid);
        EXPECT_EQ(LastData_CanTp_Transmit[2], nrc);
    }

    /** extendedSession へ遷移する（SecurityAccess は extendedSession 限定）。 */
    static void EnterExtendedSession()
    {
        const uint8 req[2] = { DCM_SID_SESSION_CTRL, DCM_SESSION_EXTENDED };
        Send(req, sizeof(req));
    }

    /** requestSeed を送り、返ってきたシードを返す（正応答でなければ EXPECT 失敗）。 */
    static uint16 RequestSeed()
    {
        const uint8 seedReq[2] = { DCM_SID_SECURITY_ACCESS, DCM_SEC_SUBFUNC_REQUEST_SEED };
        Send(seedReq, sizeof(seedReq));
        EXPECT_EQ(LastData_CanTp_Transmit[0], 0x67U);
        return (uint16)(((uint16)LastData_CanTp_Transmit[2] << 8U) | (uint16)LastData_CanTp_Transmit[3]);
    }

    /** sendKey を送る。 */
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

    /** requestSeed の後、正しい鍵を送って解錠する。 */
    static void SendCorrectKey()
    {
        const uint16 seed = RequestSeed();
        SendKey((uint16)(seed ^ DCM_SECURITY_KEY_MASK));
    }

    /** 再起動する（電源断）。保留中の書き込みは書き切らずに、NvM と Dcm を初期化し直す（EEPROM は残す）。 */
    static void Reboot()
    {
        FakeDetHw_LogSuppressed = 1U;
        NvM_Test_ResetInitState();
        NvM_Init(NULL);
        Dcm_Init(NULL);
        FakeDetHw_LogSuppressed = 0U;
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

// ------------------------------------------------------------
// 異常系: 失敗回数を NvM から読めなかったときは、ロックアウトする回数にする（[SWS_Dcm_01156]）。
// NvM が初期化されていないと NvM_ReadBlock() が E_NOT_OK を返す。
// ------------------------------------------------------------

TEST_F(Dcm_Init_SecurityAttemptCounter_Test, Dcm_Init_NG_UnreadableAttemptCounterLocksOut)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FakeDetHw_LogSuppressed = 1U;
    NvM_Test_ResetInitState();   // NvM を未初期化へ戻す
    Dcm_Init(NULL);              // NvM_ReadBlock() が E_NOT_OK
    FakeDetHw_LogSuppressed = 0U;
    EnterExtendedSession();
    const uint8 seedReq[2] = { DCM_SID_SECURITY_ACCESS, DCM_SEC_SUBFUNC_REQUEST_SEED };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(seedReq, sizeof(seedReq));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_SECURITY_ACCESS, DCM_NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED);
}

// ------------------------------------------------------------
// 正常系: 一度も書かれていない（消去済みの）EEPROM からは、失敗回数 0 で始まり、ロックアウトしない。
// （1 バイトの 0xFF は NvM の CRC8 でも正しい組になるため、NvM は既定値へ戻さない。）
// ------------------------------------------------------------

TEST_F(Dcm_Init_SecurityAttemptCounter_Test, Dcm_Init_OK_ErasedEepromStartsWithZeroAttempts)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    SendWrongKey();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_SECURITY_ACCESS, DCM_NRC_INVALID_KEY);   // 1 回目の失敗（ロックアウトではない）
}

// ------------------------------------------------------------
// 正常系: 失敗した回数は、再起動しても残る。2 回失敗して再起動した後の 1 回の失敗が、3 回目として数えられる
// （[SWS_Dcm_01154]/[SWS_Dcm_01155]）。
// ------------------------------------------------------------

TEST_F(Dcm_Init_SecurityAttemptCounter_Test, Dcm_Init_OK_FailedAttemptsSurviveReboot)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();
    SendWrongKey();
    ExpectNegative(DCM_SID_SECURITY_ACCESS, DCM_NRC_INVALID_KEY);   // 1 回目
    SendWrongKey();
    ExpectNegative(DCM_SID_SECURITY_ACCESS, DCM_NRC_INVALID_KEY);   // 2 回目
    Reboot();
    EnterExtendedSession();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    SendWrongKey();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_SECURITY_ACCESS, DCM_NRC_EXCEEDED_NUM_ATTEMPTS);   // 再起動をまたいで 3 回目
}

// ------------------------------------------------------------
// 正常系: ロックアウトは、再起動しても解けない。再起動の直後は requestSeed が NRC 0x37 になり、
// 待機時間（DCM_SECURITY_DELAY_MS）が経つと、再びシードを発行する（[SWS_Dcm_01355]）。
// ------------------------------------------------------------

TEST_F(Dcm_Init_SecurityAttemptCounter_Test, Dcm_Init_OK_LockoutSurvivesReboot)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();
    for (unsigned i = 0U; i < DCM_SECURITY_MAX_ATTEMPTS; i++)
    {
        SendWrongKey();
    }
    ExpectNegative(DCM_SID_SECURITY_ACCESS, DCM_NRC_EXCEEDED_NUM_ATTEMPTS);
    Reboot();
    EnterExtendedSession();
    const uint8 seedReq[2] = { DCM_SID_SECURITY_ACCESS, DCM_SEC_SUBFUNC_REQUEST_SEED };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(seedReq, sizeof(seedReq));
    const uint8 firstResponse = LastData_CanTp_Transmit[0];
    const uint8 firstNrc      = LastData_CanTp_Transmit[2];
    FakeMillis_Value += DCM_SECURITY_DELAY_MS + 1UL;
    Send(seedReq, sizeof(seedReq));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(firstResponse, DCM_SID_NEGATIVE_RESP);                       // 再起動の直後は、まだロックアウト中
    EXPECT_EQ(firstNrc, DCM_NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED);
    EXPECT_EQ(LastData_CanTp_Transmit[0], 0x67U);                          // 待機時間が経てば、シードを発行する
}

// ------------------------------------------------------------
// 正常系: 待機時間が経ってロックアウトが解けると、保持している回数も 0 に戻る（[SWS_Dcm_01357]）。
// 再起動した後も、また 1 回目の失敗から数える。
// ------------------------------------------------------------

TEST_F(Dcm_Init_SecurityAttemptCounter_Test, Dcm_Init_OK_ExpiredLockoutResetsPersistedCounter)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();
    for (unsigned i = 0U; i < DCM_SECURITY_MAX_ATTEMPTS; i++)
    {
        SendWrongKey();
    }
    FakeMillis_Value += DCM_SECURITY_DELAY_MS + 1UL;
    (void)RequestSeed();   // 待機時間が経っているので、ロックアウトが解けて回数が 0 に戻る
    Reboot();
    EnterExtendedSession();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    SendWrongKey();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_SECURITY_ACCESS, DCM_NRC_INVALID_KEY);   // 1 回目の失敗として数える
}

// ------------------------------------------------------------
// 正常系: sendKey が成功すると、保持している回数も 0 に戻る（[SWS_Dcm_01357]）。
// 2 回失敗してから解錠し、再起動した後の失敗は、また 1 回目として数える。
// ------------------------------------------------------------

TEST_F(Dcm_Init_SecurityAttemptCounter_Test, Dcm_Init_OK_SuccessfulKeyResetsPersistedCounter)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();
    SendWrongKey();
    SendWrongKey();
    SendCorrectKey();
    ASSERT_EQ(LastData_CanTp_Transmit[0], 0x67U);   // 解錠できた
    Reboot();
    EnterExtendedSession();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    SendWrongKey();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_SECURITY_ACCESS, DCM_NRC_INVALID_KEY);   // 1 回目の失敗として数える
}

// ------------------------------------------------------------
// 正常系: 想定外の値（上限と消去済みのどちらでもない）が保持されていたときは、安全側に倒して、ロックアウトする。
// ------------------------------------------------------------

TEST_F(Dcm_Init_SecurityAttemptCounter_Test, Dcm_Init_OK_UnexpectedStoredValueLocksOut)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    const uint8 unexpected = 5U;   // DCM_SECURITY_MAX_ATTEMPTS（3）より大きく、0xFF（消去済み）ではない
    ASSERT_EQ(NvM_WriteBlock(NVM_BLOCK_ID_DCM_SECURITY, &unexpected), E_OK);
    NvM_WriteAll();
    Reboot();
    EnterExtendedSession();
    const uint8 seedReq[2] = { DCM_SID_SECURITY_ACCESS, DCM_SEC_SUBFUNC_REQUEST_SEED };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Send(seedReq, sizeof(seedReq));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_SECURITY_ACCESS, DCM_NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED);
}

// ------------------------------------------------------------
// 正常系: 鍵を間違えた応答を返す前に、失敗回数を EEPROM へ書き切っている。応答の直後に電源を切られても
// （保留中の書き込みを書き切らずに再起動しても）、失敗は残る。
// ------------------------------------------------------------

TEST_F(Dcm_Init_SecurityAttemptCounter_Test, Dcm_Init_OK_FailedAttemptIsPersistedBeforeResponse)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();
    SendWrongKey();
    SendWrongKey();
    Reboot();   // 応答の直後の電源断（NvM_WriteAll() は呼ばない）
    EnterExtendedSession();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    SendWrongKey();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectNegative(DCM_SID_SECURITY_ACCESS, DCM_NRC_EXCEEDED_NUM_ATTEMPTS);
}

}  // namespace
