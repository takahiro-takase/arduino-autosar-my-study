/**
 * \file    Bsw_ComM_test.cpp
 * \brief   ComM.c（src/Bsw/ComM/ComM.c）の単体テスト（GoogleTest / PlatformIO
 *          `[env:native_chain]`）。
 *
 * \details 2026-09、モジュール単位のテストファイルを1モジュール1ファイルへ
 *          集約する方針のもと、`Bsw_ComM_CommunicationAllowed_test.cpp` /
 *          `Bsw_ComM_GetRequestedComMode_test.cpp` / 旧 `Bsw_ComM_test.cpp`
 *          （NG系のみ）の3ファイルを本ファイルへ統合した。各セクションの経緯は
 *          元ファイルのコメントをそのまま引き継ぐ。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "ComM.h"
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

// ============================================================================
// ComM_CommunicationAllowed()（旧 Bsw_ComM_CommunicationAllowed_test.cpp）
// ============================================================================

/**
 * \details AUTOSAR SWS_ComM_00871 準拠のシグネチャで新設。ここでは
 *          Allowed=FALSE のまま保留される経路（CanSM を一切呼ばない）と、
 *          異常系・初期値のみ検証する。「Allowed=TRUE で保留中の要求が
 *          実際に CanSM まで届く」正常系は、他の ComM API と同じ static
 *          共有状態のハザードを避けるため、CanSM/CanIf/Can が安全に
 *          初期化済みの Bsw_NmStack_SleepCoordination_test.cpp 側で検証する
 *          （下記 ComM_GetRequestedComMode セクション末尾のコメントと同じ理由）。
 */

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* ComM_CommunicationAllowed() の単体テスト用フィクスチャ。
 * SetUp(): ComM_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): ComM_DeInit() で未初期化へ戻す。 */
class Bsw_ComM_CommunicationAllowed_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        ComM_Init(NULL);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
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

TEST_F(Bsw_ComM_CommunicationAllowed_Test, ComM_CommunicationAllowed_NG_UninitializedReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ComM_DeInit();
    FakeDetHw_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ComM_CommunicationAllowed(COMM_CHANNEL_0, TRUE);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_UNINIT);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);
}

TEST_F(Bsw_ComM_CommunicationAllowed_Test, ComM_CommunicationAllowed_NG_InvalidChannelReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ComM_CommunicationAllowed(COMM_CHANNEL_COUNT, TRUE);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_WRONG_PARAMETERS);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);
}

// ============================================================================
// ComM_GetRequestedComMode()（旧 Bsw_ComM_GetRequestedComMode_test.cpp）
// ============================================================================

/**
 * \details AUTOSAR SWS_ComM_00079 準拠のシグネチャで新設。集約前のユーザ単位
 *          の要求値（`ComM_UserRequest[User]`）をそのまま返す薄いgetterで
 *          あることを検証する。異常系と初期値のみ検証し、
 *          「ComM_RequestComMode() で設定した値を反映する」正常系は
 *          ComM_RequestComMode() 自体のカスケード（CanSM/CanIf/Can）が絡む
 *          static 共有状態のハザードを避けるためあえて省略する（詳細は末尾の
 *          コメント参照）。
 */

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* ComM_GetRequestedComMode() の単体テスト用フィクスチャ。
 * SetUp(): ComM_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。範囲外のユーザーを表す kInvalidUser も用意する。
 * TearDown(): ComM_DeInit() で未初期化へ戻す。 */
class Bsw_ComM_GetRequestedComMode_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        ComM_Init(NULL);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        ComM_DeInit();
    }

    static const ComM_UserHandleType kInvalidUser = COMM_USER_COUNT;
};

