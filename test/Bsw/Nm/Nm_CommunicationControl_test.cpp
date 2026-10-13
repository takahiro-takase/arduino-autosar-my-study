/**
 * \file    Nm_CommunicationControl_test.cpp
 * \brief   Nm_DisableCommunication() / Nm_EnableCommunication() の単体テスト
 * \details 診断の通信制御（UDS 0x28）が呼ぶ Nm の 2 関数は、メインのネットワーク（NM_MAIN_NETWORK_HANDLE）の CanNm へ委譲し、
 *          CanNm の戻り値をそのまま返す。
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

/* Nm_DisableCommunication() / Nm_EnableCommunication() の単体テスト用フィクスチャ。
 * SetUp(): DET の記録を初期化し、CanNm_Init(NULL)・Nm_Init(NULL) を呼ぶ（Init 自体の記録は消す）。
 * TearDown(): Nm・CanNm を未初期化へ戻す。 */
class Nm_CommunicationControl_Test : public ::testing::Test
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

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Nm_CommunicationControl_Test, Nm_CommunicationControl_OK_DelegatesToCanNm)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType retDisable = Nm_DisableCommunication(NM_MAIN_NETWORK_HANDLE);
    const Std_ReturnType retEnable = Nm_EnableCommunication(NM_MAIN_NETWORK_HANDLE);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(retDisable, E_OK);
    EXPECT_EQ(retEnable, E_OK);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

}  // namespace
