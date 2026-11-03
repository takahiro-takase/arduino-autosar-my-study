/**
 * \file    Dem_SelectExtendedDataRecord_test.cpp
 * \brief   Dem_SelectExtendedDataRecord() の単体テスト
 * \details 2026-11、Dem の ExtendedData 取得 API（Dem_SelectExtendedDataRecord() 系）の導入に合わせて追加した。
 *          旧 Dem_GetOccurrenceCounterOfEvent()（独自関数）は、この 3 関数へ置き換えて廃止した。
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
class Dem_SelectExtendedDataRecord_Test : public ::testing::Test
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

    // イベント 0 を確定 FAILED にする（ExtendedData の故障確定回数が 1 になる）。
    static void ConfirmEvent0()
    {
        for (uint8 i = 0U; i < 20U; i++)
        {
            (void)Dem_SetEventStatus(0U, DEM_EVENT_STATUS_FAILED);
        }
    }

    // イベント 0 の DTC を選び、ExtendedData のレコードを選ぶ。
    static void SelectRecord(uint8 recordNumber)
    {
        ASSERT_EQ(Dem_SelectDTC(0U, ValidDtc(), DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY), E_OK);
        ASSERT_EQ(Dem_SelectExtendedDataRecord(0U, recordNumber), E_OK);
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Dem_SelectExtendedDataRecord_Test, Dem_SelectExtendedDataRecord_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dem_Test_ResetInitState();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_SelectExtendedDataRecord(0U, DEM_EXTENDED_DATA_RECORD_NUMBER);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_UNINIT);
}

TEST_F(Dem_SelectExtendedDataRecord_Test, Dem_SelectExtendedDataRecord_NG_InvalidClientId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_SelectExtendedDataRecord(DEM_CLIENT_COUNT, DEM_EXTENDED_DATA_RECORD_NUMBER);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_WRONG_CONFIGURATION);
}

TEST_F(Dem_SelectExtendedDataRecord_Test, Dem_SelectExtendedDataRecord_NG_NoDtcSelection)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // Dem_SelectDTC() を呼ばない

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_SelectExtendedDataRecord(0U, DEM_EXTENDED_DATA_RECORD_NUMBER);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_WRONG_CONDITION);   // [SWS_Dem_01274]
}

TEST_F(Dem_SelectExtendedDataRecord_Test, Dem_SelectExtendedDataRecord_NG_WrongDtc)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SelectDTC(0U, 0x123456UL, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_SelectExtendedDataRecord(0U, DEM_EXTENDED_DATA_RECORD_NUMBER);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, DEM_WRONG_DTC);
}

TEST_F(Dem_SelectExtendedDataRecord_Test, Dem_SelectExtendedDataRecord_NG_WrongDtcOrigin)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SelectDTC(0U, ValidDtc(), DEM_DTC_FORMAT_UDS, static_cast<Dem_DTCOriginType>(0xFFFFU)), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_SelectExtendedDataRecord(0U, DEM_EXTENDED_DATA_RECORD_NUMBER);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, DEM_WRONG_DTCORIGIN);
}

TEST_F(Dem_SelectExtendedDataRecord_Test, Dem_SelectExtendedDataRecord_NG_SelectDtcAgainDiscardsTheRecordSelection)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SelectDTC(0U, ValidDtc(), DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY), E_OK);
    ASSERT_EQ(Dem_SelectExtendedDataRecord(0U, DEM_EXTENDED_DATA_RECORD_NUMBER), E_OK);
    ASSERT_EQ(Dem_SelectDTC(0U, ValidDtc(), DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY), E_OK);   // 選び直す

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    uint16 size = 0U;
    const Std_ReturnType ret = Dem_GetSizeOfExtendedDataRecordSelection(0U, &size);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);   // レコードの選択は破棄された
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_WRONG_CONDITION);
}

TEST_F(Dem_SelectExtendedDataRecord_Test, Dem_SelectExtendedDataRecord_OK_SelectsTheRecord)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SelectDTC(0U, ValidDtc(), DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_SelectExtendedDataRecord(0U, DEM_EXTENDED_DATA_RECORD_NUMBER);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
}

TEST_F(Dem_SelectExtendedDataRecord_Test, Dem_SelectExtendedDataRecord_OK_AcceptsAnUnsupportedRecordNumber)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SelectDTC(0U, ValidDtc(), DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_SelectExtendedDataRecord(0U, 0x02U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);   // 構成に無いレコード番号は、本関数ではなく、Get 系が DEM_NO_SUCH_ELEMENT で返す
}

}  // namespace
