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

TEST_F(Bsw_Dem_DtcFilter_Test, Dem_GetNextFilteredDTC_OK_MaskZeroReportsAllSupportedDtcs)
{
    // 0x19/0x0A: DTCStatusMask=0x00 はステータスで絞り込まず、対応する全 DTC を返す。
    ASSERT_EQ(SetFilter(0x00U), E_OK);
    uint16 count = 0U;
    ASSERT_EQ(Dem_GetNumberOfFilteredDTC(0U, &count), E_OK);
    EXPECT_EQ(count, DEM_EVENT_COUNT);

    uint32 dtcs[DEM_EVENT_COUNT + 2U];
    uint8  statuses[DEM_EVENT_COUNT + 2U];
    const uint8 n = DrainDtcs(dtcs, statuses);

    EXPECT_EQ(n, DEM_EVENT_COUNT);
    EXPECT_EQ(dtcs[0], DEM_DTC_ENGINE_OVERHEAT);
    EXPECT_EQ(dtcs[DEM_EVENT_COUNT - 1U], DEM_DTC_NVM_REQ_FAILED);
}

TEST_F(Bsw_Dem_DtcFilter_Test, Dem_GetNextFilteredDTC_OK_StatusMaskSelectsMatchingDtcsOnly)
{
    // 初期状態は全イベントとも testNotCompletedSinceLastClear(0x10) のみ。
    // BUTTON_STUCK（デバウンス閾値 1）を FAILED 確定させると TF/PENDING/CONFIRMED 等が立つ。
    ASSERT_EQ(Dem_SetEventStatus(DEM_EVENT_BUTTON_STUCK, DEM_EVENT_STATUS_FAILED), E_OK);

    ASSERT_EQ(SetFilter(DEM_STATUS_CONFIRMED), E_OK);
    uint16 count = 0U;
    ASSERT_EQ(Dem_GetNumberOfFilteredDTC(0U, &count), E_OK);
    EXPECT_EQ(count, 1U);

    uint32 dtcs[DEM_EVENT_COUNT + 2U];
    uint8  statuses[DEM_EVENT_COUNT + 2U];
    const uint8 n = DrainDtcs(dtcs, statuses);

    ASSERT_EQ(n, 1U);
    EXPECT_EQ(dtcs[0], DEM_DTC_BUTTON_STUCK);
    EXPECT_NE(statuses[0] & DEM_STATUS_CONFIRMED, 0U);
}

TEST_F(Bsw_Dem_DtcFilter_Test, Dem_GetNextFilteredDTC_OK_StatusMaskWithNoMatchReturnsNoElementImmediately)
{
    // 誰も confirmed ではない状態で confirmed だけを要求する → 0 件。
    ASSERT_EQ(SetFilter(DEM_STATUS_CONFIRMED), E_OK);
    uint32 dtc = 0U;
    uint8  status = 0U;

    EXPECT_EQ(Dem_GetNextFilteredDTC(0U, &dtc, &status), DEM_NO_SUCH_ELEMENT);
}

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

TEST_F(Bsw_Dem_DtcFilter_Test, Dem_GetNextFilteredDTCAndFDC_OK_FdcFilterReportsOnlyPrefailedDtcs)
{
    // 0x19/0x14: FilterForFaultDetectionCounter=TRUE。ENGINE_OVERHEAT（閾値 2）に FAILED を 1 回だけ
    // 報告すると、確定前の prefailed（FDC が 1〜0x7E）になる。BUTTON_STUCK（閾値 1）は確定済みで対象外。
    ASSERT_EQ(Dem_SetEventStatus(DEM_EVENT_ENGINE_OVERHEAT, DEM_EVENT_STATUS_FAILED), E_OK);
    ASSERT_EQ(Dem_SetEventStatus(DEM_EVENT_BUTTON_STUCK, DEM_EVENT_STATUS_FAILED), E_OK);

    ASSERT_EQ(SetFilter(0x00U, TRUE), E_OK);
    uint16 count = 0U;
    ASSERT_EQ(Dem_GetNumberOfFilteredDTC(0U, &count), E_OK);
    EXPECT_EQ(count, 1U);

    uint32 dtc = 0U;
    sint8  fdc = 0;
    ASSERT_EQ(Dem_GetNextFilteredDTCAndFDC(0U, &dtc, &fdc), E_OK);
    EXPECT_EQ(dtc, DEM_DTC_ENGINE_OVERHEAT);
    EXPECT_GE(fdc, 1);
    EXPECT_LE(fdc, 0x7E);
    EXPECT_EQ(Dem_GetNextFilteredDTCAndFDC(0U, &dtc, &fdc), DEM_NO_SUCH_ELEMENT);
}

TEST_F(Bsw_Dem_DtcFilter_Test, Dem_GetNextFilteredDTCAndFDC_OK_NoPrefailedDtcReturnsNoElement)
{
    ASSERT_EQ(SetFilter(0x00U, TRUE), E_OK);
    uint16 count = 99U;
    ASSERT_EQ(Dem_GetNumberOfFilteredDTC(0U, &count), E_OK);
    EXPECT_EQ(count, 0U);

    uint32 dtc = 0U;
    sint8  fdc = 0;
    EXPECT_EQ(Dem_GetNextFilteredDTCAndFDC(0U, &dtc, &fdc), DEM_NO_SUCH_ELEMENT);
}

}  // namespace
