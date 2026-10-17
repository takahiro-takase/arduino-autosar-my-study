/**
 * \file    CanTp_MainFunction_test.cpp
 * \brief   CanTp_MainFunction() の単体テスト
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

/* CanTp の単体テスト用フィクスチャ。
 * SetUp(): 時刻と CanTp の wrap（呼び出し回数・故障注入。他のテストが設定したまま残っていても影響を受けないよう）を初期化し、PduR への送信は成功する既定にして、CanTp_Init(NULL)・Dem_Init(NULL)・Dcm_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): ComM・PduR の wrap を戻し、Dcm を未初期化へ戻す（他のテストファイルへ影響を残さない）。 */
class CanTp_MainFunction_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        WrapCanTp_Reset();
        WrapDcm_Reset();
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

    // FC フレームを、CanTp が受け取ったことにする。
    static void ReceiveFlowControl(uint8 fs, uint8 bs, uint8 stMin)
    {
        uint8 frame[8] = { (uint8)(0x30U | fs), bs, stMin, 0U, 0U, 0U, 0U, 0U };
        PduInfoType info = { frame, 8U };
        CanTp_RxIndication(CANTP_RX_SDU_ID, &info);
    }

    // FF（総長 10: 先頭 6 バイト＋ CF 1 本）を、CanTp が受け取ったことにする。中身は TesterPresent 風の 0x3E で、残りは 0。
    static void ReceiveFirstFrame()
    {
        uint8 frame[8] = { 0x10U, 0x0AU, 0x3EU, 0U, 0U, 0U, 0U, 0U };
        PduInfoType info = { frame, 8U };
        CanTp_RxIndication(CANTP_RX_SDU_ID, &info);
    }

    // VIN の応答（20 バイト）を FF まで送り、FC（続行、ブロックサイズ bs、STmin st）を受け取った状態にする。
    void StartVinTransmitAfterFlowControl(uint8 bs, uint8 st, uint16 sduLen = 20U)
    {
        FakeMillis_Value = 100UL;   // 0 は「タイマ未設定」と区別できないため、0 以外から始める
        MakeDcmSendingVinResponse();
        ASSERT_EQ(Transmit(sduLen), E_OK);
        ReceiveFlowControl(0U, bs, st);
    }

    // CanTp に DeInit() は無いため、未初期化状態が必要なテストは各自
    // CanTp_Test_ResetInitState() を明示的に呼ぶ（ファイル冒頭コメント参照）。
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(CanTp_MainFunction_Test, CanTp_MainFunction_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanTp_Test_ResetInitState();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTp_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, CANTP_E_UNINIT);
}

TEST_F(CanTp_MainFunction_Test, CanTp_MainFunction_NG_TxAbortsWhenNoFlowControlArrivesWithinNBs)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FakeMillis_Value = 100UL;
    MakeDcmSendingVinResponse();
    ASSERT_EQ(Transmit(20U), E_OK);   // FF を送って FC を待つ
    FakeMillis_Value += 5000UL;       // N_Bs（5000 ms）が経過する

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTp_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Dcm_TpTxConfirmation, 1U);
    EXPECT_EQ(LastResult_Dcm_TpTxConfirmation, E_NOT_OK);   // 送信失敗として上位層へ通知する
    EXPECT_FALSE(CanTp_Test_IsTxBusy());
}

TEST_F(CanTp_MainFunction_Test, CanTp_MainFunction_NG_RxAbortsWhenNoConsecutiveFrameArrivesWithinNCr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FakeMillis_Value = 100UL;
    ReceiveFirstFrame();   // CF を待つ
    FakeMillis_Value += 1000UL;   // N_Cr（1000 ms）が経過する

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTp_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Dcm_TpRxIndication, 1U);
    EXPECT_EQ(LastResult_Dcm_TpRxIndication, E_NOT_OK);   // 受信失敗として上位層へ通知する
}

TEST_F(CanTp_MainFunction_Test, CanTp_MainFunction_NG_TxAbortsWhenUpperLayerHasNoMoreDataForNCs)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // Dcm の応答は 20 バイトだが、CanTp へは 30 バイトの送信を指示する（CF の 3 本目で、Dcm にデータが無くなる）。
    StartVinTransmitAfterFlowControl(0U, 0U, 30U);
    CanTp_MainFunction();   // CF 1 本目
    CanTp_MainFunction();   // CF 2 本目
    CanTp_MainFunction();   // CF 3 本目: データが無い（待つ）
    ASSERT_EQ(CallCount_Dcm_TpTxConfirmation, 0U);
    FakeMillis_Value += 1000UL;   // データが無い状態が N_As/N_Cs（1000 ms）続く

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTp_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Dcm_TpTxConfirmation, 1U);
    EXPECT_EQ(LastResult_Dcm_TpTxConfirmation, E_NOT_OK);
    EXPECT_FALSE(CanTp_Test_IsTxBusy());
}

TEST_F(CanTp_MainFunction_Test, CanTp_MainFunction_NG_TxAbortsWhenUpperLayerCopyFails)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    StartVinTransmitAfterFlowControl(0U, 0U);
    Dcm_Init(NULL);   // Dcm が応答の送信中ではなくなる（Dcm_CopyTxData() が失敗する）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTp_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Dcm_TpTxConfirmation, 1U);
    EXPECT_EQ(LastResult_Dcm_TpTxConfirmation, E_NOT_OK);
    EXPECT_FALSE(CanTp_Test_IsTxBusy());
}

