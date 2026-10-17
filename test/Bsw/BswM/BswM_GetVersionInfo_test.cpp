/**
 * \file    BswM_GetVersionInfo_test.cpp
 * \brief   BswM_GetVersionInfo() の単体テスト
 * \details バージョン情報を、BswM_Cfg.h の定数どおりに書き込む。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "BswM.h"
#include "BswM_Cfg.h"
#include "BswM_PBCfg.h"
#include "EcuM.h"
#include "ComM.h"
#include "Dcm_Types.h"
#include "Com_Cfg.h"
#include "Nm_Cfg.h"
#include "Wrap_Os.h"
#include "Wrap_Com.h"
#include "Wrap_Nm.h"
#include "Wrap_BswM.h"
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

/* BswM_GetVersionInfo() の単体テスト用フィクスチャ。
 * SetUp(): Os・Com・Nm・BswM の wrap と DET の記録を初期化し、BswM_Init(&BswM_Config)（本番のルール表）を呼ぶ
 * （Init 中のログは抑制し、Init 自体の記録は消す）。呼び出し回数は、各ルールの Action が呼ぶ関数の wrap で数える。
 * TearDown(): BswM_Deinit() で未初期化へ戻す。 */
class BswM_GetVersionInfo_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;
        WrapOs_Reset();
        WrapCom_Reset();
        WrapNm_Reset();
        WrapBswM_Reset();
        FakeDetHw_Reset();
        BswM_Init(&BswM_Config);
        ResetCounts();
        FakeDetHw_Reset();
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        BswM_Deinit();
    }

    /* Init 中の記録を消す。 */
    static void ResetCounts()
    {
        WrapOs_Reset();
        WrapCom_Reset();
        WrapNm_Reset();
    }

    /* マスクの 1 のビットの数（その分だけ Os_SetTaskActive() が呼ばれる）。 */
    static uint32 Bits(uint32 mask)
    {
        uint32 n = 0U;
        for (; mask != 0U; mask >>= 1U)
        {
            n += (mask & 1U);
        }
        return n;
    }
};

/* BswM の単体テスト用フィクスチャ。
 * SetUp(): wrap の状態を戻し、BswM_Init(&BswM_Config) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): BswM_Deinit() で未初期化へ戻す。 */
class BswM_GetVersionInfo_Base_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        WrapBswM_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        BswM_Init(&BswM_Config);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        BswM_Deinit();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(BswM_GetVersionInfo_Base_Test, BswM_GetVersionInfo_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BswM_GetVersionInfo(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, BSWM_E_PARAM_POINTER);
}

TEST_F(BswM_GetVersionInfo_Test, BswM_GetVersionInfo_OK_ReturnsConfiguredVersion)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Std_VersionInfoType info = { 0xFFFFU, 0xFFFFU, 0xFFU, 0xFFU, 0xFFU };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BswM_GetVersionInfo(&info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(info.vendorID, BSWM_VENDOR_ID);
    EXPECT_EQ(info.moduleID, BSWM_MODULE_ID);
    EXPECT_EQ(info.sw_major_version, BSWM_SW_MAJOR_VERSION);
    EXPECT_EQ(info.sw_minor_version, BSWM_SW_MINOR_VERSION);
    EXPECT_EQ(info.sw_patch_version, BSWM_SW_PATCH_VERSION);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

}  // namespace
