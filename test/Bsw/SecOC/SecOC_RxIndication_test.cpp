/**
 * \file    SecOC_RxIndication_test.cpp
 * \brief   SecOC_RxIndication() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 SecOC_test.cpp）。
 *
 *          `SecOC_RxIndication()` の `SECOC_E_CRYPTO_FAILURE`
 *          （[SWS_SecOC_00166]、Csm/CryIf/Crypto 自体の異常）は、Csm は
 *          初期化しつつ CryIf を意図的に未初期化のままにすることで
 *          `Csm_MacVerify()` の `CSM_E_SERVICE_NOT_STARTED` 経路を通して
 *          再現する（`test/Bsw/Csm/` と同じ手法）。
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
class SecOC_RxIndication_Test : public ::testing::Test
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

TEST_F(SecOC_RxIndication_Test, SecOC_RxIndication_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    SecOC_DeInit();
    uint8 buf[6];
    PduInfoType info = MakeValidSecuredPdu(buf);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    SecOC_RxIndication(0U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, SECOC_E_UNINIT);
}

TEST_F(SecOC_RxIndication_Test, SecOC_RxIndication_NG_NullPduInfoPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    SecOC_RxIndication(0U, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, SECOC_E_PARAM_POINTER);
}

TEST_F(SecOC_RxIndication_Test, SecOC_RxIndication_NG_NullSduDataPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PduInfoType info = { NULL, 6U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    SecOC_RxIndication(0U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, SECOC_E_PARAM_POINTER);
}

TEST_F(SecOC_RxIndication_Test, SecOC_RxIndication_NG_InvalidPduSduId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 buf[6];
    PduInfoType info = MakeValidSecuredPdu(buf);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    SecOC_RxIndication(1U, &info);  // SecOC_Config には RxPduId=0 しか無い

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, SECOC_E_INVALID_PDU_SDU_ID);
}

TEST_F(SecOC_RxIndication_Test, SecOC_RxIndication_NG_CryptoFailure)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // Csm は初期化するが CryIf は意図的に未初期化のままにし、
    // Csm_MacVerify() を CSM_E_SERVICE_NOT_STARTED 経路で失敗させる
    // （ファイル冒頭コメント参照）。
    FakeDetHw_LogSuppressed = 1U;
    Csm_Init();
    FakeDetHw_LogSuppressed = 0U;

    uint8 buf[6];
    PduInfoType info = MakeValidSecuredPdu(buf);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    SecOC_RxIndication(0U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, SECOC_E_CRYPTO_FAILURE);
}

}  // namespace
