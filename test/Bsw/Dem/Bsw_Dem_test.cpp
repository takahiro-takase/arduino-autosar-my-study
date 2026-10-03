/**
 * \file    Bsw_Dem_test.cpp
 * \brief   Dem.c（src/Bsw/Dem/Dem.c）の単体テスト（NG系）
 * \details Dem.c 全公開APIの `Det_ReportError()` 呼び出し箇所（NG ケースのみ）を
 *          1ファイルにまとめる。各ケースで、報告される ErrorId が仕様どおり
 *          正しい値になっていることを `Fake_Det_Hw.h`（`FakeDetHw_LastErrorId`）
 *          で検証する。
 *
 *          Dem には `Dem_DeInit()` に相当する API が無く、`Dem_Initialized` は
 *          native_chain_tests バイナリ全体で共有される static のため、
 *          `DEM_E_UNINIT` の検証には `Dem_Test_ResetInitState()`
 *          （`DEM_UNIT_TEST` ビルドのみに存在するテスト専用関数、
 *          `Mcu_Test_ResetInitState()` と同じ設計）を使う。
 *
 *          `Dem_Init()` は NvM_ReadBlock() が常に E_NOT_OK を返す
 *          `Fake_NvM.c` スタブ経由で毎回「初回起動」として決定的にリセット
 *          されるため（`Fake_NvM.c` 冒頭コメント参照）、他のテストファイルが
 *          先に `Dem_Init()` を呼んでいても本ファイルの SetUp() で安全に
 *          再初期化できる（`Dem_Init()` 自体に「既に初期化済み」チェックは無い）。
 *
 *          GoogleTest の main() は test_main.cpp に集約しているため、
 *          本ファイルでは定義しない。
 */
#include <gtest/gtest.h>

extern "C" {
#include "Dem.h"
#include "Fake_Det_Hw.h"
#include "Wrap_Dem.h"
}

namespace
{

class Bsw_Dem_Test : public ::testing::Test
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

    static const Dem_EventIdType kInvalidEventId = DEM_EVENT_COUNT;
};

// ------------------------------------------------------------
// Dem_GetVersionInfo()（[SWS_Dem_00124]により未初期化チェック対象外の例外API）
// ------------------------------------------------------------

TEST_F(Bsw_Dem_Test, Dem_GetVersionInfo_NG_NullPointer)
{
    Dem_GetVersionInfo(NULL);

    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// Dem_ClearDTC()
// ------------------------------------------------------------

TEST_F(Bsw_Dem_Test, Dem_ClearDTC_NG_Uninit)
{
    Dem_Test_ResetInitState();

    Std_ReturnType ret = Dem_ClearDTC(0U, DEM_GROUP_ALL_DTCS, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_UNINIT);
}

TEST_F(Bsw_Dem_Test, Dem_ClearDTC_NG_WrongConfiguration)
{
    Std_ReturnType ret = Dem_ClearDTC(0U, DEM_GROUP_ALL_DTCS, DEM_DTC_FORMAT_UDS, static_cast<Dem_DTCOriginType>(0xFFFFU));

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_WRONG_CONFIGURATION);
}

// ------------------------------------------------------------
// Dem_GetEventUdsStatus()
// ------------------------------------------------------------

TEST_F(Bsw_Dem_Test, Dem_GetEventUdsStatus_NG_Uninit)
{
    Dem_Test_ResetInitState();
    Dem_UdsStatusByteType status;

    Std_ReturnType ret = Dem_GetEventUdsStatus(0U, &status);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_UNINIT);
}

TEST_F(Bsw_Dem_Test, Dem_GetEventUdsStatus_NG_WrongConfiguration)
{
    Dem_UdsStatusByteType status;

    Std_ReturnType ret = Dem_GetEventUdsStatus(kInvalidEventId, &status);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_WRONG_CONFIGURATION);
}

TEST_F(Bsw_Dem_Test, Dem_GetEventUdsStatus_NG_NullPointer)
{
    Std_ReturnType ret = Dem_GetEventUdsStatus(0U, NULL);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// Dem_GetDTCOfEvent()
// ------------------------------------------------------------

TEST_F(Bsw_Dem_Test, Dem_GetDTCOfEvent_NG_Uninit)
{
    Dem_Test_ResetInitState();
    uint32 dtc;

    Std_ReturnType ret = Dem_GetDTCOfEvent(0U, DEM_DTC_FORMAT_UDS, &dtc);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_UNINIT);
}

TEST_F(Bsw_Dem_Test, Dem_GetDTCOfEvent_NG_WrongConfiguration)
{
    uint32 dtc;

    Std_ReturnType ret = Dem_GetDTCOfEvent(kInvalidEventId, DEM_DTC_FORMAT_UDS, &dtc);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_WRONG_CONFIGURATION);
}

