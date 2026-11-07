/**
 * \file    ComM_GetCurrentComMode_test.cpp
 * \brief   ComM_GetCurrentComMode() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 ComM_test.cpp）。
 *
 *          分割前の共通の説明は、旧 ComM_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "ComM.h"
#include "CanSM.h"
#include "Fake_Det_Hw.h"
#include "Wrap_CanSM.h"
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

/* 「ComM_RequestComMode() で設定した値をそのまま返す」という正常系は、
 * ここでは意図的に検証しない: ComM_RequestComMode(FULL_COM) は
 * ComM_ApplyAggregatedRequest() 経由で実体の CanSM_RequestComMode() →
 * CanIf_SetControllerMode() までカスケードする。CanSM/CanIf/Can は
 * native_chain バイナリ内で他のテストファイル（Bsw_NmStack_SleepCoordination_
 * test.cpp 等）と static な初期化状態を共有しており、実行順序次第で
 * CanSM が既に初期化済み（かつ実 HW 前提の内部状態）のままこの呼び出しに
 * 到達し、ハングする実害を確認した（実測: 177秒でタイムアウト）。
 * PduR_ConfigPtr 等と同じ「バイナリ全体で共有される static、かつ
 * DeInit で完全には戻らない」既知の危険パターン（test/Bsw/PduR/ の
 * PduR_SecOCTxConfirmation セクション冒頭コメント参照）のため、この正常系は
 * 意図的に省略する。 */

/**
 * \details 上記2セクションが未カバーだった残り全ての `Det_ReportError()`
 *          呼び出し箇所（NG ケースのみ）をまとめる。各ケースで、報告される
 *          ErrorId が仕様どおり正しい値になっていることを `Fake_Det_Hw.h`
 *          （`FakeDetHw_LastErrorId`）で検証する。
 *
 *          ComM には `ComM_DeInit()` が存在するため、Mcu/PduR/CanTp と異なり
 *          「未初期化状態」の検証にテスト専用のリセット関数は不要（各テストが
 *          明示的に `ComM_DeInit()` を呼んでから検証する）。
 *
 *          上記 ComM_GetRequestedComMode セクション末尾コメントが指摘する
 *          とおり、ComM の「有効なパラメータ」経路は実体リンクされた CanSM/
 *          CanIf/Can へカスケードしうる（他テストファイルの残留 static 状態と
 *          衝突すると native_chain_tests 全体がハングしうる、
 *          `feedback_native_chain_shared_static_hang` 参照）。本セクションは
 *          Uninit/WrongParameters/ParamPointer のいずれも、そのカスケードへ
 *          到達する前（ComM.c 冒頭の検証）で reject される分岐のみを検証する
 *          ため、CanSM/CanIf/Can の初期化は一切行わない。
 */

/* ComM（上記 2 グループ以外の API）の単体テスト用フィクスチャ。
 * SetUp(): ComM_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): ComM_DeInit() で未初期化へ戻す。 */
class ComM_GetCurrentComMode_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        WrapCanSM_Reset();
        ComM_Init(NULL);
        FakeDetHw_Reset();              // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        ComM_DeInit();
        CanSM_DeInit();   // OK ケースが初期化した CanSM を、未初期化へ戻す（未初期化でも呼べる）
        WrapCanSM_Reset();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(ComM_GetCurrentComMode_Test, ComM_GetCurrentComMode_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ComM_DeInit();
    ComM_ModeType mode;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = ComM_GetCurrentComMode(0U, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_UNINIT);
}

TEST_F(ComM_GetCurrentComMode_Test, ComM_GetCurrentComMode_NG_WrongParameters)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ComM_ModeType mode;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = ComM_GetCurrentComMode(COMM_USER_COUNT, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_WRONG_PARAMETERS);
}

TEST_F(ComM_GetCurrentComMode_Test, ComM_GetCurrentComMode_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = ComM_GetCurrentComMode(0U, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_PARAM_POINTER);
}

TEST_F(ComM_GetCurrentComMode_Test, ComM_GetCurrentComMode_OK_AsksCanSM)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    /* ComM が通知から保存した値ではなく、CanSM へ問い合わせた値を返すことを見る（[SWS_ComM_00084]）。
     * CanSM_Init() 直後の CanSM は NO_COM（CanSM_GetCurrentComMode() はカスケードせず、状態を読むだけ）。 */
    FakeDetHw_LogSuppressed = 1U;
    CanSM_Init(NULL);
    FakeDetHw_LogSuppressed = 0U;
    ComM_ModeType mode = COMM_FULL_COMMUNICATION;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = ComM_GetCurrentComMode(COMM_USER_0, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(mode, COMM_NO_COMMUNICATION);
    EXPECT_EQ(CallCount_CanSM_GetCurrentComMode, COMM_CHANNEL_COUNT);   // チャネルごとに 1 回、CanSM へ問い合わせた
}

TEST_F(ComM_GetCurrentComMode_Test, ComM_GetCurrentComMode_OK_ReturnsCanSMModeNotStoredMode)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    /* ComM が保存したチャネルのモード（SILENT_COM）と、CanSM が持つモード（NO_COM）を、わざと食い違わせる。
     * 保存した値を返す実装なら SILENT_COM が、CanSM へ問い合わせる実装なら NO_COM が返る。 */
    FakeDetHw_LogSuppressed = 1U;
    CanSM_Init(NULL);
    ComM_BusSM_ModeIndication(0U, COMM_SILENT_COMMUNICATION);
    FakeDetHw_LogSuppressed = 0U;
    ComM_ModeType mode = COMM_FULL_COMMUNICATION;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = ComM_GetCurrentComMode(COMM_USER_0, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(mode, COMM_NO_COMMUNICATION);
}

TEST_F(ComM_GetCurrentComMode_Test, ComM_GetCurrentComMode_NG_CanSMFails)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    /* CanSM が未初期化だと、CanSM_GetCurrentComMode() が E_NOT_OK を返す。他のテストが初期化した CanSM が残っていても、
     * 結果が変わらないよう、明示的に未初期化へ戻す。 */
    FakeDetHw_LogSuppressed = 1U;
    CanSM_DeInit();
    FakeDetHw_LogSuppressed = 0U;
    ComM_ModeType mode = COMM_FULL_COMMUNICATION;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = ComM_GetCurrentComMode(COMM_USER_0, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(mode, COMM_FULL_COMMUNICATION);   // 失敗したときは、出力を変えない
}

}  // namespace
