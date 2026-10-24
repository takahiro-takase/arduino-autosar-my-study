/**
 * \file    CanTp_RxIndication_test.cpp
 * \brief   CanTp_RxIndication() の単体テスト
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

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* CanTp の単体テスト用フィクスチャ。
 * SetUp(): 時刻と、CanTp・Dcm・CanIf の wrap（呼び出し回数・故障注入。他のテストが設定したまま残っていても影響を受けないよう）を初期化し、PduR への送信は成功する既定にして、CanTp_Init(NULL)・Dem_Init(NULL)・Dcm_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): ComM・PduR の wrap を戻し、Dcm を未初期化へ戻す（他のテストファイルへ影響を残さない）。 */
class CanTp_RxIndication_Test : public ::testing::Test
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

    // 解釈を調べる FC フレーム（CTS、BS=0、STmin=raw）を、FC 待ち状態の CanTp へ渡し、換算された STmin を返す。
    // 直前の値が残っていると結果が区別できないため、毎回 FC 待ち状態にし直す。
    uint8 ReceiveFcAndGetStMin(uint8 raw)
    {
        CanTp_Test_SetTxWaitFc();
        uint8 frame[8] = { 0x30U, 0x00U, raw, 0U, 0U, 0U, 0U, 0U };
        PduInfoType info = { frame, 8U };
        CanTp_RxIndication(CANTP_RX_SDU_ID, &info);
        return CanTp_Test_GetTxStMin();
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        WrapComM_Reset();           // 他のテストファイルへ影響を残さない
        WrapPduR_Reset();
        Dcm_Test_ResetInitState();
    }

    // FF（総長 10: 先頭 6 バイト＋ CF 1 本）を受け取ったことにする。中身は TesterPresent 風の 0x3E で、残りは 0。
    static void ReceiveFirstFrame()
    {
        uint8 frame[8] = { 0x10U, 0x0AU, 0x3EU, 0U, 0U, 0U, 0U, 0U };
        PduInfoType info = { frame, 8U };
        CanTp_RxIndication(CANTP_RX_SDU_ID, &info);
    }

    // CF（シーケンス番号 sn、データ部は 0）を、長さ len で受け取ったことにする。
    static void ReceiveConsecutiveFrame(uint8 sn, uint8 len)
    {
        uint8 frame[8] = { (uint8)(0x20U | sn), 0U, 0U, 0U, 0U, 0U, 0U, 0U };
        PduInfoType info = { frame, len };
        CanTp_RxIndication(CANTP_RX_SDU_ID, &info);
    }

    // FC（フロー状態 fs、STmin 5）を受け取ったことにする。
    static void ReceiveFlowControl(uint8 fs)
    {
        uint8 frame[8] = { (uint8)(0x30U | fs), 0U, 5U, 0U, 0U, 0U, 0U, 0U };
        PduInfoType info = { frame, 8U };
        CanTp_RxIndication(CANTP_RX_SDU_ID, &info);
    }

    // CanTp に DeInit() は無いため、未初期化状態が必要なテストは各自
    // CanTp_Test_ResetInitState() を明示的に呼ぶ（ファイル冒頭コメント参照）。
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(CanTp_RxIndication_Test, CanTp_RxIndication_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanTp_Test_ResetInitState();
    uint8 sdu[8] = { 0 };
    PduInfoType info = { sdu, 8U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTp_RxIndication(CANTP_RX_SDU_ID, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, CANTP_E_UNINIT);
}

TEST_F(CanTp_RxIndication_Test, CanTp_RxIndication_NG_InvalidRxPduId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 sdu[8] = { 0 };
    PduInfoType info = { sdu, 8U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTp_RxIndication(CANTP_RX_SDU_ID + 1U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, CANTP_E_INVALID_RX_ID);
}

TEST_F(CanTp_RxIndication_Test, CanTp_RxIndication_NG_NullPduInfoPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTp_RxIndication(CANTP_RX_SDU_ID, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, CANTP_E_PARAM_POINTER);
}

TEST_F(CanTp_RxIndication_Test, CanTp_RxIndication_NG_NullSduDataPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PduInfoType info = { NULL, 0U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTp_RxIndication(CANTP_RX_SDU_ID, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, CANTP_E_PARAM_POINTER);
}

TEST_F(CanTp_RxIndication_Test, CanTp_RxIndication_NG_SfPayloadLengthZero)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 frame[8] = { 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U };
    PduInfoType info = { frame, 8U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTp_RxIndication(CANTP_RX_SDU_ID, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Dcm_StartOfReception, 0U);   // 判定で弾かれ、上位層（Dcm）へ受信の開始を通知しない
}

TEST_F(CanTp_RxIndication_Test, CanTp_RxIndication_NG_SfPayloadLengthAboveMax)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // 宣言したペイロード長 8 バイトより、フレームが長い（16 バイト）ようにする。8 バイトのフレームだと、
    // 「フレームが宣言より短い」判定でも弾かれてしまい、ペイロード長の上限の判定を単独で確かめられない。
    uint8 frame[16] = { 0x08U };
    PduInfoType info = { frame, 16U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTp_RxIndication(CANTP_RX_SDU_ID, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Dcm_StartOfReception, 0U);   // 判定で弾かれ、上位層（Dcm）へ受信の開始を通知しない
}

TEST_F(CanTp_RxIndication_Test, CanTp_RxIndication_NG_SfFrameShorterThanDeclaredLength)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 frame[8] = { 0x05U, 0x01U, 0x02U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U };
    PduInfoType info = { frame, 3U };   // 5 バイトのペイロードを宣言しているのに、フレームは 3 バイトしかない

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTp_RxIndication(CANTP_RX_SDU_ID, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Dcm_StartOfReception, 0U);   // 判定で弾かれ、上位層（Dcm）へ受信の開始を通知しない
}

TEST_F(CanTp_RxIndication_Test, CanTp_RxIndication_NG_SfShorterThanEightBytesIsRejectedWithPaddingError)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 frame[8] = { 0x02U, 0x10U, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U };
    PduInfoType info = { frame, 3U };   // ペイロード長（2）とは合っているが、DLC が 8 未満（パディングされていない）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTp_RxIndication(CANTP_RX_SDU_ID, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Dcm_StartOfReception, 0U);   // 受け付けない（[SWS_CanTp_00345]）
    EXPECT_EQ(FakeDetHw_LastApiId, CANTP_API_ID_RX_INDICATION);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANTP_E_PADDING);
}

