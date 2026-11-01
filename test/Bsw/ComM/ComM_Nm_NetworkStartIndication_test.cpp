/**
 * \file    ComM_Nm_NetworkStartIndication_test.cpp
 * \brief   ComM_Nm_NetworkStartIndication() の単体テスト
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
#include "Fake_Det_Hw.h"
#include "Wrap_CanSM.h"
#include "Wrap_Nm.h"
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
class ComM_Nm_NetworkStartIndication_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        ComM_Init(NULL);
        WrapCanSM_Reset();
        WrapNm_Reset();
        FailFromCallCount_Nm_NetworkRequest = 1U;   // Nm への要求は、実体を呼ばずに成功する（CanNm へカスケードしない）
        ForcedReturn_Nm_NetworkRequest      = E_OK;
        FailFromCallCount_Nm_NetworkRelease = 1U;
        ForcedReturn_Nm_NetworkRelease      = E_OK;
        FakeDetHw_Reset();              // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        ComM_DeInit();
        WrapCanSM_Reset();
        WrapNm_Reset();
    }

    // 協調スリープ待ちの最中に、チャネルが SILENT_COM になった状態（ComM_NmReleasePending が立っている）を用意する。
    // 返り値: この時点までの Nm_NetworkRequest() の呼び出し回数。
    uint32 ArrangeSilentComWithReleasePending()
    {
        ComM_BusSM_ModeIndication(0U, COMM_FULL_COMMUNICATION);                  // チャネルが FULL_COM になる
        EXPECT_EQ(ComM_RequestComMode(COMM_USER_0, COMM_NO_COMMUNICATION), E_OK);   // 協調スリープを始める（CanNm へ解放を伝える）
        ComM_BusSM_ModeIndication(0U, COMM_SILENT_COMMUNICATION);                // Bus-Off などで、SILENT_COM になる
        return CallCount_Nm_NetworkRequest;
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(ComM_Nm_NetworkStartIndication_Test, ComM_Nm_NetworkStartIndication_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ComM_DeInit();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ComM_Nm_NetworkStartIndication(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_UNINIT);
}

TEST_F(ComM_Nm_NetworkStartIndication_Test, ComM_Nm_NetworkStartIndication_NG_WrongParameters)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ComM_Nm_NetworkStartIndication(COMM_CHANNEL_COUNT);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_WRONG_PARAMETERS);
}

TEST_F(ComM_Nm_NetworkStartIndication_Test, ComM_Nm_NetworkStartIndication_OK_RejectedByCanSmKeepsNmReleasePending)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    const uint32 requestsBefore = ArrangeSilentComWithReleasePending();
    FailFromCallCount_CanSM_RequestComMode = 1U;   // CanSM が、FULL_COM の要求を、受け付けない
    ForcedReturn_CanSM_RequestComMode      = E_NOT_OK;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ComM_Nm_NetworkStartIndication(0U);
    ComM_BusSM_ModeIndication(0U, COMM_FULL_COMMUNICATION);   // その後、CanSM が FULL_COM を通知してくる（回復など）

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_CanSM_RequestComMode, 1U);
    EXPECT_EQ(CallCount_Nm_NetworkRequest, requestsBefore);   // 要求が受け付けられなかったので、協調スリープ待ちは残り、CanNm を起こさない
}

TEST_F(ComM_Nm_NetworkStartIndication_Test, ComM_Nm_NetworkStartIndication_OK_AcceptedByCanSmClearsNmReleasePending)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    const uint32 requestsBefore = ArrangeSilentComWithReleasePending();
    FailFromCallCount_CanSM_RequestComMode = 1U;   // CanSM が、FULL_COM の要求を、受け付ける
    ForcedReturn_CanSM_RequestComMode      = E_OK;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ComM_Nm_NetworkStartIndication(0U);
    ComM_BusSM_ModeIndication(0U, COMM_FULL_COMMUNICATION);   // 受け付けられたので、CanSM が FULL_COM を通知する

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_CanSM_RequestComMode, 1U);
    EXPECT_EQ(CallCount_Nm_NetworkRequest, requestsBefore + 1U);   // 協調スリープ待ちが解除されたので、CanNm のネットワークを要求する
}

}  // namespace
