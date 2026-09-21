/**
 * \file    Bsw_Dcm_RoutineControl_test.cpp
 * \brief   UDS SID 0x31 RoutineControl の単体テスト（GoogleTest /
 *          PlatformIO `[env:native_chain]`。2026-08新設時は専用環境`[env:native_dcm]`だったが、2026-09にnative_chainへ統合した）。
 *
 * \details Bsw_Dcm_ControlDTCSetting_test.cpp と同じ「Dcm_ComIndication() に
 *          生の UDS バイト列を直接渡し、CanTp_Transmit()（Fake_CanTp.h で
 *          キャプチャ）へ渡された応答を検証する」ブラックボックステスト方式。
 *
 *          0x31 は extendedSession 限定のため、EnterExtendedSession() で
 *          事前にセッションを遷移させてから各テストを実行する。対応 RID は
 *          DCM_RID_ENGINE_HEALTH_CHECK (0x0203) のみ、routineControlOptionRecord
 *          は定義しない（Dcm_HandleRoutineControl() 参照）。
 *
 *          2026-09 追加: [SWS_Dcm_01140]「overall length」の厳密チェック
 *          （以前は udsLen<4 の下限のみで、余分な末尾バイトを黙って受理
 *          していた）の回帰検知が主目的。
 */
#include <gtest/gtest.h>

extern "C" {
#include "Dcm.h"
#include "Dcm_Cfg.h"
#include "Dem.h"
#include "Fake_CanTp.h"
#include "Fake_Millis.h"
#include "Fake_Det_Hw.h"
#include "Wrap_ComM.h"
}

namespace
{

class Bsw_Dcm_RoutineControl_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        FakeCanTp_Reset();
        Suppressed_ComM_DcmDiagnostic = 1U;  // 本テストは通信管理(ComM/CanSM/Nm)が対象外
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制

        Dem_Init(NULL);
        Dcm_Init(NULL);

        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        WrapComM_Reset();  // 他のテストファイルへ影響を残さない
        FakeDetHw_LogSuppressed = 1U;
    }

    /** UDS ペイロードを Dcm_ComIndication() へ直接渡す。 */
    void Send(const uint8* payload, uint8 len)
    {
        PduInfoType pdu = { const_cast<uint8*>(payload), len };
        Dcm_ComIndication(0U, &pdu);
    }

    /** [0x10, 0x03] extendedDiagnosticSession へ遷移する（0x31 の前提）。 */
    void EnterExtendedSession()
    {
        uint8 req[2] = { DCM_SID_SESSION_CTRL, DCM_SESSION_EXTENDED };
        Send(req, sizeof(req));
        ASSERT_EQ(FakeCanTp_TxBuf[0], 0x50U);  // 正応答確認（前提が崩れていないこと）
        FakeCanTp_Reset();
    }

    /** [0x31, subFunc, RID_H, RID_L] をちょうど4byteで送る。 */
    void SendRoutineControl(uint8 subFunc, uint16 rid = DCM_RID_ENGINE_HEALTH_CHECK)
    {
        uint8 req[4] = { DCM_SID_ROUTINE_CONTROL, subFunc,
                          (uint8)(rid >> 8U), (uint8)(rid & 0xFFU) };
        Send(req, sizeof(req));
    }

    /** 直近の応答が [0x7F, 0x31, nrc] というネガティブレスポンスであることを
     *  検証する（自己/simplify指摘: 4テストで重複していた検証ブロックを集約）。 */
    static void ExpectNegativeResponse(uint8 nrc)
    {
        ASSERT_EQ(FakeCanTp_TransmitCount, 1U);
        ASSERT_EQ(FakeCanTp_TxLength, 3U);
        EXPECT_EQ(FakeCanTp_TxBuf[0], DCM_SID_NEGATIVE_RESP);
        EXPECT_EQ(FakeCanTp_TxBuf[1], DCM_SID_ROUTINE_CONTROL);
        EXPECT_EQ(FakeCanTp_TxBuf[2], nrc);
    }
};

