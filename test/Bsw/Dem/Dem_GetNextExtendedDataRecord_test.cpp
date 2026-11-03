/**
 * \file    Dem_GetNextExtendedDataRecord_test.cpp
 * \brief   Dem_GetNextExtendedDataRecord() の単体テスト
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
class Dem_GetNextExtendedDataRecord_Test : public ::testing::Test
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

TEST_F(Dem_GetNextExtendedDataRecord_Test, Dem_GetNextExtendedDataRecord_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dem_Test_ResetInitState();
    uint8  buf[4] = { 0xAAU, 0xAAU, 0xAAU, 0xAAU };
    uint16 bufSize = sizeof(buf);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetNextExtendedDataRecord(0U, buf, &bufSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_UNINIT);
}

TEST_F(Dem_GetNextExtendedDataRecord_Test, Dem_GetNextExtendedDataRecord_NG_InvalidClientId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8  buf[4] = { 0xAAU, 0xAAU, 0xAAU, 0xAAU };
    uint16 bufSize = sizeof(buf);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetNextExtendedDataRecord(DEM_CLIENT_COUNT, buf, &bufSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_WRONG_CONFIGURATION);
}

TEST_F(Dem_GetNextExtendedDataRecord_Test, Dem_GetNextExtendedDataRecord_NG_NullBuffer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    SelectRecord(DEM_EXTENDED_DATA_RECORD_NUMBER);
    uint16 bufSize = 4U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetNextExtendedDataRecord(0U, NULL, &bufSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_PARAM_POINTER);
}

TEST_F(Dem_GetNextExtendedDataRecord_Test, Dem_GetNextExtendedDataRecord_NG_NullBufSize)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    SelectRecord(DEM_EXTENDED_DATA_RECORD_NUMBER);
    uint8 buf[4] = { 0U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetNextExtendedDataRecord(0U, buf, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_PARAM_POINTER);
}

TEST_F(Dem_GetNextExtendedDataRecord_Test, Dem_GetNextExtendedDataRecord_NG_NoDtcSelection)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8  buf[4] = { 0xAAU, 0xAAU, 0xAAU, 0xAAU };
    uint16 bufSize = sizeof(buf);   // Dem_SelectDTC() を呼ばない

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetNextExtendedDataRecord(0U, buf, &bufSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_WRONG_CONDITION);
}

TEST_F(Dem_GetNextExtendedDataRecord_Test, Dem_GetNextExtendedDataRecord_NG_NoRecordSelection)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SelectDTC(0U, ValidDtc(), DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY), E_OK);
    uint8  buf[4] = { 0xAAU, 0xAAU, 0xAAU, 0xAAU };
    uint16 bufSize = sizeof(buf);   // Dem_SelectExtendedDataRecord() を呼ばない

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetNextExtendedDataRecord(0U, buf, &bufSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_WRONG_CONDITION);   // [SWS_Dem_01274]
}

TEST_F(Dem_GetNextExtendedDataRecord_Test, Dem_GetNextExtendedDataRecord_NG_GroupOfAllDtcsIsNotASingleDtc)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SelectDTC(0U, DEM_GROUP_ALL_DTCS, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY), E_OK);
    ASSERT_EQ(Dem_SelectExtendedDataRecord(0U, DEM_EXTENDED_DATA_RECORD_NUMBER), E_OK);
    uint8  buf[4] = { 0xAAU, 0xAAU, 0xAAU, 0xAAU };
    uint16 bufSize = sizeof(buf);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetNextExtendedDataRecord(0U, buf, &bufSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, DEM_WRONG_DTC);
}

TEST_F(Dem_GetNextExtendedDataRecord_Test, Dem_GetNextExtendedDataRecord_NG_UnsupportedRecordNumber)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ConfirmEvent0();
    SelectRecord(0x02U);
    uint8  buf[4] = { 0xAAU, 0xAAU, 0xAAU, 0xAAU };
    uint16 bufSize = sizeof(buf);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetNextExtendedDataRecord(0U, buf, &bufSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, DEM_NO_SUCH_ELEMENT);
}

TEST_F(Dem_GetNextExtendedDataRecord_Test, Dem_GetNextExtendedDataRecord_NG_BufferTooSmall)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ConfirmEvent0();
    SelectRecord(DEM_EXTENDED_DATA_RECORD_NUMBER);
    uint8  buf[4] = { 0xAAU, 0xAAU, 0xAAU, 0xAAU };
    uint16 bufSize = DEM_EXTENDED_DATA_RECORD_SIZE - 1U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetNextExtendedDataRecord(0U, buf, &bufSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, DEM_BUFFER_TOO_SMALL);
    EXPECT_EQ(buf[0], 0xAAU);   // バッファは書き換えない
    EXPECT_EQ(bufSize, DEM_EXTENDED_DATA_RECORD_SIZE - 1U);
}

TEST_F(Dem_GetNextExtendedDataRecord_Test, Dem_GetNextExtendedDataRecord_NG_BufferTooSmallDoesNotConsumeTheRecord)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ConfirmEvent0();
    SelectRecord(DEM_EXTENDED_DATA_RECORD_NUMBER);
    uint8  buf[4] = { 0U };
    uint16 tooSmall = 1U;
    ASSERT_EQ(Dem_GetNextExtendedDataRecord(0U, buf, &tooSmall), DEM_BUFFER_TOO_SMALL);
    uint16 bufSize = sizeof(buf);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetNextExtendedDataRecord(0U, buf, &bufSize);   // 大きいバッファで、もう一度

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(bufSize, DEM_EXTENDED_DATA_RECORD_SIZE);
}

TEST_F(Dem_GetNextExtendedDataRecord_Test, Dem_GetNextExtendedDataRecord_NG_SecondCallHasNoFurtherElement)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ConfirmEvent0();
    SelectRecord(DEM_EXTENDED_DATA_RECORD_NUMBER);
    uint8  buf[4] = { 0xAAU, 0xAAU, 0xAAU, 0xAAU };
    uint16 bufSize = sizeof(buf);
    ASSERT_EQ(Dem_GetNextExtendedDataRecord(0U, buf, &bufSize), E_OK);
    bufSize = sizeof(buf);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetNextExtendedDataRecord(0U, buf, &bufSize);   // 続けて、もう一度

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, DEM_NO_SUCH_ELEMENT);   // 1 回の選択で得られるレコードは、渡し済み
}

TEST_F(Dem_GetNextExtendedDataRecord_Test, Dem_GetNextExtendedDataRecord_OK_ReturnsAnEmptyBufferWhenNotRecorded)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    SelectRecord(DEM_EXTENDED_DATA_RECORD_NUMBER);   // 一度も確定 FAILED していない
    uint8  buf[4] = { 0xAAU, 0xAAU, 0xAAU, 0xAAU };
    uint16 bufSize = sizeof(buf);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetNextExtendedDataRecord(0U, buf, &bufSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(bufSize, 0U);   // [SWS_Dem_00631]
    EXPECT_EQ(buf[0], 0xAAU);
}

TEST_F(Dem_GetNextExtendedDataRecord_Test, Dem_GetNextExtendedDataRecord_OK_ReturnsTheRecordNumberAndTheCounter)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ConfirmEvent0();
    SelectRecord(DEM_EXTENDED_DATA_RECORD_NUMBER);
    uint8  buf[4] = { 0xAAU, 0xAAU, 0xAAU, 0xAAU };
    uint16 bufSize = sizeof(buf);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetNextExtendedDataRecord(0U, buf, &bufSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    ASSERT_EQ(bufSize, DEM_EXTENDED_DATA_RECORD_SIZE);
    EXPECT_EQ(buf[0], DEM_EXTENDED_DATA_RECORD_NUMBER);   // [SWS_Dem_00075]: 先頭がレコード番号
    EXPECT_EQ(buf[1], 1U);   // 確定 FAILED は 1 回
    EXPECT_EQ(buf[2], 0xAAU);
}

TEST_F(Dem_GetNextExtendedDataRecord_Test, Dem_GetNextExtendedDataRecord_OK_AllRecordsReturnTheActualRecordNumber)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ConfirmEvent0();
    SelectRecord(DEM_EXTENDED_DATA_NUMBER_ALL);
    uint8  buf[4] = { 0xAAU, 0xAAU, 0xAAU, 0xAAU };
    uint16 bufSize = sizeof(buf);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetNextExtendedDataRecord(0U, buf, &bufSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    ASSERT_EQ(bufSize, DEM_EXTENDED_DATA_RECORD_SIZE);
    EXPECT_EQ(buf[0], DEM_EXTENDED_DATA_RECORD_NUMBER);   // 要求の 0xFF ではなく、実際のレコード番号
}

}  // namespace
