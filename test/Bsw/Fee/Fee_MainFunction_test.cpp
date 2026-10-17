/**
 * \file    Fee_MainFunction_test.cpp
 * \brief   Fee_MainFunction() の単体テスト
 * \details 書き込みジョブがあれば、呼ぶたびに 1 バイトずつ書き、全バイトを書き終えたらジョブを完了（MEMIF_JOB_OK）にする。
 *          未初期化、またはジョブが無いときは、何もしない。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Fee.h"
#include "Fee_Hw.h"
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

/* Fee_MainFunction() の単体テスト用フィクスチャ。
 * SetUp(): Fee_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。ReadByte() は、模擬 EEPROM の 1 バイトを読む補助関数。
 * TearDown(): 未初期化の状態へ戻す。 */
class Fee_MainFunction_Test : public ::testing::Test
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

    /* 模擬 EEPROM の 1 バイトを読む。 */
    static uint8 ReadByte(uint16 address)
    {
        uint8 value = 0U;
        Fee_Hw_ReadBlock(&value, address, 1U);
        return value;
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Fee_MainFunction_Test, Fee_MainFunction_NG_UninitDoesNotWrite)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    const uint16 address = 0x20U;
    const uint8 data[1] = { 0xA5U };
    ASSERT_EQ(Fee_Write(address, data, 1U), E_OK);   // ジョブを積んでおく（書き込みは MainFunction が行う）
    Fee_Test_ResetInitState();                       // 未初期化にする

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Fee_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ReadByte(address), 0xFFU);   // 未初期化では書かない（模擬 EEPROM は消去済みの値のまま）
}

TEST_F(Fee_MainFunction_Test, Fee_MainFunction_NG_NoActiveJobDoesNothing)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み。書き込みジョブは積んでいない）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Fee_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Fee_GetStatus(), MEMIF_IDLE);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(Fee_MainFunction_Test, Fee_MainFunction_OK_WritesOneBytePerCallAndCompletesJob)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    const uint16 address = 0x20U;
    const uint8 data[2] = { 0x11U, 0x22U };
    ASSERT_EQ(Fee_Write(address, data, 2U), E_OK);
    ASSERT_EQ(Fee_GetStatus(), MEMIF_BUSY);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Fee_MainFunction();
    const uint8 afterFirst0 = ReadByte(address);
    const uint8 afterFirst1 = ReadByte(address + 1U);
    const MemIf_StatusType statusAfterFirst = Fee_GetStatus();

    Fee_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(afterFirst0, 0x11U);               // 1 回目で 1 バイト目
    EXPECT_EQ(afterFirst1, 0xFFU);               // 2 バイト目はまだ書かない
    EXPECT_EQ(statusAfterFirst, MEMIF_BUSY);     // まだ途中
    EXPECT_EQ(ReadByte(address + 1U), 0x22U);    // 2 回目で 2 バイト目
    EXPECT_EQ(Fee_GetStatus(), MEMIF_IDLE);      // 全バイトを書き終えて完了
    EXPECT_EQ(Fee_GetJobResult(), MEMIF_JOB_OK);
}

}  // namespace
