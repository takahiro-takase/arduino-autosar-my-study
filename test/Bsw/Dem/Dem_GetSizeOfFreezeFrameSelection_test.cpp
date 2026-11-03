/**
 * \file    Dem_GetSizeOfFreezeFrameSelection_test.cpp
 * \brief   Dem_GetSizeOfFreezeFrameSelection() の単体テスト
 * \details 2026-11、Dem の FreezeFrame 取得 API（Dem_SelectFreezeFrameData() 系）の導入に合わせて追加した。
 *          旧 Dem_GetFreezeFrameOfEvent() / Dem_SetFreezeFrameContext()（独自関数）は、この 3 関数へ置き換えて廃止した。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Dem.h"
#include "Fake_Det_Hw.h"
#include "Wrap_Dem.h"
#include "Fake_Rte.h"
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
class Dem_GetSizeOfFreezeFrameSelection_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        WrapDem_Reset();  // 他ファイルの故障注入が漏れ伝わらないよう防御的にリセット
        FakeRte_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        Dem_Init(NULL);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Dem_Test_ResetInitState();
        FakeRte_Reset();
    }

    // 構成にある DTC（イベント 0 の DTC）。
    static uint32 ValidDtc()
    {
        uint32 dtc = 0U;
        (void)Dem_GetDTCOfEvent(0U, DEM_DTC_FORMAT_UDS, &dtc);
        return dtc;
    }

    // イベント 0 を確定 FAILED にする（FreezeFrame が記録される）。
    static void ConfirmEvent0()
    {
        for (uint8 i = 0U; i < 20U; i++)
        {
            (void)Dem_SetEventStatus(0U, DEM_EVENT_STATUS_FAILED);
        }
    }

    // イベント 0 の DTC を選び、FreezeFrame のレコードを選ぶ。
    static void SelectRecord(uint8 recordNumber)
    {
        ASSERT_EQ(Dem_SelectDTC(0U, ValidDtc(), DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY), E_OK);
        ASSERT_EQ(Dem_SelectFreezeFrameData(0U, recordNumber), E_OK);
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Dem_GetSizeOfFreezeFrameSelection_Test, Dem_GetSizeOfFreezeFrameSelection_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dem_Test_ResetInitState();
    uint16 size = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetSizeOfFreezeFrameSelection(0U, &size);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_UNINIT);
}

TEST_F(Dem_GetSizeOfFreezeFrameSelection_Test, Dem_GetSizeOfFreezeFrameSelection_NG_InvalidClientId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint16 size = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetSizeOfFreezeFrameSelection(DEM_CLIENT_COUNT, &size);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_WRONG_CONFIGURATION);
}

TEST_F(Dem_GetSizeOfFreezeFrameSelection_Test, Dem_GetSizeOfFreezeFrameSelection_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    SelectRecord(DEM_FREEZE_FRAME_RECORD_NUMBER);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetSizeOfFreezeFrameSelection(0U, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_PARAM_POINTER);
}

TEST_F(Dem_GetSizeOfFreezeFrameSelection_Test, Dem_GetSizeOfFreezeFrameSelection_NG_NoDtcSelection)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint16 size = 0U;   // Dem_SelectDTC() を呼ばない

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetSizeOfFreezeFrameSelection(0U, &size);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_WRONG_CONDITION);
}

TEST_F(Dem_GetSizeOfFreezeFrameSelection_Test, Dem_GetSizeOfFreezeFrameSelection_NG_NoRecordSelection)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SelectDTC(0U, ValidDtc(), DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY), E_OK);
    uint16 size = 0U;   // Dem_SelectFreezeFrameData() を呼ばない

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetSizeOfFreezeFrameSelection(0U, &size);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_WRONG_CONDITION);   // [SWS_Dem_01271]
}

TEST_F(Dem_GetSizeOfFreezeFrameSelection_Test, Dem_GetSizeOfFreezeFrameSelection_NG_WrongDtc)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SelectDTC(0U, 0x123456UL, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY), E_OK);
    ASSERT_EQ(Dem_SelectFreezeFrameData(0U, DEM_FREEZE_FRAME_RECORD_NUMBER), DEM_WRONG_DTC);   // 選べない
    uint16 size = 0xAAU;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetSizeOfFreezeFrameSelection(0U, &size);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);   // 選択が失敗したので、レコードの選択は無い
    EXPECT_EQ(size, 0xAAU);
}

TEST_F(Dem_GetSizeOfFreezeFrameSelection_Test, Dem_GetSizeOfFreezeFrameSelection_NG_GroupOfAllDtcsIsNotASingleDtc)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SelectDTC(0U, DEM_GROUP_ALL_DTCS, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY), E_OK);
    ASSERT_EQ(Dem_SelectFreezeFrameData(0U, DEM_FREEZE_FRAME_RECORD_NUMBER), E_OK);
    uint16 size = 0xAAU;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetSizeOfFreezeFrameSelection(0U, &size);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, DEM_WRONG_DTC);
    EXPECT_EQ(size, 0xAAU);
}

TEST_F(Dem_GetSizeOfFreezeFrameSelection_Test, Dem_GetSizeOfFreezeFrameSelection_NG_UnsupportedRecordNumber)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ConfirmEvent0();
    SelectRecord(0x02U);
    uint16 size = 0xAAU;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetSizeOfFreezeFrameSelection(0U, &size);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, DEM_NO_SUCH_ELEMENT);
    EXPECT_EQ(size, 0xAAU);
}

TEST_F(Dem_GetSizeOfFreezeFrameSelection_Test, Dem_GetSizeOfFreezeFrameSelection_NG_ObdRecordsAreNotConfigured)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ConfirmEvent0();
    SelectRecord(0xFEU);
    uint16 size = 0xAAU;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetSizeOfFreezeFrameSelection(0U, &size);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, DEM_NO_SUCH_ELEMENT);   // 0xFE（OBD のレコード全て）は、本プロジェクトに無い
}

TEST_F(Dem_GetSizeOfFreezeFrameSelection_Test, Dem_GetSizeOfFreezeFrameSelection_OK_ReturnsZeroWhenNotRecorded)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    SelectRecord(DEM_FREEZE_FRAME_RECORD_NUMBER);   // 一度も確定 FAILED していない
    uint16 size = 0xAAU;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetSizeOfFreezeFrameSelection(0U, &size);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(size, 0U);   // [SWS_Dem_00630]
}

TEST_F(Dem_GetSizeOfFreezeFrameSelection_Test, Dem_GetSizeOfFreezeFrameSelection_OK_ReturnsTheSizeOfTheRecord)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ConfirmEvent0();
    SelectRecord(DEM_FREEZE_FRAME_RECORD_NUMBER);
    uint16 size = 0xAAU;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetSizeOfFreezeFrameSelection(0U, &size);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(size, DEM_FREEZE_FRAME_RECORD_SIZE);   // {レコード番号, FreezeFrame}
}

TEST_F(Dem_GetSizeOfFreezeFrameSelection_Test, Dem_GetSizeOfFreezeFrameSelection_OK_AllRecordsReturnTheSizeOfTheOnlyRecord)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ConfirmEvent0();
    SelectRecord(DEM_RECORD_NUMBER_ALL);
    uint16 size = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetSizeOfFreezeFrameSelection(0U, &size);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(size, DEM_FREEZE_FRAME_RECORD_SIZE);   // 0xFF は全レコード（= この 1 件）
}

}  // namespace
