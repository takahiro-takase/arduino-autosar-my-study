/**
 * \file    Dem_EnableDTCRecordUpdate_test.cpp
 * \brief   Dem_EnableDTCRecordUpdate() の単体テスト
 * \details 2026-11、Dem の第 4 段（レコードの更新の停止・再開、[SWS_Dem_00270]）として追加した。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Dem.h"
#include "Fake_Det_Hw.h"
#include "Fake_Rte.h"
#include "NvM.h"
#include "NvM_TestUtil.h"
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
 * SetUp(): wrap と Fake の RTE を戻し、NvM を空の EEPROM から初期化して、Dem_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消す）。
 * TearDown(): 未初期化の状態へ戻す。 */
class Dem_EnableDTCRecordUpdate_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        WrapDem_Reset();  // 他ファイルの故障注入が漏れ伝わらないよう防御的にリセット
        FakeRte_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        NvMTest_ColdBoot();            // 消去や再開は、不揮発メモリへの書き込みを積むため
        Dem_Init(NULL);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Dem_Test_ResetInitState();
        FakeRte_Reset();
        NvMTest_Shutdown();   // 次のテストへ NvM の初期化状態を残さない
    }

    // 構成にある DTC（イベント eventId の DTC）。
    static uint32 DtcOf(Dem_EventIdType eventId)
    {
        uint32 dtc = 0U;
        (void)Dem_GetDTCOfEvent(eventId, DEM_DTC_FORMAT_UDS, &dtc);
        return dtc;
    }

    // イベントを、確定 FAILED にする（FreezeFrame の取得と、ExtendedData の加算が起きる）。
    static void Confirm(Dem_EventIdType eventId)
    {
        for (uint8 i = 0U; i < 20U; i++)
        {
            (void)Dem_SetEventStatus(eventId, DEM_EVENT_STATUS_FAILED);
        }
    }

    // イベントを、回復させる（次に確定させると、もう一度、確定の遷移が起きる）。
    static void Recover(Dem_EventIdType eventId)
    {
        for (uint8 i = 0U; i < 20U; i++)
        {
            (void)Dem_SetEventStatus(eventId, DEM_EVENT_STATUS_PASSED);
        }
    }

    // イベントの DTC を選ぶ。
    static void Select(Dem_EventIdType eventId)
    {
        ASSERT_EQ(Dem_SelectDTC(0U, DtcOf(eventId), DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY), E_OK);
    }

    // 選んだ DTC の FreezeFrame の EngineSpeed（記録が無ければ、-1）。
    static int ReadSpeed()
    {
        uint8  buf[DEM_FREEZE_FRAME_RECORD_SIZE] = { 0U };
        uint16 size = sizeof(buf);
        EXPECT_EQ(Dem_SelectFreezeFrameData(0U, DEM_FREEZE_FRAME_RECORD_NUMBER), E_OK);
        EXPECT_EQ(Dem_GetNextFreezeFrameData(0U, buf, &size), E_OK);
        return (size == 0U) ? -1 : (int)(((unsigned)buf[4] << 8U) | (unsigned)buf[5]);
    }

    // 選んだ DTC の ExtendedData の故障確定回数（記録が無ければ、0）。
    static int ReadOccurrence()
    {
        uint8  buf[DEM_EXTENDED_DATA_RECORD_SIZE] = { 0U };
        uint16 size = sizeof(buf);
        EXPECT_EQ(Dem_SelectExtendedDataRecord(0U, DEM_EXTENDED_DATA_RECORD_NUMBER), E_OK);
        EXPECT_EQ(Dem_GetNextExtendedDataRecord(0U, buf, &size), E_OK);
        return (size == 0U) ? 0 : (int)buf[1];
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Dem_EnableDTCRecordUpdate_Test, Dem_EnableDTCRecordUpdate_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dem_Test_ResetInitState();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_EnableDTCRecordUpdate(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_UNINIT);
}

TEST_F(Dem_EnableDTCRecordUpdate_Test, Dem_EnableDTCRecordUpdate_NG_InvalidClientId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_EnableDTCRecordUpdate(DEM_CLIENT_COUNT);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_WRONG_CONFIGURATION);
}

TEST_F(Dem_EnableDTCRecordUpdate_Test, Dem_EnableDTCRecordUpdate_NG_NoDtcSelection)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // Dem_SelectDTC() を呼ばない

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_EnableDTCRecordUpdate(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);   // 選択が無い [SWS_Dem_00234]
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_WRONG_CONDITION);
}

TEST_F(Dem_EnableDTCRecordUpdate_Test, Dem_EnableDTCRecordUpdate_NG_NothingIsLocked)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Select(0U);   // Dem_DisableDTCRecordUpdate() を呼ばない

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_EnableDTCRecordUpdate(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_WRONG_CONDITION);
}

TEST_F(Dem_EnableDTCRecordUpdate_Test, Dem_EnableDTCRecordUpdate_NG_SecondCallHasNothingToRelease)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Select(0U);
    ASSERT_EQ(Dem_DisableDTCRecordUpdate(0U), E_OK);
    ASSERT_EQ(Dem_EnableDTCRecordUpdate(0U), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_EnableDTCRecordUpdate(0U);   // 続けて、もう一度

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
}

TEST_F(Dem_EnableDTCRecordUpdate_Test, Dem_EnableDTCRecordUpdate_OK_ReleasesTheLock)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Select(0U);
    ASSERT_EQ(Dem_DisableDTCRecordUpdate(0U), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_EnableDTCRecordUpdate(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(Dem_DisableDTCRecordUpdate(0U), E_OK);   // 別の DTC を、また止められる
}

TEST_F(Dem_EnableDTCRecordUpdate_Test, Dem_EnableDTCRecordUpdate_OK_ResumesTheRecordUpdate)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FakeRte_EngineSpeed = 0x1111U;
    Confirm(0U);
    Select(0U);
    ASSERT_EQ(Dem_DisableDTCRecordUpdate(0U), E_OK);
    ASSERT_EQ(Dem_EnableDTCRecordUpdate(0U), E_OK);
    FakeRte_EngineSpeed = 0x2222U;
    Recover(0U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Confirm(0U);   // 戻したあとの、確定

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ReadSpeed(), 0x2222);   // FreezeFrame が、取り直される
    EXPECT_EQ(ReadOccurrence(), 2);     // 故障確定回数が、加算される
}

}  // namespace
