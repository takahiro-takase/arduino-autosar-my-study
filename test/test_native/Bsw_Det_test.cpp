/**
 * \file    Bsw_Det_test.cpp
 * \brief   Det.c（src/Bsw/Det/Det.c）の単体テスト
 * \details AUTOSAR SWS_Det_00011 が規定する Det_GetVersionInfo() の
 *          NULL ポインタチェックと ModuleId 返却、および SWS_Det_00008/00010
 *          が規定する Det_Init()/Det_Start()（DET エラーを報告しないこと）を
 *          検証する。Det.c 自体は実物をリンクし、実 HW 依存の Det_Hw.cpp のみを
 *          Hal_Det_Hw_fake.c に差し替える（他モジュールのテストと同じ構成）。
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

// ------------------------------------------------------------
// Det_Init()/Det_Start()（[SWS_Det_00008]/[SWS_Det_00010] 準拠で新設。
// Det 自身は初期化を要する内部変数を持たないため no-op だが、
// 呼び出しても DET エラーを報告しないことだけを確認する）
// ------------------------------------------------------------

TEST_F(DetTest, InitDoesNotReportError)
{
    Det_Init(NULL);

    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(DetTest, StartDoesNotReportError)
{
    Det_Start();

    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

}  // namespace
