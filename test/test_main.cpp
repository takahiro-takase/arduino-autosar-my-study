/**
 * \file    test_main.cpp
 * \brief   test/test_native/ 配下の全テストファイル共通の GoogleTest エントリポイント
 * \details 1 テストバイナリにつき main() は 1 つしか定義できないため、
 *          ここに集約する。新しいテストファイル（Bsw_XXX_test.cpp 等）を
 *          追加する際、そちらには int main() を書かないこと。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Fake_Fee_Hw.h"
}

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

namespace
{

/**
 * \brief   毎テストケース開始前に `FakeFeeHw_Reset()` を自動的に呼ぶリスナー。
 *
 * \details `Fake_Fee_Hw.c`（NvM/MemIf/Fee の実体リンクに伴い導入。
 *          `Fake_Fee_Hw.h` 冒頭コメント参照）は旧 `Fake_NvM.c` と異なり
 *          RAM バッファに内容を実際に保持するため、何もしなければ
 *          `Dem_Init()` を呼ぶ既存の全テストファイル（`test/Bsw/DcmStack/`
 *          等、約36ファイル）が暗黙に前提とする「毎回必ず初回起動」という
 *          挙動が2回目以降のテストで崩れてしまう。個々のテストファイルの
 *          `SetUp()` を変更する代わりに、ここで全テスト共通に自動リセット
 *          することで既存ファイルを一切変更せずに済ませる。
 */
class FeeHwAutoResetListener : public ::testing::EmptyTestEventListener
{
    void OnTestStart(const ::testing::TestInfo&) override
    {
        FakeFeeHw_Reset();
    }
};

}  // namespace

/* ======================================================================
 * Functions
 * ====================================================================== */

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::UnitTest::GetInstance()->listeners().Append(new FeeHwAutoResetListener());
    return RUN_ALL_TESTS();
}
