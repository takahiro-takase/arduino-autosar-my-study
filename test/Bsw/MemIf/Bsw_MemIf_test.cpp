/**
 * \file    Bsw_MemIf_test.cpp
 * \brief   MemIf.c（src/Bsw/MemIf/MemIf.c）の単体テスト（NG系）
 * \details MemIf.c は `MemIf_CheckDevice()`（1箇所の `Det_ReportError()`
 *          呼び出し）を Read/Write/WriteImmediate/Cancel/GetStatus/
 *          GetJobResult の6エントリポイントから呼ぶ薄い層のため、ソース上の
 *          `Det_ReportError()` 呼び出し箇所は2箇所（本関数＋GetVersionInfo）
 *          のみだが、6エントリポイントそれぞれで報告される ApiId が正しい値に
 *          なっていることを個別に検証する（`Fake_Det_Hw.h`の
 *          `FakeDetHw_LastErrorId`/`FakeDetHw_LastApiId`）。
 *
 *          MemIf 自体は初期化状態を持たない（`MemIf_Init()` はただ
 *          `Fee_Init()` を呼ぶだけの薄いラッパー、MemIf.c 参照）ため、
 *          未初期化チェックの検証対象は無い。
 *
 *          GoogleTest の main() は test_main.cpp に集約しているため、
 *          本ファイルでは定義しない。
 */
#include <gtest/gtest.h>

extern "C" {
#include "MemIf.h"
#include "Fake_Det_Hw.h"
}

namespace
{

const MemIf_DeviceType kInvalidDevice = MEMIF_DEVICE_0 + 1U;

class Bsw_MemIf_Test : public ::testing::Test
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

// ------------------------------------------------------------
// MemIf_GetVersionInfo()
// ------------------------------------------------------------

TEST_F(Bsw_MemIf_Test, MemIf_GetVersionInfo_NG_NullPointer)
{
    MemIf_GetVersionInfo(NULL);

    EXPECT_EQ(FakeDetHw_LastErrorId, MEMIF_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// MemIf_Read()
// ------------------------------------------------------------

TEST_F(Bsw_MemIf_Test, MemIf_Read_NG_InvalidDevice)
{
    uint8 buf[1];

    Std_ReturnType ret = MemIf_Read(kInvalidDevice, 0U, buf, 1U);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, MEMIF_E_PARAM_DEVICE);
    EXPECT_EQ(FakeDetHw_LastApiId, MEMIF_API_ID_READ);
}

// ------------------------------------------------------------
// MemIf_Write()
// ------------------------------------------------------------

TEST_F(Bsw_MemIf_Test, MemIf_Write_NG_InvalidDevice)
{
    const uint8 data[1] = { 0U };

    Std_ReturnType ret = MemIf_Write(kInvalidDevice, 0U, data, 1U);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, MEMIF_E_PARAM_DEVICE);
    EXPECT_EQ(FakeDetHw_LastApiId, MEMIF_API_ID_WRITE);
}

// ------------------------------------------------------------
// MemIf_WriteImmediate()
// ------------------------------------------------------------

TEST_F(Bsw_MemIf_Test, MemIf_WriteImmediate_NG_InvalidDevice)
{
    const uint8 data[1] = { 0U };

    Std_ReturnType ret = MemIf_WriteImmediate(kInvalidDevice, 0U, data, 1U);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, MEMIF_E_PARAM_DEVICE);
    EXPECT_EQ(FakeDetHw_LastApiId, MEMIF_API_ID_WRITE_IMMEDIATE);
}

// ------------------------------------------------------------
// MemIf_Cancel()
// ------------------------------------------------------------

TEST_F(Bsw_MemIf_Test, MemIf_Cancel_NG_InvalidDevice)
{
    MemIf_Cancel(kInvalidDevice);

    EXPECT_EQ(FakeDetHw_LastErrorId, MEMIF_E_PARAM_DEVICE);
    EXPECT_EQ(FakeDetHw_LastApiId, MEMIF_API_ID_CANCEL);
}

// ------------------------------------------------------------
// MemIf_GetStatus()
// ------------------------------------------------------------

TEST_F(Bsw_MemIf_Test, MemIf_GetStatus_NG_InvalidDevice)
{
    MemIf_StatusType status = MemIf_GetStatus(kInvalidDevice);

    EXPECT_EQ(status, MEMIF_UNINIT);
    EXPECT_EQ(FakeDetHw_LastErrorId, MEMIF_E_PARAM_DEVICE);
    EXPECT_EQ(FakeDetHw_LastApiId, MEMIF_API_ID_GET_STATUS);
}

// ------------------------------------------------------------
// MemIf_GetJobResult()
// ------------------------------------------------------------

TEST_F(Bsw_MemIf_Test, MemIf_GetJobResult_NG_InvalidDevice)
{
    MemIf_JobResultType result = MemIf_GetJobResult(kInvalidDevice);

    EXPECT_EQ(result, MEMIF_JOB_FAILED);
    EXPECT_EQ(FakeDetHw_LastErrorId, MEMIF_E_PARAM_DEVICE);
    EXPECT_EQ(FakeDetHw_LastApiId, MEMIF_API_ID_GET_JOB_RESULT);
}

}  // namespace
