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
    uint32 dtc = 0U;
    uint8  status = 0U;
    uint16 count = 0U;
    sint8  fdc = 0;

    EXPECT_EQ(Dem_GetNumberOfFilteredDTC(0U, &count), E_NOT_OK);
    EXPECT_EQ(Dem_GetNextFilteredDTC(0U, &dtc, &status), E_NOT_OK);
    EXPECT_EQ(Dem_GetNextFilteredDTCAndFDC(0U, &dtc, &fdc), E_NOT_OK);
}

TEST_F(Bsw_Dem_DtcFilter_Test, Dem_SetDTCFilter_OK_CallingAgainRestartsFromTheFirstDtc)
{
    ASSERT_EQ(SetFilter(0x00U), E_OK);
    uint32 dtc = 0U;
    uint8  status = 0U;
    ASSERT_EQ(Dem_GetNextFilteredDTC(0U, &dtc, &status), E_OK);
    const uint32 first = dtc;
    ASSERT_EQ(Dem_GetNextFilteredDTC(0U, &dtc, &status), E_OK);
    ASSERT_NE(dtc, first);

    ASSERT_EQ(SetFilter(0x00U), E_OK);  // 設定し直すと走査位置が先頭へ戻る
    ASSERT_EQ(Dem_GetNextFilteredDTC(0U, &dtc, &status), E_OK);

    EXPECT_EQ(dtc, first);
}

}  // namespace
