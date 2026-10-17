/**
 * \file    Com_GetStatus_test.cpp
 * \brief   Com_GetStatus() の単体テスト
 * \details 初期化前は COM_UNINIT、Com_Init() の後は COM_INIT を返し、Com_DeInit() の後は COM_UNINIT へ戻る。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Com.h"
#include "Fake_Det_Hw.h"
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

const Com_ConfigType kEmptyConfig = {
    NULL, 0U,
    NULL, 0U,
    NULL, 0U,
    NULL, 0U
};

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* Com_GetStatus() の単体テスト用フィクスチャ。
 * SetUp(): Com_DeInit() で未初期化の状態から始める。
 * TearDown(): Com_DeInit() で未初期化へ戻す。 */
class Com_GetStatus_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init()/DeInit() のログはノイズになるため抑制
        Com_DeInit();                  // 他のテストファイルが初期化したまま残していても、未初期化から始める
        FakeDetHw_Reset();
        FakeDetHw_LogSuppressed = 0U;
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Com_DeInit();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Com_GetStatus_Test, Com_GetStatus_OK_UninitializedBeforeInit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で未初期化にしてある）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Com_StatusType status = Com_GetStatus();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(status, COM_UNINIT);
}

TEST_F(Com_GetStatus_Test, Com_GetStatus_OK_InitializedAfterInitAndUninitializedAfterDeInit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で未初期化にしてある）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Com_Init(&kEmptyConfig);
    const Com_StatusType afterInit = Com_GetStatus();
    Com_DeInit();
    const Com_StatusType afterDeInit = Com_GetStatus();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(afterInit, COM_INIT);
    EXPECT_EQ(afterDeInit, COM_UNINIT);
}

}  // namespace
