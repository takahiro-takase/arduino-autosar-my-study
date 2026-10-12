/**
 * \file    Bsw_IoHwAb_ButtonStuck_test.cpp
 * \brief   IoHwAb_MainFunction() のボタン固着の検出の単体テスト
 * \details 押下が確定したまま 500 サンプル（10ms × 500 = 5 秒）続くと、BUTTON_STUCK を FAILED で報告する。
 *          解放が確定すると、PASSED を報告する。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "IoHwAb.h"
#include "Dio.h"
#include "Dio_Cfg.h"
#include "Adc.h"
#include "Adc_Cfg.h"
#include "Dem.h"
#include "Dem_Cfg.h"
#include "Fake_Dio_Hw.h"
#include "Fake_Adc_Hw.h"
#include "Fake_Det_Hw.h"
#include "Wrap_Dem.h"
}

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

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

/* IoHwAb_MainFunction() のボタン固着の検出 の単体テスト用フィクスチャ。
 * SetUp(): 模擬 Dio_Hw・Adc_Hw・DET・Dem の wrap を初期化し、Dem_Init(NULL)・Adc_Init(NULL)・IoHwAb_Init(NULL) を
 * EcuM_Init() と同じ順序で呼ぶ（Init 自体の記録は消す）。 */
class Bsw_IoHwAb_ButtonStuck_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDioHw_Reset();
        FakeAdcHw_Reset();
        FakeDetHw_Reset();
        WrapDem_Reset();
        Dem_Init(NULL);
        Adc_Init(NULL);
        IoHwAb_Init(NULL);
        WrapDem_Reset();
        FakeDetHw_Reset();
    }

    /* ボタンの生レベルを設定する（押下 = DIO_LOW、IoHwAb は論理反転して 押下=1 にする）。 */
    static void SetButtonPressed(bool pressed)
    {
        Dio_WriteChannel(DIO_CHANNEL_BUTTON, pressed ? DIO_LOW : DIO_HIGH);
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Bsw_IoHwAb_ButtonStuck_Test, IoHwAb_MainFunction_OK_ButtonStuckReportedAfterLongPressAndClearedOnRelease)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    SetButtonPressed(true);
    for (uint16 i = 0U; i < 3U; i++)
    {
        IoHwAb_MainFunction();   // デバウンス（4 回目の呼び出しで押下が確定する）
    }
    WrapDem_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    // 押下が確定してから 500 サンプル（IOHWAB_BUTTON_STUCK_COUNT）で固着と判定する。
    // 確定する 4 回目の呼び出しも 1 サンプルに数えるため、残りは 499 回の呼び出しで 500 サンプルに達する。
    for (uint16 i = 0U; i < 498U; i++)
    {
        IoHwAb_MainFunction();
    }
    const Dem_EventStatusType beforeStuck = LastStatusOfEvent_Dem_SetEventStatus[DemConf_DemEventParameter_BUTTON_STUCK];
    IoHwAb_MainFunction();   // 499 回目
    IoHwAb_MainFunction();   // 500 サンプル目
    const Dem_EventStatusType atStuck = LastStatusOfEvent_Dem_SetEventStatus[DemConf_DemEventParameter_BUTTON_STUCK];

    SetButtonPressed(false);
    for (uint8 i = 0U; i < 4U; i++)
    {
        IoHwAb_MainFunction();   // 解放が確定する
    }
    const Dem_EventStatusType afterRelease = LastStatusOfEvent_Dem_SetEventStatus[DemConf_DemEventParameter_BUTTON_STUCK];

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(beforeStuck, 0xFFU);                          // 500 サンプルの手前までは報告しない
    EXPECT_EQ(atStuck, DEM_EVENT_STATUS_FAILED);            // 5 秒の固着を FAILED で報告する
    EXPECT_EQ(afterRelease, DEM_EVENT_STATUS_PASSED);       // 解放で PASSED を報告する
}

}  // namespace
