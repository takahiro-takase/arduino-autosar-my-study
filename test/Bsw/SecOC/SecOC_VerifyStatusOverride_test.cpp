/**
 * \file    SecOC_VerifyStatusOverride_test.cpp
 * \brief   SecOC_VerifyStatusOverride() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 SecOC_test.cpp）。
 *
 *          分割前の共通の説明は、旧 SecOC_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "SecOC.h"
#include "SecOC_Cfg.h"
#include "SecOC_PBCfg.h"
#include "Csm.h"
#include "CryIf.h"
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

/* SecOC の単体テスト用フィクスチャ。
 * SetUp(): SecOC_Init(&SecOC_Config) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。MakeValidSecuredPdu() は、有効な Secured I-PDU を作る補助関数。
 * TearDown(): SecOC_DeInit() を呼び、Csm と CryIf を未初期化の状態へ戻す。 */
class SecOC_VerifyStatusOverride_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        SecOC_Init(&SecOC_Config);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        SecOC_DeInit();
        Csm_Test_ResetInitState();
        CryIf_Test_ResetInitState();
    }

    PduInfoType MakeValidSecuredPdu(uint8* buf)
    {
        for (uint8 i = 0U; i < 6U; i++)
            buf[i] = 0U;
        PduInfoType info = { buf, 6U };  // SecOC_Config の RxPdu[0].SecuredPduLength=6
        return info;
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(SecOC_VerifyStatusOverride_Test, SecOC_VerifyStatusOverride_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    SecOC_DeInit();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = SecOC_VerifyStatusOverride(0U, SECOC_OVERRIDE_CANCEL, 0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, SECOC_E_UNINIT);
}

TEST_F(SecOC_VerifyStatusOverride_Test, SecOC_VerifyStatusOverride_NG_InvalidPduSduId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = SecOC_VerifyStatusOverride(1U, SECOC_OVERRIDE_CANCEL, 0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, SECOC_E_INVALID_PDU_SDU_ID);
}

}  // namespace