TEST_F(Bsw_ComM_GetRequestedComMode_Test, ComM_GetRequestedComMode_OK_ReturnsNoComRightAfterInit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ComM_ModeType mode = COMM_FULL_COMMUNICATION;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = ComM_GetRequestedComMode(COMM_USER_0, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(mode, COMM_NO_COMMUNICATION);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(Bsw_ComM_GetRequestedComMode_Test, ComM_GetRequestedComMode_NG_UninitializedReturnsErrorAndReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ComM_DeInit();
    FakeDetHw_Reset();

    ComM_ModeType mode = COMM_FULL_COMMUNICATION;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = ComM_GetRequestedComMode(COMM_USER_0, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_UNINIT);
}

TEST_F(Bsw_ComM_GetRequestedComMode_Test, ComM_GetRequestedComMode_NG_InvalidUserReturnsErrorAndReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ComM_ModeType mode = COMM_FULL_COMMUNICATION;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = ComM_GetRequestedComMode(kInvalidUser, &mode);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_WRONG_PARAMETERS);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);
}

TEST_F(Bsw_ComM_GetRequestedComMode_Test, ComM_GetRequestedComMode_NG_NullPointerReturnsErrorAndReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = ComM_GetRequestedComMode(COMM_USER_0, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_PARAM_POINTER);
}

/* 「ComM_RequestComMode() で設定した値をそのまま返す」という正常系は、
 * ここでは意図的に検証しない: ComM_RequestComMode(FULL_COM) は
 * ComM_ApplyAggregatedRequest() 経由で実体の CanSM_RequestComMode() →
 * CanIf_SetControllerMode() までカスケードする。CanSM/CanIf/Can は
 * native_chain バイナリ内で他のテストファイル（Bsw_NmStack_SleepCoordination_
 * test.cpp 等）と static な初期化状態を共有しており、実行順序次第で
 * CanSM が既に初期化済み（かつ実 HW 前提の内部状態）のままこの呼び出しに
 * 到達し、ハングする実害を確認した（実測: 177秒でタイムアウト）。
 * PduR_ConfigPtr 等と同じ「バイナリ全体で共有される static、かつ
 * DeInit で完全には戻らない」既知の危険パターン（Bsw_PduR_test.cpp の
 * PduR_SecOCTxConfirmation セクション冒頭コメント参照）のため、この正常系は
 * 意図的に省略する。 */

// ============================================================================
// その他公開APIの NG系（旧 Bsw_ComM_test.cpp）
// ============================================================================

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

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* ComM（上記 2 グループ以外の API）の単体テスト用フィクスチャ。
 * SetUp(): ComM_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): ComM_DeInit() で未初期化へ戻す。 */
class Bsw_ComM_Test : public ::testing::Test
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

// ------------------------------------------------------------
// ComM_DeInit()
// ------------------------------------------------------------

TEST_F(Bsw_ComM_Test, ComM_DeInit_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ComM_DeInit();  // 1回目: SetUp() の Init を正常に解除する
    FakeDetHw_Reset();

    ComM_DeInit();  // 2回目: 既に未初期化のため NG

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_UNINIT);
}

// ------------------------------------------------------------
// ComM_GetStatus()（未初期化チェックは無い例外API、ComM.c 参照）
// ------------------------------------------------------------

