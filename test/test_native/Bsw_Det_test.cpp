/**
 * \file    Bsw_Det_test.cpp
 * \brief   Det.c（src/Bsw/Det/Det.c）の Det_GetVersionInfo() 単体テスト
 * \details AUTOSAR SWS_Det_00011 が規定する Det_GetVersionInfo() の
 *          NULL ポインタチェックと ModuleId 返却を検証する。Det.c 自体は
 *          実物をリンクし、実 HW 依存の Det_Hw.cpp のみを Hal_Det_Hw_fake.c に
 *          差し替える（他モジュールのテストと同じ構成）。
 *
 *          GoogleTest の main() は test_main.cpp に集約しているため、
 *          本ファイルでは定義しない。
 */
#include <gtest/gtest.h>

extern "C" {
#include "Det.h"
#include "Hal_Det_Hw_fake.h"
}

namespace
{

class DetTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_Reset();
    }
};

TEST_F(DetTest, GetVersionInfoRejectsNullPointer)
{
    Det_GetVersionInfo(NULL);

    EXPECT_EQ(FakeDetHw_LastErrorId, DET_E_PARAM_POINTER);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);
}

TEST_F(DetTest, GetVersionInfoFillsExpectedModuleId)
{
    Std_VersionInfoType info;

    Det_GetVersionInfo(&info);

    EXPECT_EQ(info.moduleID, DET_MODULE_ID);
}

}  // namespace
