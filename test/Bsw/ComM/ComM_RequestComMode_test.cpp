/**
 * \file    ComM_RequestComMode_test.cpp
 * \brief   ComM_RequestComMode() の単体テスト
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
#include "CanSM.h"
#include "CanSM_Cfg.h"
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
class ComM_RequestComMode_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        ComM_Init(NULL);
        FakeDetHw_Reset();              // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        ComM_DeInit();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(ComM_RequestComMode_Test, ComM_RequestComMode_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ComM_DeInit();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = ComM_RequestComMode(0U, COMM_FULL_COMMUNICATION);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_UNINIT);
}

TEST_F(ComM_RequestComMode_Test, ComM_RequestComMode_NG_WrongParameters)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = ComM_RequestComMode(COMM_USER_COUNT, COMM_FULL_COMMUNICATION);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_WRONG_PARAMETERS);
}

TEST_F(ComM_RequestComMode_Test, ComM_RequestComMode_NG_WrongComMode)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = ComM_RequestComMode(COMM_USER_0, static_cast<ComM_ModeType>(COMM_FULL_COMMUNICATION + 1U));   // User は有効、ComMode だけが範囲外

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_WRONG_PARAMETERS);
}

TEST_F(ComM_RequestComMode_Test, ComM_RequestComMode_OK_SilentRequestWhileFullIsForwardedToCanSm)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanSM_DeInit();   // CanSM は未初期化にしておく（他のテストの状態を巻き込まない。転送先で拒否されるだけにする）
    ComM_BusSM_ModeIndication(0U, COMM_FULL_COMMUNICATION);   // チャネルを FULL_COM にする
    FakeDetHw_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = ComM_RequestComMode(COMM_USER_0, COMM_SILENT_COMMUNICATION);   // FULL_COM のチャネルへ、SILENT_COM を要求する

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);                               // CanSM（未初期化）へそのまま転送され、拒否される
    EXPECT_EQ(FakeDetHw_LastModuleId, CANSM_MODULE_ID);     // NO_COM への解放要求（Nm_NetworkRelease）ではなく、CanSM へ転送する
}

TEST_F(ComM_RequestComMode_Test, ComM_RequestComMode_OK_FullRequestWhileSilentIsForwardedToCanSm)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanSM_DeInit();
    ComM_BusSM_ModeIndication(0U, COMM_SILENT_COMMUNICATION);   // チャネルを SILENT_COM にする（例: Bus-Off）
    FakeDetHw_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = ComM_RequestComMode(COMM_USER_0, COMM_FULL_COMMUNICATION);   // SILENT_COM のチャネルへ、FULL_COM を要求する

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);                               // NO_COM からの遷移ではないので、許可待ち（CommunicationAllowed）を経ずに CanSM へ転送される
    EXPECT_EQ(FakeDetHw_LastModuleId, CANSM_MODULE_ID);
}

}  // namespace