TEST_F(Bsw_ComM_Test, ComM_GetStatus_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = ComM_GetStatus(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// ComM_RequestComMode()
// ------------------------------------------------------------

TEST_F(Bsw_ComM_Test, ComM_RequestComMode_NG_Uninit)
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

TEST_F(Bsw_ComM_Test, ComM_RequestComMode_NG_WrongParameters)
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

// ------------------------------------------------------------
// ComM_GetCurrentComMode()
// ------------------------------------------------------------

TEST_F(Bsw_ComM_Test, ComM_GetCurrentComMode_NG_Uninit)
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

TEST_F(Bsw_ComM_Test, ComM_GetCurrentComMode_NG_WrongParameters)
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

TEST_F(Bsw_ComM_Test, ComM_GetCurrentComMode_NG_NullPointer)
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

// ------------------------------------------------------------
// ComM_GetVersionInfo()
// ------------------------------------------------------------

TEST_F(Bsw_ComM_Test, ComM_GetVersionInfo_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ComM_GetVersionInfo(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// ComM_Nm_NetworkStartIndication()
// ------------------------------------------------------------

TEST_F(Bsw_ComM_Test, ComM_Nm_NetworkStartIndication_NG_Uninit)
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

TEST_F(Bsw_ComM_Test, ComM_Nm_NetworkStartIndication_NG_WrongParameters)
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

// ------------------------------------------------------------
// ComM_Nm_NetworkMode()
// ------------------------------------------------------------

TEST_F(Bsw_ComM_Test, ComM_Nm_NetworkMode_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ComM_DeInit();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ComM_Nm_NetworkMode(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_UNINIT);
}

TEST_F(Bsw_ComM_Test, ComM_Nm_NetworkMode_NG_WrongParameters)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ComM_Nm_NetworkMode(COMM_CHANNEL_COUNT);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_WRONG_PARAMETERS);
}

// ------------------------------------------------------------
// ComM_Nm_PrepareBusSleepMode()
// ------------------------------------------------------------

TEST_F(Bsw_ComM_Test, ComM_Nm_PrepareBusSleepMode_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ComM_DeInit();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ComM_Nm_PrepareBusSleepMode(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_UNINIT);
}

TEST_F(Bsw_ComM_Test, ComM_Nm_PrepareBusSleepMode_NG_WrongParameters)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ComM_Nm_PrepareBusSleepMode(COMM_CHANNEL_COUNT);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_WRONG_PARAMETERS);
}

// ------------------------------------------------------------
// ComM_Nm_BusSleepMode()
// ------------------------------------------------------------

TEST_F(Bsw_ComM_Test, ComM_Nm_BusSleepMode_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ComM_DeInit();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ComM_Nm_BusSleepMode(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_UNINIT);
}

TEST_F(Bsw_ComM_Test, ComM_Nm_BusSleepMode_NG_WrongParameters)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ComM_Nm_BusSleepMode(COMM_CHANNEL_COUNT);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_WRONG_PARAMETERS);
}

// ------------------------------------------------------------
// ComM_DCM_ActiveDiagnostic()
// ------------------------------------------------------------

TEST_F(Bsw_ComM_Test, ComM_DCM_ActiveDiagnostic_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ComM_DeInit();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ComM_DCM_ActiveDiagnostic(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_UNINIT);
}

TEST_F(Bsw_ComM_Test, ComM_DCM_ActiveDiagnostic_NG_WrongParameters)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ComM_DCM_ActiveDiagnostic(COMM_CHANNEL_COUNT);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_WRONG_PARAMETERS);
}

// ------------------------------------------------------------
// ComM_DCM_InactiveDiagnostic()
// ------------------------------------------------------------

TEST_F(Bsw_ComM_Test, ComM_DCM_InactiveDiagnostic_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ComM_DeInit();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ComM_DCM_InactiveDiagnostic(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_UNINIT);
}

TEST_F(Bsw_ComM_Test, ComM_DCM_InactiveDiagnostic_NG_WrongParameters)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ComM_DCM_InactiveDiagnostic(COMM_CHANNEL_COUNT);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_WRONG_PARAMETERS);
}

// ------------------------------------------------------------
// ComM_BusSM_ModeIndication()
// ------------------------------------------------------------

TEST_F(Bsw_ComM_Test, ComM_BusSM_ModeIndication_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ComM_DeInit();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ComM_BusSM_ModeIndication(0U, COMM_FULL_COMMUNICATION);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_UNINIT);
}

TEST_F(Bsw_ComM_Test, ComM_BusSM_ModeIndication_NG_WrongParameters)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ComM_BusSM_ModeIndication(COMM_CHANNEL_COUNT, COMM_FULL_COMMUNICATION);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_WRONG_PARAMETERS);
}

// ------------------------------------------------------------
// ComM_MainFunction()
// ------------------------------------------------------------

TEST_F(Bsw_ComM_Test, ComM_MainFunction_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ComM_DeInit();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ComM_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, COMM_E_UNINIT);
}

}  // namespace
