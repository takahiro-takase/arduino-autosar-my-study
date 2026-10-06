/**
 * \file    Bsw_SecOC_test.cpp
 * \brief   SecOC.c（src/Bsw/SecOC/SecOC.c）の単体テスト（NG系）
 * \details SecOC.c 全公開APIの `Det_ReportError()` 呼び出し箇所（NG ケースの
 *          み）を1ファイルにまとめる。各ケースで、報告される ErrorId が
 *          仕様どおり正しい値になっていることを `Fake_Det_Hw.h`
 *          （`FakeDetHw_LastErrorId`）で検証する。
 *
 *          SecOC には `SecOC_DeInit()` が存在するため、Mcu/PduR/CanTp と異なり
 *          「未初期化状態」の検証にテスト専用のリセット関数は不要（各テストが
 *          明示的に `SecOC_DeInit()` を呼んでから検証する）。
 *
 *          `SecOC_RxIndication()` の `SECOC_E_CRYPTO_FAILURE`
 *          （[SWS_SecOC_00166]、Csm/CryIf/Crypto 自体の異常）は、Csm は
 *          初期化しつつ CryIf を意図的に未初期化のままにすることで
 *          `Csm_MacVerify()` の `CSM_E_SERVICE_NOT_STARTED` 経路を通して
 *          再現する（`Bsw_Csm_test.cpp` と同じ手法）。
 *
 *          `SECOC_TX_PDU_COUNT` は本プロジェクトでは 0（TX 方向で SecOC を
 *          使う PDU が無い、SecOC_Cfg.h 参照）のため、`SecOC_IfTransmit()`/
 *          `SecOC_TxConfirmation()` はどの TxPduId を渡しても
 *          `SECOC_E_INVALID_PDU_SDU_ID` になる。
 *
 *          GoogleTest の main() は test_main.cpp に集約しているため、
 *          本ファイルでは定義しない。
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
class Bsw_SecOC_Test : public ::testing::Test
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

// ------------------------------------------------------------
// SecOC_Init()
// ------------------------------------------------------------

TEST_F(Bsw_SecOC_Test, SecOC_Init_NG_NullConfigPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    SecOC_DeInit();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    SecOC_Init(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, SECOC_E_PARAM_POINTER);
}

TEST_F(Bsw_SecOC_Test, SecOC_Init_NG_RxPduCountExceedsMax)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    SecOC_DeInit();
    const SecOC_ConfigType badConfig = { NULL, SECOC_RX_PDU_COUNT + 1U, NULL, 0U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    SecOC_Init(&badConfig);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, SECOC_E_INIT_FAILED);
}

TEST_F(Bsw_SecOC_Test, SecOC_Init_NG_TxPduCountExceedsMax)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    SecOC_DeInit();
    const SecOC_ConfigType badConfig = { NULL, 0U, NULL, SECOC_TX_PDU_COUNT + 1U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    SecOC_Init(&badConfig);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, SECOC_E_INIT_FAILED);
}

// ------------------------------------------------------------
// SecOC_DeInit()
// ------------------------------------------------------------

TEST_F(Bsw_SecOC_Test, SecOC_DeInit_NG_Uninit)
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

// ------------------------------------------------------------
// SecOC_RxIndication()
// ------------------------------------------------------------

TEST_F(Bsw_SecOC_Test, SecOC_RxIndication_NG_Uninit)
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

TEST_F(Bsw_SecOC_Test, SecOC_RxIndication_NG_NullPduInfoPtr)
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

TEST_F(Bsw_SecOC_Test, SecOC_RxIndication_NG_NullSduDataPtr)
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

TEST_F(Bsw_SecOC_Test, SecOC_RxIndication_NG_InvalidPduSduId)
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

TEST_F(Bsw_SecOC_Test, SecOC_RxIndication_NG_CryptoFailure)
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

// ------------------------------------------------------------
// SecOC_VerifyStatusOverride()
// ------------------------------------------------------------

TEST_F(Bsw_SecOC_Test, SecOC_VerifyStatusOverride_NG_Uninit)
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

TEST_F(Bsw_SecOC_Test, SecOC_VerifyStatusOverride_NG_InvalidPduSduId)
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

// ------------------------------------------------------------
// SecOC_IfTransmit()
// ------------------------------------------------------------

TEST_F(Bsw_SecOC_Test, SecOC_IfTransmit_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    SecOC_DeInit();
    uint8 sdu[2] = { 0 };
    PduInfoType info = { sdu, sizeof(sdu) };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = SecOC_IfTransmit(0U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, SECOC_E_UNINIT);
}

TEST_F(Bsw_SecOC_Test, SecOC_IfTransmit_NG_NullPduInfoPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = SecOC_IfTransmit(0U, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, SECOC_E_PARAM_POINTER);
}

TEST_F(Bsw_SecOC_Test, SecOC_IfTransmit_NG_InvalidPduSduId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // SECOC_TX_PDU_COUNT=0 のため、どの TxPduId でも一致しない
    // （ファイル冒頭コメント参照）。
    uint8 sdu[2] = { 0 };
    PduInfoType info = { sdu, sizeof(sdu) };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = SecOC_IfTransmit(0U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, SECOC_E_INVALID_PDU_SDU_ID);
}

// ------------------------------------------------------------
// SecOC_TxConfirmation()
// ------------------------------------------------------------

TEST_F(Bsw_SecOC_Test, SecOC_TxConfirmation_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    SecOC_DeInit();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    SecOC_TxConfirmation(0U, E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, SECOC_E_UNINIT);
}

TEST_F(Bsw_SecOC_Test, SecOC_TxConfirmation_NG_InvalidPduSduId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    SecOC_TxConfirmation(0U, E_OK);  // SECOC_TX_PDU_COUNT=0 のため一致しない

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, SECOC_E_INVALID_PDU_SDU_ID);
}

// ------------------------------------------------------------
// SecOC_MainFunctionTx()
// ------------------------------------------------------------

TEST_F(Bsw_SecOC_Test, SecOC_MainFunctionTx_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    SecOC_DeInit();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    SecOC_MainFunctionTx();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, SECOC_E_UNINIT);
}

// ------------------------------------------------------------
// SecOC_MainFunctionRx()
// ------------------------------------------------------------

TEST_F(Bsw_SecOC_Test, SecOC_MainFunctionRx_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    SecOC_DeInit();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    SecOC_MainFunctionRx();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, SECOC_E_UNINIT);
}

// ------------------------------------------------------------
// SecOC_GetVersionInfo()（他BSWモジュール共通の慣例により未初期化チェック対象外）
// ------------------------------------------------------------

TEST_F(Bsw_SecOC_Test, SecOC_GetVersionInfo_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    SecOC_GetVersionInfo(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, SECOC_E_PARAM_POINTER);
}

}  // namespace
