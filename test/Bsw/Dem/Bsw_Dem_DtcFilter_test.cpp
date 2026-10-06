/**
 * \file    Bsw_Dem_DtcFilter_test.cpp
 * \brief   Dem の DTC フィルタ API（Dem_SetDTCFilter / Dem_GetNumberOfFilteredDTC /
 *          Dem_GetNextFilteredDTC / Dem_GetNextFilteredDTCAndFDC）の動作の単体テスト
 * \details [SWS_Dem_00208]/[SWS_Dem_00214]/[SWS_Dem_00215]/[SWS_Dem_00227]。
 *          UDS 0x19 の 0x01/0x02/0x0A/0x14 が Dcm 経由で使う引数の組み合わせ
 *          （DTCStatusMask の値、FilterForFaultDetectionCounter）ごとに、
 *          返る DTC の集合・件数・終端（DEM_NO_SUCH_ELEMENT）を確認する。
 *          以前の独自関数 Dem_GetAllDTCs / Dem_GetSupportedDTCs /
 *          Dem_GetPrefailedDTCs と同じ結果になることが置き換えの条件。
 *
 *          Det の NG ケースは Bsw_Dem_test.cpp にある。GoogleTest の main() は
 *          test_main.cpp に集約しているため、本ファイルでは定義しない。
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

/* Dem のフィルタ API（Dem_SetDTCFilter / Dem_GetNumberOfFilteredDTC / Dem_GetNextFilteredDTC 系）の単体テスト用フィクスチャ。
 * SetUp(): Dem_Init(NULL) を呼ぶ。SetFilter() はフィルタを設定する補助関数、DrainDtcs() は一致する DTC を最後まで取り出す補助関数。
 * TearDown(): Dem を未初期化の状態へ戻す。 */
class Bsw_Dem_DtcFilter_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        WrapDem_Reset();
        FakeDetHw_LogSuppressed = 1U;
        Dem_Init(NULL);
        FakeDetHw_Reset();
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Dem_Test_ResetInitState();
    }

    static Std_ReturnType SetFilter(uint8 mask, boolean forFdc = FALSE)
    {
        return Dem_SetDTCFilter(0U, mask, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, 0U, forFdc);
    }

    /** 現在のフィルタで Dem_GetNextFilteredDTC() を最後まで回した件数（終端の戻り値も確認する）。 */
    static uint8 DrainDtcs(uint32* dtcOut, uint8* statusOut)
    {
        uint8 n = 0U;
        for (uint8 guard = 0U; guard < (DEM_EVENT_COUNT + 2U); guard++)
        {
            uint32 dtc = 0U;
            uint8  status = 0U;
            const Std_ReturnType ret = Dem_GetNextFilteredDTC(0U, &dtc, &status);
            if (ret == DEM_NO_SUCH_ELEMENT)
            {
                return n;
            }
            EXPECT_EQ(ret, E_OK);
            if (ret != E_OK)
            {
                return n;
            }
            dtcOut[n]    = dtc;
            statusOut[n] = status;
            n++;
        }
        ADD_FAILURE() << "DEM_NO_SUCH_ELEMENT が返らない";
        return n;
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Bsw_Dem_DtcFilter_Test, Dem_GetNextFilteredDTC_NG_NoFilterSetReturnsNotOk)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint32 dtc = 0U;
    uint8  status = 0U;
    uint16 count = 0U;
    sint8  fdc = 0;

    EXPECT_EQ(Dem_GetNumberOfFilteredDTC(0U, &count), E_NOT_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Dem_GetNextFilteredDTC(0U, &dtc, &status);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(Dem_GetNextFilteredDTCAndFDC(0U, &dtc, &fdc), E_NOT_OK);
}

TEST_F(Bsw_Dem_DtcFilter_Test, Dem_SetDTCFilter_OK_CallingAgainRestartsFromTheFirstDtc)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    uint32 dtc = 0U;
    uint8  status = 0U;
    Std_ReturnType retSet1  = SetFilter(0x00U);
    Std_ReturnType retNext1 = Dem_GetNextFilteredDTC(0U, &dtc, &status);
    const uint32   first    = dtc;
    Std_ReturnType retNext2 = Dem_GetNextFilteredDTC(0U, &dtc, &status);
    const uint32   second   = dtc;

    Std_ReturnType retSet2  = SetFilter(0x00U);  // 設定し直すと走査位置が先頭へ戻る
    Std_ReturnType retNext3 = Dem_GetNextFilteredDTC(0U, &dtc, &status);
    const uint32   third    = dtc;

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_EQ(retSet1, E_OK);
    ASSERT_EQ(retNext1, E_OK);
    ASSERT_EQ(retNext2, E_OK);
    ASSERT_NE(second, first);

    ASSERT_EQ(retSet2, E_OK);
    ASSERT_EQ(retNext3, E_OK);

    EXPECT_EQ(third, first);
}

}  // namespace
