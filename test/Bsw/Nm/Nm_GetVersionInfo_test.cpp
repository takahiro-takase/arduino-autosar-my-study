/**
 * \file    Nm_GetVersionInfo_test.cpp
 * \brief   Nm_GetVersionInfo() の単体テスト
 * \details バージョン情報を、Nm_Cfg.h の定数どおりに書き込む。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Nm.h"
#include "Nm_Cfg.h"
#include "CanNm.h"
#include "CanNm_Cfg.h"
#include "Fake_Det_Hw.h"
}

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

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

/* Nm_GetVersionInfo() の単体テスト用フィクスチャ。
 * SetUp(): DET の記録を初期化し、CanNm_Init(NULL)・Nm_Init(NULL) を呼ぶ（Init 自体の記録は消す）。
 * TearDown(): Nm・CanNm を未初期化へ戻す。 */
class Nm_GetVersionInfo_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;
        FakeDetHw_Reset();
        CanNm_Init(NULL);
        Nm_Init(NULL);
        FakeDetHw_Reset();
        FakeDetHw_LogSuppressed = 0U;
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Nm_Test_ResetInitState();
        CanNm_DeInit();
    }
};

/* Nm の単体テスト用フィクスチャ。
 * SetUp(): Nm_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。範囲外のチャネルを表す kInvalidChannel も用意する。
 * TearDown(): 未初期化の状態へ戻す。 */
class Nm_GetVersionInfo_Base_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        Nm_Init(NULL);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Nm_Test_ResetInitState();
    }

    static const NetworkHandleType kInvalidChannel = NM_MAIN_NETWORK_HANDLE + 1U;
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Nm_GetVersionInfo_Base_Test, Nm_GetVersionInfo_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Nm_GetVersionInfo(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, NM_E_PARAM_POINTER);
}

TEST_F(Nm_GetVersionInfo_Test, Nm_GetVersionInfo_OK_ReturnsConfiguredVersion)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Std_VersionInfoType info = { 0xFFFFU, 0xFFFFU, 0xFFU, 0xFFU, 0xFFU };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Nm_GetVersionInfo(&info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(info.vendorID, NM_VENDOR_ID);
    EXPECT_EQ(info.moduleID, NM_MODULE_ID);
    EXPECT_EQ(info.sw_major_version, NM_SW_MAJOR_VERSION);
    EXPECT_EQ(info.sw_minor_version, NM_SW_MINOR_VERSION);
    EXPECT_EQ(info.sw_patch_version, NM_SW_PATCH_VERSION);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

}  // namespace