// ------------------------------------------------------------
// 正常系（ちょうど4byte、回帰確認）
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_RoutineControl_Test, RoutineControl_OK_StartWithExactLengthIsAccepted)
{
    EnterExtendedSession();

    SendRoutineControl(DCM_ROUTINE_SUBFUNC_START);

    /* 評価 (Assert): [0x71, 0x01, 0x02, 0x03] */
    ASSERT_EQ(FakeCanTp_TransmitCount, 1U);
    ASSERT_EQ(FakeCanTp_TxLength, 4U);
    EXPECT_EQ(FakeCanTp_TxBuf[0], 0x71U);
    EXPECT_EQ(FakeCanTp_TxBuf[1], DCM_ROUTINE_SUBFUNC_START);
    EXPECT_EQ(FakeCanTp_TxBuf[2], 0x02U);
    EXPECT_EQ(FakeCanTp_TxBuf[3], 0x03U);
}

TEST_F(Bsw_Dcm_RoutineControl_Test, RoutineControl_OK_StopWithExactLengthIsAccepted)
{
    EnterExtendedSession();
    SendRoutineControl(DCM_ROUTINE_SUBFUNC_START);
    FakeCanTp_Reset();

    SendRoutineControl(DCM_ROUTINE_SUBFUNC_STOP);

    ASSERT_EQ(FakeCanTp_TransmitCount, 1U);
    ASSERT_EQ(FakeCanTp_TxLength, 4U);
    EXPECT_EQ(FakeCanTp_TxBuf[0], 0x71U);
    EXPECT_EQ(FakeCanTp_TxBuf[1], DCM_ROUTINE_SUBFUNC_STOP);
}

// ------------------------------------------------------------
// 2026-09 是正の本題: [SWS_Dcm_01140] overall length の厳密チェック
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_RoutineControl_Test, RoutineControl_NG_TooShortReturnsIncorrectLength)
{
    EnterExtendedSession();

    /* 準備 (Arrange): RID が1byte欠けている(3byte)。是正前から検知できていた
     * はずの下限チェック（[SWS_Dcm_00696]）の回帰確認。 */
    uint8 req[3] = { DCM_SID_ROUTINE_CONTROL, DCM_ROUTINE_SUBFUNC_START, 0x02U };
    Send(req, sizeof(req));

    /* 評価 (Assert): [0x7F, 0x31, 0x13 incorrectMessageLength] */
    ExpectNegativeResponse(DCM_NRC_INCORRECT_MESSAGE_LENGTH);
}

TEST_F(Bsw_Dcm_RoutineControl_Test, RoutineControl_NG_ExtraTrailingByteReturnsIncorrectLength)
{
    EnterExtendedSession();

    /* 準備 (Arrange): [SWS_Dcm_01140] 相当。対応 RID は
     * routineControlOptionRecord を定義しないため、余分な1byte(0x99)が
     * 付いた5byte要求は不正な長さのはず（2026-09是正前は udsLen<4 の
     * 下限のみで黙って受理していた）。 */
    uint8 req[5] = { DCM_SID_ROUTINE_CONTROL, DCM_ROUTINE_SUBFUNC_START, 0x02U, 0x03U, 0x99U };
    Send(req, sizeof(req));

    /* 評価 (Assert): [0x7F, 0x31, 0x13 incorrectMessageLength] */
    ExpectNegativeResponse(DCM_NRC_INCORRECT_MESSAGE_LENGTH);

    /* 拒否された要求が副作用を持たない(ルーチンが開始されていない)ことも
     * 確認する: stopRoutine が「未開始」の requestSequenceError で拒否される
     * はず。 */
    FakeCanTp_Reset();
    SendRoutineControl(DCM_ROUTINE_SUBFUNC_STOP);
    EXPECT_EQ(FakeCanTp_TxBuf[2], DCM_NRC_REQUEST_SEQUENCE_ERROR);
}

// ------------------------------------------------------------
// 既存の異常系（回帰確認）
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_RoutineControl_Test, RoutineControl_NG_DefaultSessionRejectsWithNrc7F)
{
    /* 準備 (Arrange): Dcm_Init() 直後は defaultSession のまま */

    SendRoutineControl(DCM_ROUTINE_SUBFUNC_START);

    ExpectNegativeResponse(DCM_NRC_SERVICE_NOT_SUPPORTED_IN_SESSION);
}

TEST_F(Bsw_Dcm_RoutineControl_Test, RoutineControl_NG_UnsupportedRidReturnsRequestOutOfRange)
{
    EnterExtendedSession();

    SendRoutineControl(DCM_ROUTINE_SUBFUNC_START, 0xFFFFU);

    ExpectNegativeResponse(DCM_NRC_REQUEST_OUT_OF_RANGE);
}

}  // namespace
