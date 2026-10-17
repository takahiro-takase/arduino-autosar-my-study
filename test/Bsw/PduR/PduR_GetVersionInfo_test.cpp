/**
 * \file    PduR_GetVersionInfo_test.cpp
 * \brief   PduR_GetVersionInfo() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 PduR_test.cpp）。
 *
 *          分割前の共通の説明は、旧 PduR_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "PduR.h"
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

const PduR_TxRoutingPathType kTestTxPath = {
    /* SrcPduId */             10U,
    /* CanIfTxPduId */         0U,
    /* ConfDestPduId */        20U,
    /* ConfFct */              NULL,
    /* TransmitOverrideFct */  NULL
};

const PduR_RxDestType kTestRxDest = {
    /* Module */    PDUR_MODULE_COM,
    /* DestPduId */ 30U,
    /* RxIndFct */  NULL
};

const PduR_RxRoutingPathType kTestRxPath = {
    /* SrcPduId */  40U,
    /* Dests */     &kTestRxDest,
    /* DestCount */ 1U
};

const PduR_PBConfigType kTestConfig = {
    &kTestRxPath, 1U,
    &kTestTxPath, 1U
};

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* PduR（上記以外の API）の単体テスト用フィクスチャ。
 * SetUp(): PduR_Init(&kTestConfig) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。 */
class PduR_GetVersionInfo_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        PduR_Init(&kTestConfig);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    // PduR に DeInit() は無いため TearDown は書かない。未初期化状態が必要な
    // テストは各自 PduR_Test_ResetInitState() を明示的に呼ぶ（ファイル冒頭
    // コメント参照）。
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(PduR_GetVersionInfo_Test, PduR_GetVersionInfo_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    PduR_GetVersionInfo(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, PDUR_E_PARAM_POINTER);
}

TEST_F(PduR_GetVersionInfo_Test, PduR_GetVersionInfo_OK_ReturnsModuleVersion)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Std_VersionInfoType info = { 0xFFFFU, 0xFFFFU, 0xFFU, 0xFFU, 0xFFU };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    PduR_GetVersionInfo(&info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(info.vendorID, PDUR_VENDOR_ID);
    EXPECT_EQ(info.moduleID, PDUR_MODULE_ID);
    EXPECT_EQ(info.sw_major_version, PDUR_SW_MAJOR_VERSION);
    EXPECT_EQ(info.sw_minor_version, PDUR_SW_MINOR_VERSION);
    EXPECT_EQ(info.sw_patch_version, PDUR_SW_PATCH_VERSION);
}

}  // namespace
