/**
 * \file    Bsw_KeyM_test.cpp
 * \brief   KeyM.c（src/Bsw/KeyM/KeyM.c）の単体テスト（NG系）
 * \details KeyM.c 全公開APIの `Det_ReportError()` 呼び出し箇所（NG ケースの
 *          み）を1ファイルにまとめる。各ケースで、報告される ErrorId が
 *          仕様どおり正しい値になっていることを `Fake_Det_Hw.h`
 *          （`FakeDetHw_LastErrorId`）で検証する。
 *
 *          KeyM には `KeyM_Deinit()` が存在するため、Mcu/PduR/CanTp と異なり
 *          「未初期化状態」の検証にテスト専用のリセット関数は不要（各テストが
 *          明示的に `KeyM_Deinit()` を呼んでから検証する）。`KeyM_Deinit()`
 *          自体は内部で `Csm_KeyElementSet()` を呼ぶが、失敗しても警告ログの
 *          みで処理を継続する設計（KeyM.c 参照）のため、Csm/CryIf/Crypto の
 *          初期化状態に依存せず安全に呼べる。
 *
 *          `KeyM_Update()`/`KeyM_Finalize()` の「セッション未開始」
 *          「鍵名不明」「Csm_KeyElementSet 失敗」等は専用の DET コードが
 *          定義されていない（KeyM.c 内のコメント参照）ため、本ファイルの
 *          対象外（Det_ReportError() を呼ばない）。
 *
 *          GoogleTest の main() は test_main.cpp に集約しているため、
 *          本ファイルでは定義しない。
 */
#include <gtest/gtest.h>

extern "C" {
#include "KeyM.h"
#include "Fake_Det_Hw.h"
}

namespace
{

class Bsw_KeyM_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        KeyM_Init(NULL);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        KeyM_Deinit();
    }
};

// ------------------------------------------------------------
// KeyM_Deinit()
// ------------------------------------------------------------

TEST_F(Bsw_KeyM_Test, KeyM_Deinit_NG_Uninit)
{
    KeyM_Deinit();  // 1回目: SetUp() の Init を正常に解除する
    FakeDetHw_Reset();

    KeyM_Deinit();  // 2回目: 既に未初期化のため NG

    EXPECT_EQ(FakeDetHw_LastErrorId, KEYM_E_UNINIT);
}

// ------------------------------------------------------------
// KeyM_GetVersionInfo()
// ------------------------------------------------------------

TEST_F(Bsw_KeyM_Test, KeyM_GetVersionInfo_NG_Uninit)
{
    KeyM_Deinit();

    KeyM_GetVersionInfo(NULL);

    EXPECT_EQ(FakeDetHw_LastErrorId, KEYM_E_UNINIT);
}

TEST_F(Bsw_KeyM_Test, KeyM_GetVersionInfo_NG_NullPointer)
{
    KeyM_GetVersionInfo(NULL);

    EXPECT_EQ(FakeDetHw_LastErrorId, KEYM_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// KeyM_Start()
// ------------------------------------------------------------

TEST_F(Bsw_KeyM_Test, KeyM_Start_NG_Uninit)
{
    KeyM_Deinit();

    Std_ReturnType ret = KeyM_Start(KEYM_START_OEM_PRODUCTIONMODE, NULL, 0U, NULL, NULL);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, KEYM_E_UNINIT);
}

// ------------------------------------------------------------
// KeyM_Update()
// ------------------------------------------------------------

TEST_F(Bsw_KeyM_Test, KeyM_Update_NG_Uninit)
{
    KeyM_Deinit();
    uint8 keyName = 0x01U;
    uint8 reqData = 0x00U;

    Std_ReturnType ret = KeyM_Update(&keyName, 1U, &reqData, 1U, NULL, 0U);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, KEYM_E_UNINIT);
}

TEST_F(Bsw_KeyM_Test, KeyM_Update_NG_NullKeyNamePtr)
{
    uint8 reqData = 0x00U;

    Std_ReturnType ret = KeyM_Update(NULL, 1U, &reqData, 1U, NULL, 0U);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, KEYM_E_PARAM_POINTER);
}

TEST_F(Bsw_KeyM_Test, KeyM_Update_NG_NullRequestDataPtr)
{
    uint8 keyName = 0x01U;

    Std_ReturnType ret = KeyM_Update(&keyName, 1U, NULL, 1U, NULL, 0U);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, KEYM_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// KeyM_Finalize()
// ------------------------------------------------------------

TEST_F(Bsw_KeyM_Test, KeyM_Finalize_NG_Uninit)
{
    KeyM_Deinit();

    Std_ReturnType ret = KeyM_Finalize(NULL, 0U, NULL, 0U);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, KEYM_E_UNINIT);
}

}  // namespace
