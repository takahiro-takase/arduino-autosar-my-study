/**
 * \file    Dem_GetStatusOfDTC_test.cpp
 * \brief   Dem_GetStatusOfDTC() の単体テスト
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
class Dem_GetStatusOfDTC_Test : public ::testing::Test
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

TEST_F(Dem_GetStatusOfDTC_Test, Dem_GetStatusOfDTC_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dem_Test_ResetInitState();
    Dem_UdsStatusByteType status = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetStatusOfDTC(0U, &status);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_UNINIT);
}

TEST_F(Dem_GetStatusOfDTC_Test, Dem_GetStatusOfDTC_NG_InvalidClientId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dem_UdsStatusByteType status = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetStatusOfDTC(DEM_CLIENT_COUNT, &status);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_WRONG_CONFIGURATION);
}

TEST_F(Dem_GetStatusOfDTC_Test, Dem_GetStatusOfDTC_NG_NoSelection)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dem_UdsStatusByteType status = 0U;   // Dem_SelectDTC() を呼ばない

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetStatusOfDTC(0U, &status);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_WRONG_CONDITION);   // [SWS_Dem_01255]
}

TEST_F(Dem_GetStatusOfDTC_Test, Dem_GetStatusOfDTC_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SelectDTC(0U, ValidDtc(), DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetStatusOfDTC(0U, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_PARAM_POINTER);
}

TEST_F(Dem_GetStatusOfDTC_Test, Dem_GetStatusOfDTC_NG_WrongDtc)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SelectDTC(0U, 0x123456UL, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY), E_OK);
    Dem_UdsStatusByteType status = 0xAAU;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetStatusOfDTC(0U, &status);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, DEM_WRONG_DTC);
    EXPECT_EQ(status, 0xAAU);   // 失敗のときは、格納先を書き換えない
}

TEST_F(Dem_GetStatusOfDTC_Test, Dem_GetStatusOfDTC_NG_WrongDtcOrigin)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SelectDTC(0U, ValidDtc(), DEM_DTC_FORMAT_UDS, static_cast<Dem_DTCOriginType>(0xFFFFU)), E_OK);
    Dem_UdsStatusByteType status = 0xAAU;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetStatusOfDTC(0U, &status);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, DEM_WRONG_DTCORIGIN);
    EXPECT_EQ(status, 0xAAU);
}

TEST_F(Dem_GetStatusOfDTC_Test, Dem_GetStatusOfDTC_NG_GroupOfAllDtcsIsNotASingleDtc)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SelectDTC(0U, DEM_GROUP_ALL_DTCS, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY), E_OK);
    Dem_UdsStatusByteType status = 0xAAU;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetStatusOfDTC(0U, &status);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, DEM_WRONG_DTC);   // グループは、DTC が 1 件に決まらない
    EXPECT_EQ(status, 0xAAU);
}

TEST_F(Dem_GetStatusOfDTC_Test, Dem_GetStatusOfDTC_OK_ReturnsTheStatusOfTheSelectedDtc)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SelectDTC(0U, ValidDtc(), DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY), E_OK);
    Dem_UdsStatusByteType expected = 0U;
    ASSERT_EQ(Dem_GetEventUdsStatus(0U, &expected), E_OK);
    Dem_UdsStatusByteType status = 0xAAU;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetStatusOfDTC(0U, &status);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(status, expected);   // イベント 0 のステータス（DemStatusAvailabilityMask でマスク済み）
}

TEST_F(Dem_GetStatusOfDTC_Test, Dem_GetStatusOfDTC_OK_FollowsTheStatusOfTheEvent)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SelectDTC(0U, ValidDtc(), DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY), E_OK);
    for (uint8 i = 0U; i < 20U; i++)
    {
        (void)Dem_SetEventStatus(0U, DEM_EVENT_STATUS_FAILED);   // 確定させる
    }
    Dem_UdsStatusByteType expected = 0U;
    ASSERT_EQ(Dem_GetEventUdsStatus(0U, &expected), E_OK);
    Dem_UdsStatusByteType status = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetStatusOfDTC(0U, &status);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(status, expected);
    EXPECT_NE(status & 0x01U, 0U);   // testFailed が立っている（選択は、状態が変わっても、そのまま残る）
}

}  // namespace