TEST_F(Bsw_Dem_Test, Dem_GetDTCOfEvent_NG_NullPointer)
{
    Std_ReturnType ret = Dem_GetDTCOfEvent(0U, DEM_DTC_FORMAT_UDS, NULL);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// Dem_GetFaultDetectionCounter()
// ------------------------------------------------------------

TEST_F(Bsw_Dem_Test, Dem_GetFaultDetectionCounter_NG_Uninit)
{
    Dem_Test_ResetInitState();
    sint8 fdc;

    Std_ReturnType ret = Dem_GetFaultDetectionCounter(0U, &fdc);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_UNINIT);
}

TEST_F(Bsw_Dem_Test, Dem_GetFaultDetectionCounter_NG_WrongConfiguration)
{
    sint8 fdc;

    Std_ReturnType ret = Dem_GetFaultDetectionCounter(kInvalidEventId, &fdc);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_WRONG_CONFIGURATION);
}

TEST_F(Bsw_Dem_Test, Dem_GetFaultDetectionCounter_NG_NullPointer)
{
    Std_ReturnType ret = Dem_GetFaultDetectionCounter(0U, NULL);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// Dem_SetEventStatus()（[SWS_Dem_00124]により未初期化チェック対象外の例外API）
// ------------------------------------------------------------

TEST_F(Bsw_Dem_Test, Dem_SetEventStatus_NG_WrongConfiguration)
{
    Std_ReturnType ret = Dem_SetEventStatus(kInvalidEventId, DEM_EVENT_STATUS_FAILED);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_WRONG_CONFIGURATION);
}

TEST_F(Bsw_Dem_Test, Dem_SetEventStatus_NG_ParamData)
{
    // PREPASSED/PREFAILED はモニタからの入力としては受け付けない
    // （ファイル冒頭コメント、Dem.c の Dem_SetEventStatus() 実装コメント参照）。
    Std_ReturnType ret = Dem_SetEventStatus(0U, DEM_EVENT_STATUS_PREFAILED);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_PARAM_DATA);
}

// ------------------------------------------------------------
// Dem_GetDTCStatusAvailabilityMask()
// ------------------------------------------------------------

TEST_F(Bsw_Dem_Test, Dem_GetDTCStatusAvailabilityMask_NG_Uninit)
{
    Dem_Test_ResetInitState();
    Dem_UdsStatusByteType mask;

    Std_ReturnType ret = Dem_GetDTCStatusAvailabilityMask(0U, &mask);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_UNINIT);
}

TEST_F(Bsw_Dem_Test, Dem_GetDTCStatusAvailabilityMask_NG_NullPointer)
{
    Std_ReturnType ret = Dem_GetDTCStatusAvailabilityMask(0U, NULL);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// Dem_DisableDTCSetting()
// ------------------------------------------------------------

TEST_F(Bsw_Dem_Test, Dem_DisableDTCSetting_NG_Uninit)
{
    Dem_Test_ResetInitState();

    Std_ReturnType ret = Dem_DisableDTCSetting(0U);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_UNINIT);
}

// ------------------------------------------------------------
// Dem_EnableDTCSetting()
// ------------------------------------------------------------

TEST_F(Bsw_Dem_Test, Dem_EnableDTCSetting_NG_Uninit)
{
    Dem_Test_ResetInitState();

    Std_ReturnType ret = Dem_EnableDTCSetting(0U);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_UNINIT);
}

// ------------------------------------------------------------
// Dem_SetDTCFilter()
// ------------------------------------------------------------

TEST_F(Bsw_Dem_Test, Dem_SetDTCFilter_NG_Uninit)
{
    Dem_Test_ResetInitState();

    Std_ReturnType ret = Dem_SetDTCFilter(0U, 0xFFU, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, 0U, FALSE);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_UNINIT);
}

TEST_F(Bsw_Dem_Test, Dem_SetDTCFilter_NG_UnsupportedFormat)
{
    Std_ReturnType ret = Dem_SetDTCFilter(0U, 0xFFU, DEM_DTC_FORMAT_OBD, DEM_DTC_ORIGIN_PRIMARY_MEMORY, FALSE, 0U, FALSE);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_WRONG_CONFIGURATION);
}

TEST_F(Bsw_Dem_Test, Dem_SetDTCFilter_NG_UnsupportedOrigin)
{
    Std_ReturnType ret = Dem_SetDTCFilter(0U, 0xFFU, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_MIRROR_MEMORY, FALSE, 0U, FALSE);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_WRONG_CONFIGURATION);
}

TEST_F(Bsw_Dem_Test, Dem_SetDTCFilter_NG_FilterWithSeverityNotSupported)
{
    Std_ReturnType ret = Dem_SetDTCFilter(0U, 0xFFU, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY, TRUE, 0x01U, FALSE);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_WRONG_CONFIGURATION);
}

// ------------------------------------------------------------
// Dem_GetNumberOfFilteredDTC()
// ------------------------------------------------------------

