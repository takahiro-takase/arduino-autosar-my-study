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
 * SetUp(): 時刻と、CanTp・Dcm・CanIf の wrap（呼び出し回数・故障注入。他のテストが設定したまま残っていても影響を受けないよう）を初期化し、CanTp_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。 */
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
        CanTp_Init(NULL);              // CfgPtr は常に NULL（CanTp.h 参照）
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

    // CanTp に DeInit() は無いため TearDown は書かない。未初期化状態が必要な
    // テストは各自 CanTp_Test_ResetInitState() を明示的に呼ぶ（ファイル冒頭
    // コメント参照）。
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

}  // namespace