TEST_F(CanTp_RxIndication_Test, CanTp_RxIndication_NG_FfMessageLengthFitsSingleFrame)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 frame[8] = { 0x10U, 0x07U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U };
    PduInfoType info = { frame, 8U };   // 総長 7 は Single Frame で送れる長さ

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTp_RxIndication(CANTP_RX_SDU_ID, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Dcm_StartOfReception, 0U);   // 判定で弾かれ、上位層（Dcm）へ受信の開始を通知しない
}

TEST_F(CanTp_RxIndication_Test, CanTp_RxIndication_NG_FfFrameShorterThanEightBytes)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 frame[8] = { 0x10U, 0x14U, 0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U };
    PduInfoType info = { frame, 7U };   // FF は 8 バイト必要

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTp_RxIndication(CANTP_RX_SDU_ID, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Dcm_StartOfReception, 0U);   // 判定で弾かれ、上位層（Dcm）へ受信の開始を通知しない
}

TEST_F(CanTp_RxIndication_Test, CanTp_RxIndication_NG_FfWhileReceivingAbortsPreviousReception)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ReceiveFirstFrame();   // 1 つ目の受信（CF 待ち）
    ASSERT_EQ(CallCount_Dcm_StartOfReception, 1U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ReceiveFirstFrame();   // CF を待っている途中で、新しい FF が届く

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Dcm_TpRxIndication, 1U);
    EXPECT_EQ(LastResult_Dcm_TpRxIndication, E_NOT_OK);   // 古い受信は、中断として上位層へ通知する
    EXPECT_EQ(CallCount_Dcm_StartOfReception, 2U);        // 新しい受信は始める
}

TEST_F(CanTp_RxIndication_Test, CanTp_RxIndication_NG_CfWithoutReceptionIsIgnored)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // 受信中ではない（FF を受け取っていない）。

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ReceiveConsecutiveFrame(1U, 8U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Dcm_CopyRxData, 0U);
    EXPECT_EQ(CallCount_Dcm_TpRxIndication, 0U);
}

