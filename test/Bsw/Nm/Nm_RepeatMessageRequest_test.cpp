/**
 * \file    Nm_RepeatMessageRequest_test.cpp
 * \brief   Nm_RepeatMessageRequest() の単体テスト
 * \details CanNm の Bus-Sleep Mode からの Repeat Message Request は、CanNm が受け付けない（[SWS_CanNm_00137]）。Nm はその
 *          E_NOT_OK をそのまま返す（委譲）。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Nm.h"
#include "Nm_Cfg.h"
#include "CanNm.h"
#include "CanNm_Cfg.h"
#include "Fake_Det_Hw.h"
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

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* Nm_RepeatMessageRequest() の単体テスト用フィクスチャ。
 * SetUp(): DET の記録を初期化し、CanNm_Init(NULL)・Nm_Init(NULL) を呼ぶ（Init 自体の記録は消す）。
 * TearDown(): Nm・CanNm を未初期化へ戻す。 */
class Nm_RepeatMessageRequest_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;
        FakeDetHw_Reset();
        CanNm_Init(NULL);
        Nm_Init(NULL);
        FakeDetHw_Reset();
        FakeDetHw_LogSuppressed = 0U;
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Nm_Test_ResetInitState();
        CanNm_DeInit();
    }
};

/* Nm の単体テスト用フィクスチャ。
 * SetUp(): Nm_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。範囲外のチャネルを表す kInvalidChannel も用意する。
 * TearDown(): 未初期化の状態へ戻す。 */
class Nm_RepeatMessageRequest_Base_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        Nm_Init(NULL);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Nm_Test_ResetInitState();
    }

    static const NetworkHandleType kInvalidChannel = NM_MAIN_NETWORK_HANDLE + 1U;
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Nm_RepeatMessageRequest_Base_Test, Nm_RepeatMessageRequest_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Nm_Test_ResetInitState();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Nm_RepeatMessageRequest(NM_MAIN_NETWORK_HANDLE);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, NM_E_UNINIT);
}

TEST_F(Nm_RepeatMessageRequest_Base_Test, Nm_RepeatMessageRequest_NG_InvalidChannel)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Nm_RepeatMessageRequest(kInvalidChannel);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, NM_E_INVALID_CHANNEL);
}

TEST_F(Nm_RepeatMessageRequest_Test, Nm_RepeatMessageRequest_OK_ReturnsCanNmResult)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み。CanNm は Bus-Sleep Mode）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Nm_RepeatMessageRequest(NM_MAIN_NETWORK_HANDLE);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
}

}  // namespace
