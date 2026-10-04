/**
 * \file    Bsw_Fee_test.cpp
 * \brief   Fee.c（src/Bsw/Fee/Fee.c）の単体テスト（NG系）
 * \details Fee.c 全公開APIの `Det_ReportError()` 呼び出し箇所（NG ケースのみ）を
 *          1ファイルにまとめる。各ケースで、報告される ErrorId が仕様どおり
 *          正しい値になっていることを `Fake_Det_Hw.h`（`FakeDetHw_LastErrorId`）
 *          で検証する。
 *
 *          Fee には `Fee_DeInit()` に相当する API が無く、`Fee_Initialized` は
 *          native_chain_tests バイナリ全体で共有される static のため、
 *          `FEE_E_UNINIT` の検証には `Fee_Test_ResetInitState()`
 *          （`FEE_UNIT_TEST` ビルドのみに存在するテスト専用関数、
 *          `Mcu_Test_ResetInitState()` と同じ設計）を使う。
 *
 *          実 HW（EEPROM.h）への依存は `stub/Hal/Fake_Fee_Hw.c`（RAM バッファ）
 *          で満たす（2026-09、NvM/MemIf/Fee 自身の Det 検証のため実体リンクへ
 *          切り替えた際に新設。Fake_Fee_Hw.h 冒頭コメント参照）。
 *
 *          GoogleTest の main() は test_main.cpp に集約しているため、
 *          本ファイルでは定義しない。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Fee.h"
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

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

class Bsw_Fee_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        Fee_Init(NULL);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Fee_Test_ResetInitState();
    }

    /* Fee_Job.Active=1（MEMIF_BUSY）を作るヘルパー。 */
    void ArrangeJobActive()
    {
        static const uint8 data[1] = { 0x00U };
        ASSERT_EQ(Fee_Write(0U, data, 1U), E_OK);
        ASSERT_EQ(Fee_GetStatus(), MEMIF_BUSY);
    }
};

// ------------------------------------------------------------
// Fee_GetVersionInfo()
// ------------------------------------------------------------

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Bsw_Fee_Test, Fee_GetVersionInfo_NG_NullPointer)
{
    Fee_GetVersionInfo(NULL);

    EXPECT_EQ(FakeDetHw_LastErrorId, FEE_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// Fee_SetMode()
// ------------------------------------------------------------

TEST_F(Bsw_Fee_Test, Fee_SetMode_NG_Uninit)
{
    Fee_Test_ResetInitState();

    Fee_SetMode(MEMIF_MODE_FAST);

    EXPECT_EQ(FakeDetHw_LastErrorId, FEE_E_UNINIT);
}

TEST_F(Bsw_Fee_Test, Fee_SetMode_NG_Busy)
{
    ArrangeJobActive();

    Fee_SetMode(MEMIF_MODE_FAST);

    EXPECT_EQ(FakeDetHw_LastErrorId, FEE_E_BUSY);
}

// ------------------------------------------------------------
// Fee_Read()
// ------------------------------------------------------------

TEST_F(Bsw_Fee_Test, Fee_Read_NG_Uninit)
{
    Fee_Test_ResetInitState();
    uint8 buf[1];

    Std_ReturnType ret = Fee_Read(0U, buf, 1U);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, FEE_E_UNINIT);
}

TEST_F(Bsw_Fee_Test, Fee_Read_NG_NullPointer)
{
    Std_ReturnType ret = Fee_Read(0U, NULL, 1U);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, FEE_E_PARAM_POINTER);
}

TEST_F(Bsw_Fee_Test, Fee_Read_NG_InvalidBlockLen)
{
    uint8 buf[1];

    Std_ReturnType ret = Fee_Read(0U, buf, 0U);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, FEE_E_INVALID_BLOCK_LEN);
}

TEST_F(Bsw_Fee_Test, Fee_Read_NG_Busy)
{
    ArrangeJobActive();
    uint8 buf[1];

    Std_ReturnType ret = Fee_Read(0U, buf, 1U);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, FEE_E_BUSY);
}

// ------------------------------------------------------------
// Fee_Write()
// ------------------------------------------------------------

TEST_F(Bsw_Fee_Test, Fee_Write_NG_Uninit)
{
    Fee_Test_ResetInitState();
    const uint8 data[1] = { 0U };

    Std_ReturnType ret = Fee_Write(0U, data, 1U);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, FEE_E_UNINIT);
}

TEST_F(Bsw_Fee_Test, Fee_Write_NG_NullPointer)
{
    Std_ReturnType ret = Fee_Write(0U, NULL, 1U);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, FEE_E_PARAM_POINTER);
}

TEST_F(Bsw_Fee_Test, Fee_Write_NG_InvalidBlockLen)
{
    const uint8 data[1] = { 0U };

    Std_ReturnType ret = Fee_Write(0U, data, 0U);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, FEE_E_INVALID_BLOCK_LEN);
}

TEST_F(Bsw_Fee_Test, Fee_Write_NG_Busy)
{
    ArrangeJobActive();
    const uint8 data[1] = { 0U };

    Std_ReturnType ret = Fee_Write(0U, data, 1U);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, FEE_E_BUSY);
}

// ------------------------------------------------------------
// Fee_WriteImmediate()
// ------------------------------------------------------------

TEST_F(Bsw_Fee_Test, Fee_WriteImmediate_NG_Uninit)
{
    Fee_Test_ResetInitState();
    const uint8 data[1] = { 0U };

    Std_ReturnType ret = Fee_WriteImmediate(0U, data, 1U);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, FEE_E_UNINIT);
}

TEST_F(Bsw_Fee_Test, Fee_WriteImmediate_NG_NullPointer)
{
    Std_ReturnType ret = Fee_WriteImmediate(0U, NULL, 1U);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, FEE_E_PARAM_POINTER);
}

TEST_F(Bsw_Fee_Test, Fee_WriteImmediate_NG_InvalidBlockLen)
{
    const uint8 data[1] = { 0U };

    Std_ReturnType ret = Fee_WriteImmediate(0U, data, 0U);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, FEE_E_INVALID_BLOCK_LEN);
}

TEST_F(Bsw_Fee_Test, Fee_WriteImmediate_NG_Busy)
{
    ArrangeJobActive();
    const uint8 data[1] = { 0U };

    Std_ReturnType ret = Fee_WriteImmediate(0U, data, 1U);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, FEE_E_BUSY);
}

// ------------------------------------------------------------
// Fee_Cancel()
// ------------------------------------------------------------

TEST_F(Bsw_Fee_Test, Fee_Cancel_NG_Uninit)
{
    Fee_Test_ResetInitState();

    Fee_Cancel();

    EXPECT_EQ(FakeDetHw_LastErrorId, FEE_E_UNINIT);
}

TEST_F(Bsw_Fee_Test, Fee_Cancel_NG_InvalidCancel)
{
    // ジョブが無い(MEMIF_IDLE)状態でのキャンセル要求。
    ASSERT_EQ(Fee_GetStatus(), MEMIF_IDLE);

    Fee_Cancel();

    EXPECT_EQ(FakeDetHw_LastErrorId, FEE_E_INVALID_CANCEL);
}

// ------------------------------------------------------------
// Fee_GetJobResult()
// ------------------------------------------------------------

TEST_F(Bsw_Fee_Test, Fee_GetJobResult_NG_Uninit)
{
    Fee_Test_ResetInitState();

    MemIf_JobResultType result = Fee_GetJobResult();

    EXPECT_EQ(result, MEMIF_JOB_FAILED);
    EXPECT_EQ(FakeDetHw_LastErrorId, FEE_E_UNINIT);
}

}  // namespace
