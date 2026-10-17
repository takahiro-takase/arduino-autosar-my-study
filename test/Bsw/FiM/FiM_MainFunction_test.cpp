/**
 * \file    FiM_MainFunction_test.cpp
 * \brief   FiM_MainFunction() の単体テスト
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
class FiM_MainFunction_Test : public ::testing::Test
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

TEST_F(FiM_MainFunction_Test, MainFunction_OK_InhibitsFidWhenDtcConfirmedAfterInit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FiM_Init(&FiM_Config);
    ASSERT_EQ(GetPermission(FIM_FID_RUNNING_LED), TRUE);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    // 初期化後に Bus-Off が確定 (DEM_DEBOUNCE_LIMIT_CAN_BUSOFF=1
    // のため1回で確定)
    (void)Dem_SetEventStatus(DemConf_DemEventParameter_CAN_BUSOFF, DEM_EVENT_STATUS_FAILED);
    FiM_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    // RUNNING_LED のみ抑止され、無関係の BUTTON_ACK は影響なし
    EXPECT_EQ(GetPermission(FIM_FID_RUNNING_LED), FALSE);
    EXPECT_EQ(GetPermission(FIM_FID_BUTTON_ACK), TRUE);
}

}  // namespace
