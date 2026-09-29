/**
 * \file    Bsw_Adc_test.cpp
 * \brief   Adc.c（src/Bsw/Adc/Adc.c）の単体テスト（NG系）
 * \details Adc.c 全公開APIの `Det_ReportError()` 呼び出し箇所（NG ケースの
 *          み）を1ファイルにまとめる。各ケースで、報告される ErrorId が
 *          仕様どおり正しい値になっていることを `Fake_Det_Hw.h`
 *          （`FakeDetHw_LastErrorId`）で検証する。
 *
 *          Adc は初期化状態を持たない（`Adc_Init()` はログ出力のみの薄い
 *          実装、Adc.c 参照）ため、未初期化チェックの検証対象は無い。
 *          実 HW（analogRead()）への依存は `stub/Hal/Fake_Adc_Hw.c` で満たす。
 *
 *          GoogleTest の main() は test_main.cpp に集約しているため、
 *          本ファイルでは定義しない。
 */
#include <gtest/gtest.h>

extern "C" {
#include "Adc.h"
#include "Fake_Det_Hw.h"
}

namespace
{

class Bsw_Adc_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_Reset();
    }
};

// ------------------------------------------------------------
// Adc_ReadChannel()
// ------------------------------------------------------------

TEST_F(Bsw_Adc_Test, Adc_ReadChannel_NG_NullPointer)
{
    Std_ReturnType ret = Adc_ReadChannel(0U, NULL);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, ADC_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// Adc_GetVersionInfo()
// ------------------------------------------------------------

TEST_F(Bsw_Adc_Test, Adc_GetVersionInfo_NG_NullPointer)
{
    Adc_GetVersionInfo(NULL);

    EXPECT_EQ(FakeDetHw_LastErrorId, ADC_E_PARAM_POINTER);
}

}  // namespace
