/**
 * \file    CanTp_Transmit_test.cpp
 * \brief   CanTp_Transmit() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 CanTp_test.cpp）。
 *
 *          分割前の共通の説明は、旧 CanTp_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "CanTp.h"
#include "Fake_Det_Hw.h"
#include "Fake_Millis.h"
#include "Wrap_CanTp.h"
#include "Wrap_PduR.h"
#include "Wrap_ComM.h"
#include "Wrap_Dcm.h"
#include "Dcm.h"
#include "Dcm_Cfg.h"
#include "Dem.h"
#include "Wrap_Dcm.h"
#include "Wrap_CanIf.h"
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

/* FF の長さ欄（12 bit）で表せる N-SDU の最大長。CanTp.c 内の private なマクロ CANTP_MAX_MSG_LEN と同じ値
 * （ヘッダーには出ていないため、テスト側にも同じ値を持つ）。 */
constexpr uint16 kMaxMsgLen = 4095U;

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* CanTp の単体テスト用フィクスチャ。
 * SetUp(): 時刻と、CanTp・Dcm・CanIf の wrap（呼び出し回数・故障注入。他のテストが設定したまま残っていても影響を受けないよう）を初期化し、PduR への送信は成功する既定にして、CanTp_Init(NULL)・Dem_Init(NULL)・Dcm_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): ComM・PduR の wrap を戻し、Dcm を未初期化へ戻す（他のテストファイルへ影響を残さない）。 */