TEST_F(CanTp_MainFunction_Test, CanTp_MainFunction_NG_TxAbortsWhenFrameSendKeepsFailingForNAs)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    StartVinTransmitAfterFlowControl(0U, 0U);
    ForcedReturn_PduR_CanTpTransmit = E_NOT_OK;   // FF は送れたが、これ以降の CF の送信は失敗し続ける
    CanTp_MainFunction();   // CF の送信に失敗（再送する）
    ASSERT_EQ(CallCount_Dcm_TpTxConfirmation, 0U);
    ASSERT_TRUE(CanTp_Test_IsTxBusy());
    FakeMillis_Value += 1000UL;   // 失敗が N_As（1000 ms）続く

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTp_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Dcm_TpTxConfirmation, 1U);
    EXPECT_EQ(LastResult_Dcm_TpTxConfirmation, E_NOT_OK);
    EXPECT_FALSE(CanTp_Test_IsTxBusy());
}

TEST_F(CanTp_MainFunction_Test, CanTp_MainFunction_OK_NoRxTimeoutJustBeforeNCr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FakeMillis_Value = 100UL;
    ReceiveFirstFrame();   // CF 待ち（N_Cr 1000 ms）
    FakeMillis_Value += 999UL;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTp_MainFunction();   // N_Cr まであと 1 ms

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Dcm_TpRxIndication, 0U);
}

TEST_F(CanTp_MainFunction_Test, CanTp_MainFunction_OK_NoTxTimeoutJustBeforeNBs)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FakeMillis_Value = 100UL;
    MakeDcmSendingVinResponse();
    ASSERT_EQ(Transmit(20U), E_OK);   // FC 待ち（N_Bs 5000 ms）
    FakeMillis_Value += 4999UL;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTp_MainFunction();   // N_Bs まであと 1 ms

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_TRUE(CanTp_Test_IsTxBusy());
    EXPECT_EQ(CallCount_Dcm_TpTxConfirmation, 0U);
}

TEST_F(CanTp_MainFunction_Test, CanTp_MainFunction_OK_FlowControlWaitRestartsNBsTimer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FakeMillis_Value = 100UL;
    MakeDcmSendingVinResponse();
    ASSERT_EQ(Transmit(20U), E_OK);   // FC 待ち
    FakeMillis_Value += 4000UL;
    ReceiveFlowControl(1U, 0U, 0U);   // FC（WAIT）: 待ち時間を数え直す
    FakeMillis_Value += 4000UL;       // FF からは 8000 ms だが、WAIT からは 4000 ms

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTp_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_TRUE(CanTp_Test_IsTxBusy());
    EXPECT_EQ(CallCount_Dcm_TpTxConfirmation, 0U);
}

TEST_F(CanTp_MainFunction_Test, CanTp_MainFunction_OK_TxCompletesWithConsecutiveFrames)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    StartVinTransmitAfterFlowControl(0U, 0U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTp_MainFunction();   // CF 1 本目
    CanTp_MainFunction();   // CF 2 本目（全て送り終える）

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_PduR_CanTpTransmit, 3U);   // FF ＋ CF 2 本
    EXPECT_EQ(CallCount_Dcm_TpTxConfirmation, 1U);
    EXPECT_EQ(LastResult_Dcm_TpTxConfirmation, E_OK);
    EXPECT_FALSE(CanTp_Test_IsTxBusy());
}

TEST_F(CanTp_MainFunction_Test, CanTp_MainFunction_OK_BlockSizeWaitsForNextFlowControl)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    StartVinTransmitAfterFlowControl(1U, 0U);   // ブロックサイズ 1（CF 1 本ごとに FC を待つ）
    CanTp_MainFunction();   // CF 1 本目
    CanTp_MainFunction();   // FC 待ちのため、送らない
    ASSERT_EQ(CallCount_PduR_CanTpTransmit, 2U);
    ReceiveFlowControl(0U, 1U, 0U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTp_MainFunction();   // CF 2 本目

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_PduR_CanTpTransmit, 3U);
    EXPECT_EQ(CallCount_Dcm_TpTxConfirmation, 1U);
    EXPECT_EQ(LastResult_Dcm_TpTxConfirmation, E_OK);
}

TEST_F(CanTp_MainFunction_Test, CanTp_MainFunction_OK_StMinDelaysConsecutiveFrame)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    StartVinTransmitAfterFlowControl(0U, 10U);   // STmin 10 ms
    CanTp_MainFunction();   // まだ 10 ms 経っていない
    ASSERT_EQ(CallCount_PduR_CanTpTransmit, 1U);
    FakeMillis_Value += 10UL;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTp_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_PduR_CanTpTransmit, 2U);   // FF ＋ CF 1 本目
}

TEST_F(CanTp_MainFunction_Test, CanTp_MainFunction_OK_FailedConsecutiveFrameIsResentUnchanged)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    StartVinTransmitAfterFlowControl(0U, 0U);
    ForcedReturn_PduR_CanTpTransmit = E_NOT_OK;   // CF 1 本目の送信が 1 回失敗する
    CanTp_MainFunction();   // 失敗（組み立て済みの CF を保持する）
    ForcedReturn_PduR_CanTpTransmit = E_OK;       // 以降は成功する

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTp_MainFunction();   // 同じ CF を再送する（上位層からデータを取り直さない）
    CanTp_MainFunction();   // CF 2 本目

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Dcm_TpTxConfirmation, 1U);
    EXPECT_EQ(LastResult_Dcm_TpTxConfirmation, E_OK);   // 取り直していれば、2 本目でデータが足りず完了しない
}

}  // namespace
