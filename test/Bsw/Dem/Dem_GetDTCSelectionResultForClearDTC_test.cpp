/**
 * \file    Dem_GetDTCSelectionResultForClearDTC_test.cpp
 * \brief   Dem_GetDTCSelectionResultForClearDTC() の単体テスト
 * \details 2026-11、Dem の SelectDTC 系（[SWS_Dem_01253]）の導入に合わせて追加した。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Dem.h"
#include "Fake_Det_Hw.h"
#include "Wrap_Dem.h"
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

/* Dem の単体テスト用フィクスチャ。
 * SetUp(): wrap の状態を戻し、Dem_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): 未初期化の状態へ戻す。 */
class Dem_GetDTCSelectionResultForClearDTC_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        WrapDem_Reset();  // 他ファイルの故障注入が漏れ伝わらないよう防御的にリセット
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        Dem_Init(NULL);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Dem_Test_ResetInitState();
    }

    // 構成にある DTC（イベント 0 の DTC）。
    static uint32 ValidDtc()
    {
        uint32 dtc = 0U;
        (void)Dem_GetDTCOfEvent(0U, DEM_DTC_FORMAT_UDS, &dtc);
        return dtc;
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Dem_GetDTCSelectionResultForClearDTC_Test, Dem_GetDTCSelectionResultForClearDTC_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dem_Test_ResetInitState();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetDTCSelectionResultForClearDTC(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_UNINIT);
}

TEST_F(Dem_GetDTCSelectionResultForClearDTC_Test, Dem_GetDTCSelectionResultForClearDTC_NG_InvalidClientId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetDTCSelectionResultForClearDTC(DEM_CLIENT_COUNT);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_WRONG_CONFIGURATION);
}

TEST_F(Dem_GetDTCSelectionResultForClearDTC_Test, Dem_GetDTCSelectionResultForClearDTC_NG_NoSelectionIsReportedAsWrongDtc)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // Dem_SelectDTC() を呼ばない

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetDTCSelectionResultForClearDTC(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, DEM_WRONG_DTC);   // [SWS_Dem_01261]: 選択が無い＝無効な選択（本関数の戻り値表に E_NOT_OK は無い）
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_WRONG_CONDITION);
}

TEST_F(Dem_GetDTCSelectionResultForClearDTC_Test, Dem_GetDTCSelectionResultForClearDTC_NG_WrongDtc)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SelectDTC(0U, 0x123456UL, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetDTCSelectionResultForClearDTC(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, DEM_WRONG_DTC);
}

TEST_F(Dem_GetDTCSelectionResultForClearDTC_Test, Dem_GetDTCSelectionResultForClearDTC_NG_WrongDtcOrigin)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SelectDTC(0U, DEM_GROUP_ALL_DTCS, DEM_DTC_FORMAT_UDS, static_cast<Dem_DTCOriginType>(0xFFFFU)), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetDTCSelectionResultForClearDTC(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, DEM_WRONG_DTCORIGIN);
}

TEST_F(Dem_GetDTCSelectionResultForClearDTC_Test, Dem_GetDTCSelectionResultForClearDTC_OK_SingleDtcIsAcceptedForClear)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SelectDTC(0U, ValidDtc(), DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetDTCSelectionResultForClearDTC(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);   // 単一の DTC の消去だけを禁じる設定は持たない
}

TEST_F(Dem_GetDTCSelectionResultForClearDTC_Test, Dem_GetDTCSelectionResultForClearDTC_OK_GroupOfAllDtcsIsAcceptedForClear)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SelectDTC(0U, DEM_GROUP_ALL_DTCS, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetDTCSelectionResultForClearDTC(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
}

}  // namespace
