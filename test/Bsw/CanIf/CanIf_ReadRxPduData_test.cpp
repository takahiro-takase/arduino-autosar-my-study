/**
 * \file    CanIf_ReadRxPduData_test.cpp
 * \brief   CanIf_ReadRxPduData() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 CanIf_test.cpp）。
 *
 *          分割前の共通の説明は、旧 CanIf_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "CanIf.h"
#include "CanSM.h"
#include "Can.h"
#include "Can_Hw.h"
#include "Fake_Can_Hw.h"
#include "Fake_Det_Hw.h"
#include "Wrap_CanIf.h"
#include "Wrap_Can.h"
#include "Wrap_ComM.h"
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

const CanIf_TxPduConfigType kTestTxPdu = {
    /* UpperLayerTxPduId */ 0U,
    /* CanId */             0x100U,
    /* Dlc */               8U,
    /* Hth */               0U,
    /* TxConfirmFct */      NULL
};

const CanIf_RxPduConfigType kTestRxPdu = {
    /* CanId */                0x200U,
    /* Hrh */                  0U,
    /* UpperLayerRxPduId */    0U,
    /* Dlc */                  1U,
    /* RxIndicationFct */      NULL,
    /* ReadRxPduDataEnabled */ 0U  // CanIf_ReadRxPduData_NG_NotOptedIn がこの既定値に依存する
};

const CanIf_ConfigType kTestCanIfConfig = {
    &kTestTxPdu, 1U,
    &kTestRxPdu, 1U
};

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* CanIf（上記 2 グループ以外の API）の単体テスト用フィクスチャ。
 * SetUp(): 以前は CanIf_RxIndication() が CanSM_RxIndication() を無条件に呼んでおり（2026-10 に廃止。共有 static の状態を揃えるため CanSM_Init は残している） CanSM_Init(NULL) を先に呼び、続けて CanIf_Init(&kTestCanIfConfig) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): CanIf_DeInit() と CanSM_DeInit() で未初期化へ戻す。 */
class CanIf_ReadRxPduData_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        WrapCanIf_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制

        CanSM_Init(NULL);
        CanIf_Init(&kTestCanIfConfig);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        CanIf_DeInit();
        CanSM_DeInit();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(CanIf_ReadRxPduData_Test, CanIf_ReadRxPduData_NG_InvalidRxSduId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 buf[8];
    PduInfoType info = { buf, 0U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CanIf_ReadRxPduData(1U, &info);  // RxPduCount=1 のため範囲外

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_INVALID_RXPDUID);
}

TEST_F(CanIf_ReadRxPduData_Test, CanIf_ReadRxPduData_NG_NullInfoPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CanIf_ReadRxPduData(0U, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_POINTER);
}

TEST_F(CanIf_ReadRxPduData_Test, CanIf_ReadRxPduData_NG_NotOptedIn)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // kTestRxPdu.ReadRxPduDataEnabled == 0（既定）のため、範囲内 ID でも拒否される。
    uint8 buf[8];
    PduInfoType info = { buf, 0U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = CanIf_ReadRxPduData(0U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_INVALID_RXPDUID);
}

}  // namespace
