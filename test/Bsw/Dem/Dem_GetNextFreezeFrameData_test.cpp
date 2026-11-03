/**
 * \file    Dem_GetNextFreezeFrameData_test.cpp
 * \brief   Dem_GetNextFreezeFrameData() の単体テスト
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
class Dem_GetNextFreezeFrameData_Test : public ::testing::Test
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

TEST_F(Dem_GetNextFreezeFrameData_Test, Dem_GetNextFreezeFrameData_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dem_Test_ResetInitState();
    uint8  buf[16] = { 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU };
    uint16 bufSize = sizeof(buf);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetNextFreezeFrameData(0U, buf, &bufSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_UNINIT);
}

TEST_F(Dem_GetNextFreezeFrameData_Test, Dem_GetNextFreezeFrameData_NG_InvalidClientId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8  buf[16] = { 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU };
    uint16 bufSize = sizeof(buf);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetNextFreezeFrameData(DEM_CLIENT_COUNT, buf, &bufSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_WRONG_CONFIGURATION);
}

TEST_F(Dem_GetNextFreezeFrameData_Test, Dem_GetNextFreezeFrameData_NG_NullBuffer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    SelectRecord(DEM_FREEZE_FRAME_RECORD_NUMBER);
    uint16 bufSize = 16U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetNextFreezeFrameData(0U, NULL, &bufSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_PARAM_POINTER);
}

TEST_F(Dem_GetNextFreezeFrameData_Test, Dem_GetNextFreezeFrameData_NG_NullBufSize)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    SelectRecord(DEM_FREEZE_FRAME_RECORD_NUMBER);
    uint8 buf[16] = { 0U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetNextFreezeFrameData(0U, buf, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_PARAM_POINTER);
}

TEST_F(Dem_GetNextFreezeFrameData_Test, Dem_GetNextFreezeFrameData_NG_NoDtcSelection)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8  buf[16] = { 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU };
    uint16 bufSize = sizeof(buf);   // Dem_SelectDTC() を呼ばない

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetNextFreezeFrameData(0U, buf, &bufSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_WRONG_CONDITION);
}

TEST_F(Dem_GetNextFreezeFrameData_Test, Dem_GetNextFreezeFrameData_NG_NoRecordSelection)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SelectDTC(0U, ValidDtc(), DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY), E_OK);
    uint8  buf[16] = { 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU };
    uint16 bufSize = sizeof(buf);   // Dem_SelectFreezeFrameData() を呼ばない

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetNextFreezeFrameData(0U, buf, &bufSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_WRONG_CONDITION);   // [SWS_Dem_01271]
}

TEST_F(Dem_GetNextFreezeFrameData_Test, Dem_GetNextFreezeFrameData_NG_GroupOfAllDtcsIsNotASingleDtc)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SelectDTC(0U, DEM_GROUP_ALL_DTCS, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY), E_OK);
    ASSERT_EQ(Dem_SelectFreezeFrameData(0U, DEM_FREEZE_FRAME_RECORD_NUMBER), E_OK);
    uint8  buf[16] = { 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU };
    uint16 bufSize = sizeof(buf);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetNextFreezeFrameData(0U, buf, &bufSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, DEM_WRONG_DTC);
}

TEST_F(Dem_GetNextFreezeFrameData_Test, Dem_GetNextFreezeFrameData_NG_UnsupportedRecordNumber)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ConfirmEvent0();
    SelectRecord(0x02U);
    uint8  buf[16] = { 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU };
    uint16 bufSize = sizeof(buf);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetNextFreezeFrameData(0U, buf, &bufSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, DEM_NO_SUCH_ELEMENT);
}

TEST_F(Dem_GetNextFreezeFrameData_Test, Dem_GetNextFreezeFrameData_NG_BufferTooSmall)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ConfirmEvent0();
    SelectRecord(DEM_FREEZE_FRAME_RECORD_NUMBER);
    uint8  buf[16] = { 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU };
    uint16 bufSize = DEM_FREEZE_FRAME_RECORD_SIZE - 1U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetNextFreezeFrameData(0U, buf, &bufSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, DEM_BUFFER_TOO_SMALL);
    EXPECT_EQ(buf[0], 0xAAU);   // バッファは書き換えない
    EXPECT_EQ(bufSize, DEM_FREEZE_FRAME_RECORD_SIZE - 1U);
}

TEST_F(Dem_GetNextFreezeFrameData_Test, Dem_GetNextFreezeFrameData_NG_BufferTooSmallDoesNotConsumeTheRecord)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ConfirmEvent0();
    SelectRecord(DEM_FREEZE_FRAME_RECORD_NUMBER);
    uint8  buf[16] = { 0U };
    uint16 tooSmall = 1U;
    ASSERT_EQ(Dem_GetNextFreezeFrameData(0U, buf, &tooSmall), DEM_BUFFER_TOO_SMALL);
    uint16 bufSize = sizeof(buf);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetNextFreezeFrameData(0U, buf, &bufSize);   // 大きいバッファで、もう一度

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(bufSize, DEM_FREEZE_FRAME_RECORD_SIZE);
}

TEST_F(Dem_GetNextFreezeFrameData_Test, Dem_GetNextFreezeFrameData_NG_SecondCallHasNoFurtherElement)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ConfirmEvent0();
    SelectRecord(DEM_FREEZE_FRAME_RECORD_NUMBER);
    uint8  buf[16] = { 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU };
    uint16 bufSize = sizeof(buf);
    ASSERT_EQ(Dem_GetNextFreezeFrameData(0U, buf, &bufSize), E_OK);
    bufSize = sizeof(buf);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetNextFreezeFrameData(0U, buf, &bufSize);   // 続けて、もう一度

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, DEM_NO_SUCH_ELEMENT);   // 1 回の選択で得られるレコードは、渡し済み
}

TEST_F(Dem_GetNextFreezeFrameData_Test, Dem_GetNextFreezeFrameData_OK_ReturnsAnEmptyBufferWhenNotRecorded)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    SelectRecord(DEM_FREEZE_FRAME_RECORD_NUMBER);   // 一度も確定 FAILED していない
    uint8  buf[16] = { 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU };
    uint16 bufSize = sizeof(buf);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetNextFreezeFrameData(0U, buf, &bufSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(bufSize, 0U);   // [SWS_Dem_00630]
    EXPECT_EQ(buf[0], 0xAAU);
}

TEST_F(Dem_GetNextFreezeFrameData_Test, Dem_GetNextFreezeFrameData_OK_ReturnsTheRecordWithTheDidsAndTheDataAtConfirmation)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FakeRte_EngineSpeed = 0x1234U;   // 確定の瞬間の値が、記録される
    FakeRte_CoolantTemp = 0x56U;
    ConfirmEvent0();
    FakeRte_EngineSpeed = 0x9999U;   // 確定のあとの値の変化は、記録に影響しない
    FakeRte_CoolantTemp = 0x99U;
    SelectRecord(DEM_FREEZE_FRAME_RECORD_NUMBER);
    uint8  buf[16] = { 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU };
    uint16 bufSize = sizeof(buf);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetNextFreezeFrameData(0U, buf, &bufSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    ASSERT_EQ(bufSize, DEM_FREEZE_FRAME_RECORD_SIZE);
    const uint8 expected[DEM_FREEZE_FRAME_RECORD_SIZE] = {
        DEM_FREEZE_FRAME_RECORD_NUMBER, DEM_FREEZE_FRAME_DID_COUNT,   // [SWS_Dem_00071]: {レコード番号, DID 数,
        0x01U, 0x01U, 0x12U, 0x34U,                                   //  DID 0x0101, EngineSpeed 0x1234,
        0x01U, 0x02U, 0x56U,                                          //  DID 0x0102, CoolantTemp 0x56,
        0x01U, 0x03U, 0x00U                                           //  DID 0x0103, EngineState 0（OFF）}
    };
    for (uint8 i = 0U; i < DEM_FREEZE_FRAME_RECORD_SIZE; i++)
    {
        EXPECT_EQ(buf[i], expected[i]) << "byte " << (unsigned)i;
    }
    EXPECT_EQ(buf[DEM_FREEZE_FRAME_RECORD_SIZE], 0xAAU);   // レコードの外は、書き換えない
}

TEST_F(Dem_GetNextFreezeFrameData_Test, Dem_GetNextFreezeFrameData_OK_AllRecordsReturnTheActualRecordNumber)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ConfirmEvent0();
    SelectRecord(DEM_RECORD_NUMBER_ALL);
    uint8  buf[16] = { 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU };
    uint16 bufSize = sizeof(buf);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetNextFreezeFrameData(0U, buf, &bufSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    ASSERT_EQ(bufSize, DEM_FREEZE_FRAME_RECORD_SIZE);
    EXPECT_EQ(buf[0], DEM_FREEZE_FRAME_RECORD_NUMBER);   // 要求の 0xFF ではなく、実際のレコード番号
}

TEST_F(Dem_GetNextFreezeFrameData_Test, Dem_GetNextFreezeFrameData_OK_FillsInvalidDataWhenTheReadFunctionFails)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FakeRte_DataReadFails = TRUE;   // データ要素の読み出しが、全て失敗する
    ConfirmEvent0();
    FakeRte_DataReadFails = FALSE;
    SelectRecord(DEM_FREEZE_FRAME_RECORD_NUMBER);
    uint8  buf[16] = { 0U };
    uint16 bufSize = sizeof(buf);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_GetNextFreezeFrameData(0U, buf, &bufSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    ASSERT_EQ(bufSize, DEM_FREEZE_FRAME_RECORD_SIZE);
    const uint8 expected[DEM_FREEZE_FRAME_RECORD_SIZE] = {
        DEM_FREEZE_FRAME_RECORD_NUMBER, DEM_FREEZE_FRAME_DID_COUNT,
        0x01U, 0x01U, 0xFFU, 0xFFU,   // 読めなかったデータ要素は、0xFF（無効なデータ）で埋める
        0x01U, 0x02U, 0xFFU,
        0x01U, 0x03U, 0xFFU
    };
    for (uint8 i = 0U; i < DEM_FREEZE_FRAME_RECORD_SIZE; i++)
    {
        EXPECT_EQ(buf[i], expected[i]) << "byte " << (unsigned)i;
    }
}

}  // namespace
