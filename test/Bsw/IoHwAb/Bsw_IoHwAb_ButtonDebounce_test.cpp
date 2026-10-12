/**
 * \file    Bsw_IoHwAb_ButtonDebounce_test.cpp
 * \brief   IoHwAb_MainFunction() のボタンのデバウンスの単体テスト
 * \details ボタンの生レベルが 4 サンプル（IOHWAB_BUTTON_DEBOUNCE_COUNT）連続して変化して初めて、確定値（IoHwAb_Button_GetLevel()）が変わる。
 *          押下（DIO_LOW → 1）と解放のどちらも同じ。
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

/* IoHwAb_MainFunction() のボタンのデバウンス の単体テスト用フィクスチャ。
 * SetUp(): 模擬 Dio_Hw・Adc_Hw・DET・Dem の wrap を初期化し、Dem_Init(NULL)・Adc_Init(NULL)・IoHwAb_Init(NULL) を
 * EcuM_Init() と同じ順序で呼ぶ（Init 自体の記録は消す）。 */
class Bsw_IoHwAb_ButtonDebounce_Test : public ::testing::Test
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

TEST_F(Bsw_IoHwAb_ButtonDebounce_Test, IoHwAb_MainFunction_OK_ButtonLevelConfirmedAfterDebounceCount)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 level = 0xFFU;
    SetButtonPressed(true);   // 押下（生レベルが変わる）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    uint8 levelAfter3[1] = { 0xFFU };
    for (uint8 i = 0U; i < 3U; i++)
    {
        IoHwAb_MainFunction();
    }
    (void)IoHwAb_Button_GetLevel(&levelAfter3[0]);
    IoHwAb_MainFunction();   // 4 回目（連続一致サンプル数 4 に達する）
    (void)IoHwAb_Button_GetLevel(&level);

    SetButtonPressed(false);   // 解放
    uint8 levelAfterRelease3 = 0xFFU;
    for (uint8 i = 0U; i < 3U; i++)
    {
        IoHwAb_MainFunction();
    }
    (void)IoHwAb_Button_GetLevel(&levelAfterRelease3);
    IoHwAb_MainFunction();
    uint8 levelAfterRelease4 = 0xFFU;
    (void)IoHwAb_Button_GetLevel(&levelAfterRelease4);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(levelAfter3[0], 0U);        // 3 回では、まだ確定しない
    EXPECT_EQ(level, 1U);                 // 4 回で確定（押下 = 1）
    EXPECT_EQ(levelAfterRelease3, 1U);    // 解放も 3 回では確定しない
    EXPECT_EQ(levelAfterRelease4, 0U);    // 4 回で確定（解放 = 0）
}

}  // namespace
