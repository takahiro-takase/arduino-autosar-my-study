/**
 * \file    FiM_GetFunctionPermission_test.cpp
 * \brief   FiM_GetFunctionPermission() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 FiM_test.cpp）。
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
class FiM_GetFunctionPermission_Test : public ::testing::Test
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

TEST_F(FiM_GetFunctionPermission_Test, FiM_GetFunctionPermission_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // FiM_Init() を意図的に呼ばない（SetUp() は Dem_Init() のみ実行）。
    boolean permission = TRUE;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = FiM_GetFunctionPermission(FIM_FID_RUNNING_LED, &permission);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(permission, FALSE);  // フェールセーフ: 未初期化中は抑止扱い
    EXPECT_EQ(FakeDetHw_LastErrorId, FIM_E_UNINIT);
}

TEST_F(FiM_GetFunctionPermission_Test, FiM_GetFunctionPermission_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FiM_Init(&FiM_Config);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = FiM_GetFunctionPermission(FIM_FID_RUNNING_LED, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, FIM_E_PARAM_POINTER);
}

TEST_F(FiM_GetFunctionPermission_Test, FiM_GetFunctionPermission_NG_FidOutOfRange)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FiM_Init(&FiM_Config);
    boolean permission = TRUE;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = FiM_GetFunctionPermission(FiM_Config.FunctionCount, &permission);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(permission, FALSE);  // フェールセーフ: 不明な FID は抑止扱い
    EXPECT_EQ(FakeDetHw_LastErrorId, FIM_E_FID_OUT_OF_RANGE);
}

}  // namespace
