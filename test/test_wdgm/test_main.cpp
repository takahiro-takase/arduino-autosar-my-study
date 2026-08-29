/**
 * \file    test_main.cpp
 * \brief   test/test_wdgm/ 配下の全テストファイル共通の GoogleTest エントリポイント
 * \details 1 テストバイナリにつき main() は 1 つしか定義できないため、
 *          ここに集約する。新しいテストファイル（Bsw_XXX_test.cpp 等）を
 *          追加する際、そちらには int main() を書かないこと。
 */
#include <gtest/gtest.h>

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