TEST_F(Bsw_Dem_Test, Dem_GetNumberOfFilteredDTC_NG_Uninit)
{
    Dem_Test_ResetInitState();
    uint16 count = 0U;

    Std_ReturnType ret = Dem_GetNumberOfFilteredDTC(0U, &count);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_UNINIT);
}

TEST_F(Bsw_Dem_Test, Dem_GetNumberOfFilteredDTC_NG_NullPointer)
{
    Std_ReturnType ret = Dem_GetNumberOfFilteredDTC(0U, NULL);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// Dem_GetNextFilteredDTC()
// ------------------------------------------------------------

TEST_F(Bsw_Dem_Test, Dem_GetNextFilteredDTC_NG_Uninit)
{
    Dem_Test_ResetInitState();
    uint32 dtc = 0U;
    uint8  status = 0U;

    Std_ReturnType ret = Dem_GetNextFilteredDTC(0U, &dtc, &status);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_UNINIT);
}

TEST_F(Bsw_Dem_Test, Dem_GetNextFilteredDTC_NG_NullPointer)
{
    uint8 status = 0U;

    Std_ReturnType ret = Dem_GetNextFilteredDTC(0U, NULL, &status);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// Dem_GetNextFilteredDTCAndFDC()
// ------------------------------------------------------------

TEST_F(Bsw_Dem_Test, Dem_GetNextFilteredDTCAndFDC_NG_Uninit)
{
    Dem_Test_ResetInitState();
    uint32 dtc = 0U;
    sint8  fdc = 0;

    Std_ReturnType ret = Dem_GetNextFilteredDTCAndFDC(0U, &dtc, &fdc);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_UNINIT);
}

TEST_F(Bsw_Dem_Test, Dem_GetNextFilteredDTCAndFDC_NG_NullPointer)
{
    sint8 fdc = 0;

    Std_ReturnType ret = Dem_GetNextFilteredDTCAndFDC(0U, NULL, &fdc);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// Dem_SetFreezeFrameContext()
// ------------------------------------------------------------

TEST_F(Bsw_Dem_Test, Dem_SetFreezeFrameContext_NG_Uninit)
{
    Dem_Test_ResetInitState();

    Dem_SetFreezeFrameContext(0U, 0U, 0U);

    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_UNINIT);
}

// ------------------------------------------------------------
// Dem_GetFreezeFrameOfEvent()
// ------------------------------------------------------------

TEST_F(Bsw_Dem_Test, Dem_GetFreezeFrameOfEvent_NG_Uninit)
{
    Dem_Test_ResetInitState();
    Dem_FreezeFrameType frame;

    Std_ReturnType ret = Dem_GetFreezeFrameOfEvent(0U, &frame);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_UNINIT);
}

TEST_F(Bsw_Dem_Test, Dem_GetFreezeFrameOfEvent_NG_WrongConfiguration)
{
    Dem_FreezeFrameType frame;

    Std_ReturnType ret = Dem_GetFreezeFrameOfEvent(kInvalidEventId, &frame);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_WRONG_CONFIGURATION);
}

TEST_F(Bsw_Dem_Test, Dem_GetFreezeFrameOfEvent_NG_NullPointer)
{
    Std_ReturnType ret = Dem_GetFreezeFrameOfEvent(0U, NULL);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// Dem_GetEventIdOfDTC()
// ------------------------------------------------------------

TEST_F(Bsw_Dem_Test, Dem_GetEventIdOfDTC_NG_Uninit)
{
    Dem_Test_ResetInitState();
    Dem_EventIdType eventId;

    Std_ReturnType ret = Dem_GetEventIdOfDTC(0x123456UL, &eventId);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_UNINIT);
}

TEST_F(Bsw_Dem_Test, Dem_GetEventIdOfDTC_NG_NullPointer)
{
    Std_ReturnType ret = Dem_GetEventIdOfDTC(0x123456UL, NULL);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// Dem_GetOccurrenceCounterOfEvent()
// ------------------------------------------------------------

TEST_F(Bsw_Dem_Test, Dem_GetOccurrenceCounterOfEvent_NG_Uninit)
{
    Dem_Test_ResetInitState();
    uint8 counter;

    Std_ReturnType ret = Dem_GetOccurrenceCounterOfEvent(0U, &counter);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_UNINIT);
}

TEST_F(Bsw_Dem_Test, Dem_GetOccurrenceCounterOfEvent_NG_WrongConfiguration)
{
    uint8 counter;

    Std_ReturnType ret = Dem_GetOccurrenceCounterOfEvent(kInvalidEventId, &counter);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_WRONG_CONFIGURATION);
}

TEST_F(Bsw_Dem_Test, Dem_GetOccurrenceCounterOfEvent_NG_NullPointer)
{
    Std_ReturnType ret = Dem_GetOccurrenceCounterOfEvent(0U, NULL);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_PARAM_POINTER);
}

}  // namespace
