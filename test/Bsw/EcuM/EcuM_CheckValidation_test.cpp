/**
 * \file    EcuM_CheckValidation_test.cpp
 * \brief   EcuM_CheckValidation() の単体テスト（コールアウト、[SWS_EcuM_02925]）
 * \details CAN の起床要因が含まれていれば CanIf へ検証を問い合わせ、含まれていなければ何もしない。
 *          CanIf は未初期化のままにして、CanIf_CheckValidation() が DET へ報告する UNINIT を、呼び出された証拠にする。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "CanIf.h"
#include "CanIf_Cfg.h"
#include "EcuM.h"
#include "EcuM_Cfg.h"
#include "Fake_Bsw_EcuM.h"
#include "Fake_Det_Hw.h"
}

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

namespace
{

/** CAN 以外の起床要因（予約済みのビットのひとつ。ECUM_WKSOURCE_CAN と重ならない値） */
constexpr EcuM_WakeupSourceType kOtherSource = 0x01UL;

/* EcuM_CheckValidation() の単体テスト用フィクスチャ。
 * SetUp(): 模擬 EcuM をパススルーにして EcuM を未初期化へ戻し、DET の記録を初期化する（CanIf は未初期化のまま）。
 * TearDown(): 模擬 EcuM と DET の状態も含めて元に戻す。 */
class EcuM_CheckValidation_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeEcuM_Reset();
        FakeEcuM_PassThrough = 1U;
        EcuM_Test_ResetInitState();
        FakeDetHw_Reset();
        FakeDetHw_LogSuppressed = 1U;
    }

    void TearDown() override
    {
        EcuM_Test_ResetInitState();
        FakeEcuM_Reset();  // PassThrough も FALSE へ戻る
        FakeDetHw_Reset();
    }

    /** CanIf_CheckValidation() が呼ばれた（未初期化の DET 報告が 1 件）ことを確認する。 */
    static void ExpectCanIfCalled()
    {
        EXPECT_EQ(FakeDetHw_ReportCount, 1U);
        EXPECT_EQ(FakeDetHw_LastModuleId, CANIF_MODULE_ID);
        EXPECT_EQ(FakeDetHw_LastApiId, CANIF_API_ID_CHECK_VALIDATION);
        EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_UNINIT);
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

// OK: CAN の起床要因が含まれていれば、CanIf へ検証を問い合わせる
TEST_F(EcuM_CheckValidation_Test, EcuM_CheckValidation_OK_CanSourceAsksCanIf)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    EcuM_CheckValidation(ECUM_WKSOURCE_CAN);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectCanIfCalled();
}

// OK: CAN 以外の要因だけなら、何もしない（CanIf の「不明な要因」の DET も出さない）
TEST_F(EcuM_CheckValidation_Test, EcuM_CheckValidation_OK_NonCanSourceIsIgnored)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    EcuM_CheckValidation(kOtherSource);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

// OK: CAN と CAN 以外の要因が混ざっていても、CanIf への問い合わせは 1 回
TEST_F(EcuM_CheckValidation_Test, EcuM_CheckValidation_OK_MixedMaskAsksCanIfOnce)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    static_assert((ECUM_WKSOURCE_CAN & 0x01UL) == 0U, "kOtherSource は CAN と重ならないこと");

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    EcuM_CheckValidation(ECUM_WKSOURCE_CAN | kOtherSource);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ExpectCanIfCalled();
}

}  // namespace
