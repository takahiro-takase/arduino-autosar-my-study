/**
 * \file    Dcm_MainFunction_S3Timer_test.cpp
 * \brief   S3 タイマ（セッションタイムアウト、[SWS_Dcm_00140]/[SWS_Dcm_00141]）の単体テスト
 * \details 非 default セッションで、最後の診断活動から DCM_S3_TIMEOUT_MS が経つと、Dcm_MainFunction() が default セッションへ戻す。
 *          S3Server タイマは、要求の受信の開始で止まり、最終応答の完了（Dcm_TpTxConfirmation）・受信の失敗
 *          （Dcm_TpRxIndication の失敗）で再開する（[SWS_Dcm_00141]）。本実装は、タイマを止める代わりに、最終活動時刻を
 *          更新する。時刻は Fake_Millis で進める。生の UDS 要求は WrapDcm_InjectRequest() で Dcm へ直接渡す。
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
#include "NvM_TestUtil.h"
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

/* S3 タイマのテスト用フィクスチャ。
 * SetUp(): NvM・CanTp・Dem・Dcm を初期化する（通信管理は対象外にする。CanTp への応答の送信は、常に受け付けさせる）。
 * TearDown(): NvM の初期化状態を戻し、他のテストへ影響を残さない。
 * EnterExtendedSession(): extendedSession へ遷移し、応答の送信の完了（Dcm_TpTxConfirmation）まで済ませる。 */
class Dcm_MainFunction_S3Timer_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        WrapCanTp_Reset();
        WrapDem_Reset();
        CaptureEnabled_CanTp_Transmit = 1U;
        FailFromCallCount_CanTp_Transmit = 1U;   // 応答の送信は、常に受け付けられる（完了の通知を送るまで、送信は進行中のまま）
        ForcedReturn_CanTp_Transmit      = E_OK;
        SuppressConfirmation_CanTp_Transmit = 1U;   // 完了の通知（Dcm_TpTxConfirmation）は、テストが明示的に送る
        Suppressed_ComM_DcmDiagnostic = 1U;  // 本テストは通信管理(ComM/CanSM/CanNm)が対象外
        FakeRte_Reset();
        FakeDetHw_LogSuppressed = 1U;

        NvMTest_ColdBoot();
        CanTp_Init(NULL);
        Dem_Init(NULL);
        Dcm_Init(NULL);

        FakeDetHw_LogSuppressed = 0U;
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        NvMTest_Shutdown();
        WrapComM_Reset();
        FakeRte_Reset();
    }

    /** 生の UDS 要求を Dcm へ直接渡す。 */
    static void Send(const uint8* payload, uint8 len)
    {
        (void)WrapDcm_InjectRequest(payload, len);
    }

    /** extendedSession へ遷移する。応答の送信の完了まで済ませる（送信が進行中のままだと、S3 が止まる）。 */
    static void EnterExtendedSession()
    {
        const uint8 req[2] = { DCM_SID_SESSION_CTRL, DCM_SESSION_EXTENDED };
        Send(req, sizeof(req));
        Dcm_TpTxConfirmation(0U, E_OK);
    }

    /** 現在のセッションを返す。 */
    static Dcm_SesCtrlType Session()
    {
        Dcm_SesCtrlType session = 0U;
        (void)Dcm_GetSesCtrlType(&session);
        return session;
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Dcm_MainFunction_S3Timer_Test, Dcm_MainFunction_OK_StaysInExtendedSessionBeforeS3Timeout)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();
    FakeMillis_Value += DCM_S3_TIMEOUT_MS - 100UL;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dcm_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Session(), DCM_SESSION_EXTENDED);
}

TEST_F(Dcm_MainFunction_S3Timer_Test, Dcm_MainFunction_OK_ReturnsToDefaultSessionAfterS3Timeout)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();
    FakeMillis_Value += DCM_S3_TIMEOUT_MS + 1UL;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dcm_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Session(), DCM_SESSION_DEFAULT);
}

