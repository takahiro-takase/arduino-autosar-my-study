/**
 * \file    Nm_NodeIdentifier_test.cpp
 * \brief   Nm_GetLocalNodeIdentifier() / Nm_GetNodeIdentifier() の単体テスト
 * \details 自ノードの識別子は CANNM_SOURCE_NODE_ID（CanNm.json の sourceNodeId）と一致する。Nm_GetNodeIdentifier() は、直近に NM フレームを
 *          受信した他ノードの識別子（まだ受信していなければ 0）を返す。どちらも CanNm へ委譲する。
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

/* Nm_GetLocalNodeIdentifier() / Nm_GetNodeIdentifier() の単体テスト用フィクスチャ。
 * SetUp(): DET の記録を初期化し、CanNm_Init(NULL)・Nm_Init(NULL) を呼ぶ（Init 自体の記録は消す）。
 * TearDown(): Nm・CanNm を未初期化へ戻す。 */
class Nm_NodeIdentifier_Test : public ::testing::Test
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

TEST_F(Nm_NodeIdentifier_Test, Nm_GetNodeIdentifier_OK_ReturnsLocalAndLastReceivedNodeId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 localId = 0xFFU;
    uint8 peerId = 0xFFU;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType retLocal = Nm_GetLocalNodeIdentifier(NM_MAIN_NETWORK_HANDLE, &localId);
    const Std_ReturnType retPeer = Nm_GetNodeIdentifier(NM_MAIN_NETWORK_HANDLE, &peerId);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(retLocal, E_OK);
    EXPECT_EQ(localId, static_cast<uint8>(CANNM_SOURCE_NODE_ID));
    EXPECT_EQ(retPeer, E_OK);
    EXPECT_EQ(peerId, 0U);   // まだ他ノードの NM フレームを受信していない
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

}  // namespace