TEST_F(CanTp_RxIndication_Test, CanTp_RxIndication_NG_CfShorterThanEightBytesAbortsReception)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ReceiveFirstFrame();
    ASSERT_EQ(CallCount_Dcm_CopyRxData, 1U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ReceiveConsecutiveFrame(1U, 7U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Dcm_CopyRxData, 1U);   // データを渡さない
    EXPECT_EQ(CallCount_Dcm_TpRxIndication, 1U);
    EXPECT_EQ(LastResult_Dcm_TpRxIndication, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastApiId, CANTP_API_ID_RX_INDICATION);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANTP_E_PADDING);   // [SWS_CanTp_00346]
}

TEST_F(CanTp_RxIndication_Test, CanTp_RxIndication_NG_CfWithWrongSequenceNumberAbortsReception)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ReceiveFirstFrame();
    ASSERT_EQ(CallCount_Dcm_CopyRxData, 1U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ReceiveConsecutiveFrame(2U, 8U);   // 次に受け取るべき番号は 1

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Dcm_CopyRxData, 1U);   // データを渡さない
    EXPECT_EQ(CallCount_Dcm_TpRxIndication, 1U);
    EXPECT_EQ(LastResult_Dcm_TpRxIndication, E_NOT_OK);
}

TEST_F(CanTp_RxIndication_Test, CanTp_RxIndication_NG_CfRejectedByUpperLayerAbortsReception)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ReceiveFirstFrame();
    Dcm_Init(NULL);   // Dcm が受信中ではなくなる（Dcm_CopyRxData() が失敗する）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ReceiveConsecutiveFrame(1U, 8U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Dcm_CopyRxData, 2U);
    EXPECT_EQ(CallCount_Dcm_TpRxIndication, 1U);
    EXPECT_EQ(LastResult_Dcm_TpRxIndication, E_NOT_OK);
}

TEST_F(CanTp_RxIndication_Test, CanTp_RxIndication_NG_FcWithoutWaitingIsIgnored)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // TX は FC を待っていない（初期状態）。
    const uint8 before = CanTp_Test_GetTxStMin();   // 直前のテストの値が残っていてもよいよう、変化の有無で見る

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ReceiveFlowControl(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CanTp_Test_GetTxStMin(), before);   // STmin（5）を取り込まない
    EXPECT_FALSE(CanTp_Test_IsTxBusy());
}

TEST_F(CanTp_RxIndication_Test, CanTp_RxIndication_NG_FcShorterThanEightBytesAbortsTransmission)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanTp_Test_SetTxWaitFc();
    const uint8 before = CanTp_Test_GetTxStMin();   // 直前のテストの値が残っていてもよいよう、変化の有無で見る
    uint8 frame[8] = { 0x30U, 0U, 5U, 0U, 0U, 0U, 0U, 0U };
    PduInfoType info = { frame, 3U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTp_RxIndication(CANTP_RX_SDU_ID, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CanTp_Test_GetTxStMin(), before);   // STmin（5）を取り込まない
    EXPECT_EQ(CallCount_Dcm_TpTxConfirmation, 1U);   // 送信セッションを中断し、上位層へ失敗を伝える（[SWS_CanTp_00349]）
    EXPECT_EQ(LastResult_Dcm_TpTxConfirmation, E_NOT_OK);
    EXPECT_FALSE(CanTp_Test_IsTxBusy());
    EXPECT_EQ(FakeDetHw_LastApiId, CANTP_API_ID_RX_INDICATION);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANTP_E_PADDING);
}

TEST_F(CanTp_RxIndication_Test, CanTp_RxIndication_NG_FcOverflowAbortsTransmission)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanTp_Test_SetTxWaitFc();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ReceiveFlowControl(2U);   // OVFLW

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Dcm_TpTxConfirmation, 1U);
    EXPECT_EQ(LastResult_Dcm_TpTxConfirmation, E_NOT_OK);
    EXPECT_FALSE(CanTp_Test_IsTxBusy());
}

TEST_F(CanTp_RxIndication_Test, CanTp_RxIndication_NG_UnknownFrameTypeIsIgnored)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 frame[8] = { 0x40U, 0U, 0U, 0U, 0U, 0U, 0U, 0U };   // 上位 4 ビットが SF/FF/CF/FC のどれでもない
    PduInfoType info = { frame, 8U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTp_RxIndication(CANTP_RX_SDU_ID, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Dcm_StartOfReception, 0U);
    EXPECT_EQ(CallCount_Dcm_CopyRxData, 0U);
    EXPECT_EQ(CallCount_Dcm_TpRxIndication, 0U);
}