// ------------------------------------------------------------
// 要求の受信の開始で、S3 タイマを止める（本実装は、最終活動時刻を更新して、受信の途中で S3 が切れないようにする）。
// S3 の切れる直前に受信が始まっても、その直後に、セッションが戻ってしまうことはない。
// ------------------------------------------------------------

TEST_F(Dcm_MainFunction_S3Timer_Test, Dcm_StartOfReception_OK_RestartsS3Timer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();
    FakeMillis_Value += DCM_S3_TIMEOUT_MS - 100UL;   // S3 が切れる直前
    PduLengthType bufferSize = 0U;
    ASSERT_EQ(Dcm_StartOfReception(0U, NULL, 2U, &bufferSize), BUFREQ_OK);   // 受信が始まる
    FakeMillis_Value += 200UL;   // 受信の開始から 200 ms（最後の応答から数えると、S3 を超える）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dcm_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Session(), DCM_SESSION_EXTENDED);
}

// ------------------------------------------------------------
// 受信の失敗で、S3 タイマを再開する（[SWS_Dcm_00141]）。受信の開始から S3 に近い時間が経ってから失敗しても、
// その失敗の時点から、改めて S3 を数える。
// ------------------------------------------------------------

TEST_F(Dcm_MainFunction_S3Timer_Test, Dcm_TpRxIndication_OK_FailedReceptionRestartsS3Timer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();
    PduLengthType bufferSize = 0U;
    ASSERT_EQ(Dcm_StartOfReception(0U, NULL, 2U, &bufferSize), BUFREQ_OK);
    FakeMillis_Value += DCM_S3_TIMEOUT_MS - 100UL;   // 受信が止まったまま、S3 に近い時間が経つ
    Dcm_TpRxIndication(0U, E_NOT_OK);                // 受信が失敗した
    FakeMillis_Value += 200UL;                       // 失敗から 200 ms（受信の開始から数えると、S3 を超える）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dcm_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Session(), DCM_SESSION_EXTENDED);
}

// ------------------------------------------------------------
// 最終応答の完了（Dcm_TpTxConfirmation）で、S3 タイマを再開する（[SWS_Dcm_00141]）。応答の送信に時間がかかっても、
// 完了の時点から、S3 を数える。
// ------------------------------------------------------------

TEST_F(Dcm_MainFunction_S3Timer_Test, Dcm_TpTxConfirmation_OK_RestartsS3Timer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    const uint8 req[2] = { DCM_SID_SESSION_CTRL, DCM_SESSION_EXTENDED };
    Send(req, sizeof(req));                          // 応答の送信が始まる（完了の通知は、まだ）
    FakeMillis_Value += DCM_S3_TIMEOUT_MS - 100UL;   // 送信に、S3 に近い時間がかかる
    Dcm_TpTxConfirmation(0U, E_OK);                  // 最終応答が完了した
    FakeMillis_Value += 200UL;                       // 完了から 200 ms（要求の受信から数えると、S3 を超える）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dcm_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Session(), DCM_SESSION_EXTENDED);
}

// ------------------------------------------------------------
// 送信が進行中でないときの通知（重複した通知や、遅れて届いた通知）は、応答が完了したわけではないので、S3 タイマを再開しない。
// ------------------------------------------------------------

TEST_F(Dcm_MainFunction_S3Timer_Test, Dcm_TpTxConfirmation_OK_DuplicateConfirmationDoesNotRestartS3Timer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    EnterExtendedSession();                          // 応答の送信は、ここで完了済み（送信は進行中でない）
    FakeMillis_Value += DCM_S3_TIMEOUT_MS - 100UL;   // S3 が切れる直前
    Dcm_TpTxConfirmation(0U, E_OK);                  // 重複した通知（応答は、もう完了している）
    FakeMillis_Value += 200UL;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dcm_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Session(), DCM_SESSION_DEFAULT);   // 重複した通知では延長されず、S3 が切れる
}

}  // namespace
