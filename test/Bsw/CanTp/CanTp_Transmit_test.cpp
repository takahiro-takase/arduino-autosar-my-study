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
 * SetUp(): 時刻と、CanTp・Dcm・CanIf の wrap（呼び出し回数・故障注入。他のテストが設定したまま残っていても影響を受けないよう）を初期化し、CanTp_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。 */
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
        CanTp_Init(NULL);              // CfgPtr は常に NULL（CanTp.h 参照）
        FakeDetHw_Reset();              // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    // CanTp に DeInit() は無いため TearDown は書かない。未初期化状態が必要な
    // テストは各自 CanTp_Test_ResetInitState() を明示的に呼ぶ（ファイル冒頭
    // コメント参照）。
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

}  // namespace
