/**
 * \file    FiM_Init_test.cpp
 * \brief   FiM_Init() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 FiM_test.cpp）。
 *
 *          FiM.c は本プロジェクトでこれまでどの native テスト環境にもリンク
 *          されておらず、テストカバレッジが皆無だった。新設時、
 *          `FiM_Init()`が全FIDを無条件で「許可」初期化していた乖離
 *          （[SWS_Fim_00102]/[SWS_Fim_00104]。以前は起動直後から最初の
 *          `FiM_MainFunction()`呼び出しまでの間、既に確定済みのDTCがあって
 *          も誤って許可扱いになっていた）を是正したのに合わせ、専用環境を
 *          新設した。
 *
 *          分割前の共通の説明は、旧 FiM_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "FiM.h"
#include "Dem.h"
#include "Dem_Cfg.h"
#include "Fake_Millis.h"
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

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* FiM の単体テスト用フィクスチャ。
 * SetUp(): 時刻を初期化し、Dem_Init(NULL) を呼ぶ。FiM_Init() は各テストが呼ぶ。GetPermission() は、機能の許可状態を取得して戻り値も確認する補助関数。
 * TearDown(): FiM を未初期化の状態へ戻す。 */
class FiM_Init_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Dem_Init() のログはノイズになるため抑制

        Dem_Init(NULL);  // Fake_NvM.c により常に「初回起動」で確定的に再現

        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        FiM_Test_ResetInitState();
    }

    static boolean GetPermission(FiM_FunctionIdType fid)
    {
        boolean permission = FALSE;
        const Std_ReturnType ret = FiM_GetFunctionPermission(fid, &permission);
        EXPECT_EQ(ret, E_OK);
        return permission;
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(FiM_Init_Test, Init_NG_InhibitsAlreadyConfirmedDtcWithoutWaitingForMainFunction)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // 実機の起動順序 (EcuM.c: Dem_Init() → FiM_Init()) を
    // 模し、FiM_Init() を呼ぶ「前」に Bus-Off が既に確定している状態を作る
    // (前回起動までに NvM から復元された確定済み DTC に相当)。
    (void)Dem_SetEventStatus(DemConf_DemEventParameter_CAN_BUSOFF, DEM_EVENT_STATUS_FAILED);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    // 是正前は全 FID を無条件で「許可」初期化しており、
    // FiM_MainFunction() が最低1回実行されるまでこの乖離が顕在化しな
    // かった。FiM_MainFunction() は一度も呼ばない。
    FiM_Init(&FiM_Config);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    // FiM_Init() 完了の時点で、既に抑止が反映されている
    // べき（[SWS_Fim_00104]: FiM_GetFunctionPermission は完全初期化前は
    // 使用してはならない＝完全初期化の時点で正しい値が確定している）。
    EXPECT_EQ(GetPermission(FIM_FID_RUNNING_LED), FALSE);
    EXPECT_EQ(GetPermission(FIM_FID_BUTTON_ACK), TRUE);
}

TEST_F(FiM_Init_Test, FiM_Init_NG_NullConfigPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    FiM_Init(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, FIM_E_PARAM_POINTER);
}

TEST_F(FiM_Init_Test, Init_OK_BothFidsPermittedWhenNoConfirmedDtc)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // Dem_Init() 直後は全イベント未確定(初回起動)
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    FiM_Init(&FiM_Config);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(GetPermission(FIM_FID_RUNNING_LED), TRUE);
    EXPECT_EQ(GetPermission(FIM_FID_BUTTON_ACK), TRUE);
}

// ------------------------------------------------------------
// 2026-09 是正の本題: [SWS_Fim_00102]/[SWS_Fim_00104]
// FiM_Init() 完了時点で既に許可状態が確定していること
// ------------------------------------------------------------
TEST_F(FiM_Init_Test, Init_OK_SetFunctionAvailableStillOverridesAfterInitEvaluation)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // 確定済み DTC は無い状態で初期化
    FiM_Init(&FiM_Config);
    ASSERT_EQ(GetPermission(FIM_FID_BUTTON_ACK), TRUE);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    // [SWS_Fim_00106] 外部からの強制利用不可
    const Std_ReturnType ret = FiM_SetFunctionAvailable(FIM_FID_BUTTON_ACK, FALSE);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_EQ(ret, E_OK);
    EXPECT_EQ(GetPermission(FIM_FID_BUTTON_ACK), FALSE);
}

}  // namespace
