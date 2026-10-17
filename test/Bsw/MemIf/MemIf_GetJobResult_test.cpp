/**
 * \file    MemIf_GetJobResult_test.cpp
 * \brief   MemIf_GetJobResult() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 MemIf_test.cpp）。
 *
 *          MemIf.c は `MemIf_CheckDevice()`（1箇所の `Det_ReportError()`
 *          呼び出し）を Read/Write/Cancel/GetStatus/
 *          GetJobResult の6エントリポイントから呼ぶ薄い層のため、ソース上の
 *          `Det_ReportError()` 呼び出し箇所は2箇所（本関数＋GetVersionInfo）
 *          のみだが、6エントリポイントそれぞれで報告される ApiId が正しい値に
 *          なっていることを個別に検証する（`Fake_Det_Hw.h`の
 *          `FakeDetHw_LastErrorId`/`FakeDetHw_LastApiId`）。
 *
 *          分割前の共通の説明は、旧 MemIf_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "MemIf.h"
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

const MemIf_DeviceType kInvalidDevice = MEMIF_DEVICE_0 + 1U;

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* MemIf の単体テスト用フィクスチャ。
 * SetUp(): MemIf_Init() を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。 */
class MemIf_GetJobResult_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        MemIf_Init();
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(MemIf_GetJobResult_Test, MemIf_GetJobResult_NG_InvalidDevice)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    MemIf_JobResultType result = MemIf_GetJobResult(kInvalidDevice);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(result, MEMIF_JOB_FAILED);
    EXPECT_EQ(FakeDetHw_LastErrorId, MEMIF_E_PARAM_DEVICE);
    EXPECT_EQ(FakeDetHw_LastApiId, MEMIF_API_ID_GET_JOB_RESULT);
}

}  // namespace