TEST_F(CanTp_RxIndication_Test, CanTp_RxIndication_NG_EmptyFrameIsIgnored)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 frame[8] = { 0x02U, 0x10U, 0x01U, 0U, 0U, 0U, 0U, 0U };   // 長さが 0 なら、中身は見ない
    PduInfoType info = { frame, 0U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTp_RxIndication(CANTP_RX_SDU_ID, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Dcm_StartOfReception, 0U);
    EXPECT_EQ(FakeDetHw_LastErrorId, 0xFFU);   // DET も報告しない
}

TEST_F(CanTp_RxIndication_Test, CanTp_RxIndication_NG_FfRejectedByUpperLayerStartsNoReception)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dcm_Test_ResetInitState();   // Dcm が受信を受け付けない（Dcm_StartOfReception() が失敗する）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ReceiveFirstFrame();
    ReceiveConsecutiveFrame(1U, 8U);   // 受信は始まっていないので、CF は無視される

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Dcm_StartOfReception, 1U);
    EXPECT_EQ(CallCount_Dcm_CopyRxData, 0U);        // FF のデータも渡さない
    EXPECT_EQ(CallCount_Dcm_TpRxIndication, 0U);    // 受け付けられなかった受信は、完了も中断も通知しない
}

TEST_F(CanTp_RxIndication_Test, CanTp_RxIndication_OK_SfValidFrameStartsReception)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 frame[8] = { 0x02U, 0x10U, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U };
    PduInfoType info = { frame, 8U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTp_RxIndication(CANTP_RX_SDU_ID, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Dcm_StartOfReception, 1U);
    EXPECT_EQ(LastTpSduLength_Dcm_StartOfReception, 2U);   // SF のペイロード長（PCI の下位 4 ビット）
}

TEST_F(CanTp_RxIndication_Test, CanTp_RxIndication_OK_FfValidFrameStartsReception)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 frame[8] = { 0x10U, 0x14U, 0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U };
    PduInfoType info = { frame, 8U };   // 総長 20（0x014）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTp_RxIndication(CANTP_RX_SDU_ID, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Dcm_StartOfReception, 1U);
    EXPECT_EQ(LastTpSduLength_Dcm_StartOfReception, 20U);   // FF の長さ欄（12 ビット）
}

TEST_F(CanTp_RxIndication_Test, CanTp_RxIndication_OK_FcStMinDirectValueIsKept)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（FC 待ち状態は ReceiveFcAndGetStMin() で作る）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const uint8 stMin = ReceiveFcAndGetStMin(0x05U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(stMin, 5U);   // 0x00〜0x7F はミリ秒そのもの
}

TEST_F(CanTp_RxIndication_Test, CanTp_RxIndication_OK_FcStMinMicrosecondRangeBecomesZero)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(ReceiveFcAndGetStMin(0x20U), 32U);   // 直前の値を 0 以外にしておく

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const uint8 stMin = ReceiveFcAndGetStMin(0xF5U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(stMin, 0U);   // 0xF1〜0xF9（100〜900 マイクロ秒）は 0ms に換算する
}

TEST_F(CanTp_RxIndication_Test, CanTp_RxIndication_OK_FcStMinReservedValueIsClampedTo127)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(ReceiveFcAndGetStMin(0x05U), 5U);   // 直前の値を 127 以外にしておく

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const uint8 stMinBelowMicrosecondRange = ReceiveFcAndGetStMin(0xF0U);
    ASSERT_EQ(ReceiveFcAndGetStMin(0x05U), 5U);
    const uint8 stMinAboveMicrosecondRange = ReceiveFcAndGetStMin(0xFAU);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(stMinBelowMicrosecondRange, 127U);   // 0xF0（規格の予約値）
    EXPECT_EQ(stMinAboveMicrosecondRange, 127U);   // 0xFA（規格の予約値）
}

TEST_F(CanTp_RxIndication_Test, CanTp_RxIndication_OK_MultiFrameReceptionDeliversWholeMessage)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ReceiveFirstFrame();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ReceiveConsecutiveFrame(1U, 8U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Dcm_CopyRxData, 2U);              // FF の 6 バイトと、CF の残り 4 バイト
    EXPECT_EQ(CallCount_Dcm_TpRxIndication, 1U);
    EXPECT_EQ(LastResult_Dcm_TpRxIndication, E_OK);
}

}  // namespace