class CanTp_Transmit_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        WrapCanTp_Reset();
        WrapDcm_Reset();
        WrapCanIf_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        WrapPduR_Reset();
        FailFromCallCount_PduR_CanTpTransmit = 0U;   // 既定: CAN フレームの送信（PduR 経由）は成功する
        ForcedReturn_PduR_CanTpTransmit      = E_OK;
        Suppressed_ComM_DcmDiagnostic        = 1U;   // 本テストは通信管理(ComM/CanSM/CanNm)が対象外
        CanTp_Init(NULL);              // CfgPtr は常に NULL（CanTp.h 参照）
        Dem_Init(NULL);
        Dcm_Init(NULL);                // 上位層（Dcm）は実体。CanTp から渡るデータを、実際に受け取る・取り出す
        FakeDetHw_Reset();              // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        WrapComM_Reset();           // 他のテストファイルへ影響を残さない
        WrapPduR_Reset();
        Dcm_Test_ResetInitState();
    }

    // Dcm に、応答（req への返答）を CanTp で送信している最中の状態を作る。
    // Dcm から CanTp への送信は成功したことにし、送信の確認は返さない。その後は、実体の CanTp_Transmit() が呼ばれる。
    void MakeDcmSendingResponse(const uint8* req, uint8 len)
    {
        FailFromCallCount_CanTp_Transmit    = 0U;
        ForcedReturn_CanTp_Transmit         = E_OK;
        SuppressConfirmation_CanTp_Transmit = 1U;
        (void)WrapDcm_InjectRequest(req, len);
        FailFromCallCount_CanTp_Transmit    = WRAP_CANTP_FAIL_FROM_CALL_COUNT_DISABLED;
        SuppressConfirmation_CanTp_Transmit = 0U;
    }

    // VIN の読み出し（応答 20 バイト: FF と CF 2 本で運ぶ長さ）の応答を送信している最中にする。
    void MakeDcmSendingVinResponse()
    {
        const uint8 req[3] = { DCM_SID_READ_DATA, (uint8)(DCM_DID_VIN >> 8U), (uint8)(DCM_DID_VIN & 0xFFU) };
        MakeDcmSendingResponse(req, sizeof(req));
    }

    // 診断セッション切り替え（defaultSession）の応答（6 バイト: SF で運べる長さ）を送信している最中にする。
    void MakeDcmSendingSessionResponse()
    {
        const uint8 req[2] = { DCM_SID_SESSION_CTRL, DCM_SESSION_DEFAULT };
        MakeDcmSendingResponse(req, sizeof(req));
    }

    // Dcm の応答のうち len バイトの送信を CanTp へ指示する。
    static Std_ReturnType Transmit(uint16 len)
    {
        uint8 dummy[1] = { 0U };
        PduInfoType info = { dummy, len };
        return CanTp_Transmit(CANTP_TX_SDU_ID, &info);
    }

    // CanTp に DeInit() は無いため、未初期化状態が必要なテストは各自
    // CanTp_Test_ResetInitState() を明示的に呼ぶ（ファイル冒頭コメント参照）。
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(CanTp_Transmit_Test, CanTp_Transmit_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanTp_Test_ResetInitState();
    uint8 sdu[4] = { 0 };
    PduInfoType info = { sdu, 4U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CanTp_Transmit(CANTP_TX_SDU_ID, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANTP_E_UNINIT);
}

TEST_F(CanTp_Transmit_Test, CanTp_Transmit_NG_InvalidTxSduId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 sdu[4] = { 0 };
    PduInfoType info = { sdu, 4U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CanTp_Transmit(CANTP_TX_SDU_ID + 1U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANTP_E_INVALID_TX_ID);
}

TEST_F(CanTp_Transmit_Test, CanTp_Transmit_NG_NullPduInfoPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CanTp_Transmit(CANTP_TX_SDU_ID, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANTP_E_PARAM_POINTER);
}

TEST_F(CanTp_Transmit_Test, CanTp_Transmit_NG_MsgLengthZero)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 sdu[4] = { 0 };
    PduInfoType info = { sdu, 0U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CanTp_Transmit(CANTP_TX_SDU_ID, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(CallCount_Dcm_CopyTxData, 0U);   // 長さの確認で弾かれ、データ取得へ進まない
    EXPECT_EQ(CallCount_CanIf_Transmit, 0U);   // フレームも送らない
}

TEST_F(CanTp_Transmit_Test, CanTp_Transmit_NG_MsgLengthAboveMax)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 sdu[4] = { 0 };
    PduInfoType info = { sdu, kMaxMsgLen + 1U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CanTp_Transmit(CANTP_TX_SDU_ID, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(CallCount_Dcm_CopyTxData, 0U);   // 長さの確認で弾かれ、データ取得へ進まない
    EXPECT_EQ(CallCount_CanIf_Transmit, 0U);   // フレームも送らない
}

TEST_F(CanTp_Transmit_Test, CanTp_Transmit_NG_ChannelBusyWhileMultiFrameInProgress)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    MakeDcmSendingVinResponse();
    ASSERT_EQ(Transmit(20U), E_OK);   // マルチフレーム送信を始める（FC 待ち）
    ASSERT_EQ(CallCount_PduR_CanTpTransmit, 1U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Transmit(20U);   // 送信中にもう 1 つ送信を指示する

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(CallCount_PduR_CanTpTransmit, 1U);   // 2 つ目のフレームは送らない
}

TEST_F(CanTp_Transmit_Test, CanTp_Transmit_NG_SfCopyTxDataFailsSendsNothing)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // Dcm は応答を送信している最中ではないので、Dcm_CopyTxData() は失敗する。

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Transmit(6U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(CallCount_PduR_CanTpTransmit, 0U);
}

TEST_F(CanTp_Transmit_Test, CanTp_Transmit_NG_FfCopyTxDataFailsSendsNothing)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // Dcm は応答を送信している最中ではないので、Dcm_CopyTxData() は失敗する。

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Transmit(20U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_FALSE(CanTp_Test_IsTxBusy());
    EXPECT_EQ(CallCount_PduR_CanTpTransmit, 0U);
}

TEST_F(CanTp_Transmit_Test, CanTp_Transmit_NG_SfFrameSendFailureIsNotConfirmedToUpperLayer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    MakeDcmSendingSessionResponse();
    ForcedReturn_PduR_CanTpTransmit = E_NOT_OK;   // CAN フレームの送信が失敗する

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Transmit(6U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);                          // 呼び出し元（Dcm）へ、送信できなかったことを返す
    EXPECT_EQ(CallCount_Dcm_TpTxConfirmation, 0U);     // 送信完了としては通知しない
}

TEST_F(CanTp_Transmit_Test, CanTp_Transmit_NG_FfFrameSendFailureReleasesChannel)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    MakeDcmSendingVinResponse();
    ForcedReturn_PduR_CanTpTransmit = E_NOT_OK;   // CAN フレームの送信が失敗する

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Transmit(20U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_FALSE(CanTp_Test_IsTxBusy());   // FC を待たない（次の送信を受け付けられる）
}

TEST_F(CanTp_Transmit_Test, CanTp_Transmit_OK_SfIsSentAndConfirmedImmediately)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    MakeDcmSendingSessionResponse();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Transmit(6U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(CallCount_PduR_CanTpTransmit, 1U);
    EXPECT_EQ(CallCount_Dcm_TpTxConfirmation, 1U);   // SF は、送ったら終わり
    EXPECT_EQ(LastResult_Dcm_TpTxConfirmation, E_OK);
    EXPECT_FALSE(CanTp_Test_IsTxBusy());
}

TEST_F(CanTp_Transmit_Test, CanTp_Transmit_OK_FfIsSentAndWaitsForFlowControl)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    MakeDcmSendingVinResponse();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Transmit(20U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(CallCount_PduR_CanTpTransmit, 1U);
    EXPECT_TRUE(CanTp_Test_IsTxBusy());              // FC を待つ
    EXPECT_EQ(CallCount_Dcm_TpTxConfirmation, 0U);   // まだ送信は終わっていない
}

}  // namespace
