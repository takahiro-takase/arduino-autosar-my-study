/**
 * \file    SecOC_DeInit_test.cpp
 * \brief   SecOC_DeInit() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 SecOC_test.cpp）。
 *
 *          SecOC には `SecOC_DeInit()` が存在するため、Mcu/PduR/CanTp と異なり
 *          「未初期化状態」の検証にテスト専用のリセット関数は不要（各テストが
 *          明示的に `SecOC_DeInit()` を呼んでから検証する）。
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
class SecOC_DeInit_Test : public ::testing::Test
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

TEST_F(SecOC_DeInit_Test, SecOC_DeInit_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    SecOC_DeInit();  // 1回目: SetUp() の Init を正常に解除する
    FakeDetHw_Reset();

    SecOC_DeInit();  // 2回目: 既に未初期化のため NG

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, SECOC_E_UNINIT);
}

}  // namespace
