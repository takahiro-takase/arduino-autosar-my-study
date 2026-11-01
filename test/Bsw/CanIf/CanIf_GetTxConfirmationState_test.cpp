/**
 * \file    CanIf_GetTxConfirmationState_test.cpp
 * \brief   CanIf_GetTxConfirmationState() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 CanIf_test.cpp）。
 *
 *          CanIf_ReadTxNotifStatus/CanIf_ReadRxNotifStatus/
 *          CanIf_GetTxConfirmationState の3関数は、src/ 全体を grep しても
 *          本番コードからの呼び出し元が一つも無い（テストファイル自身が
 *          唯一の呼び出し元）。これらの OK テスト（正常な戻り値そのものを
 *          検証するテスト）は、他モジュールで採用している「OK 系はスタックの
 *          コールチェーンで検証し、モジュール単体では NG 系のみ」という方針の
 *          対象にできない（コールチェーンが存在しないため）。かといって
 *          削除すると、この機能が正しく動作することを検証する手段が完全に
 *          無くなる。既存の `test/Bsw/Can/` の
 *          `Can_SetControllerMode_NG_OtherTransition`（`#if 0` で無効化済み）と
 *          同じ前例に倣い、`#if 0` で無効化しつつコードとしては残す
 *          （将来、上位層からの呼び出しが実装された際に再度有効化できるように
 *          しておく）。NG 系（Det エラー報告の契約自体の検証）は呼び出し元の
 *          有無に関わらず価値があるため、通常どおり有効のままとする。
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

const CanIf_TxPduConfigType kNotifStatusCanIfTxPdu = {
    /* UpperLayerTxPduId */ 0U,
    /* CanId */             0x999U,
    /* Dlc */               1U,
    /* Hth */               0U,
    /* TxConfirmFct */      NULL
};

const CanIf_RxPduConfigType kNotifStatusCanIfRxPdu = {
    /* CanId */                0x998U,
    /* Hrh */                  0U,
    /* UpperLayerRxPduId */    0U,
    /* Dlc */                  1U,
    /* RxIndicationFct */      NULL,
    /* ReadRxPduDataEnabled */ 0U
};

const CanIf_ConfigType kNotifStatusCanIfConfig = {
    /* TxPduConfig */ &kNotifStatusCanIfTxPdu,
    /* TxPduCount */  1U,
    /* RxPduConfig */ &kNotifStatusCanIfRxPdu,
    /* RxPduCount */  1U
};

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* CanIf の通知状態系 API（ReadTxNotifStatus / ReadRxNotifStatus / GetTxConfirmationState）の単体テスト用フィクスチャ。
 * SetUp(): 模擬 Can_Hw と wrap をリセットし、Can_Init() と、このグループ専用の CanIf 設定（kNotifStatusCanIfConfig）で CanIf_Init() を呼び、コントローラを STARTED にする。以前は CanIf_RxIndication() が CanSM_RxIndication() を無条件に呼んでおり（2026-10 に廃止。共有 static の状態を揃えるため CanSM_Init は残している）、CanSM_Init(NULL) も呼んでおく（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): CanSM_DeInit() と CanIf_DeInit() で未初期化へ戻す。 */
class CanIf_GetTxConfirmationState_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeCanHw_Reset();
        WrapCan_Reset();  // 同上（Can.c 側）
        WrapCanIf_Reset();  // 他ファイルの故障注入が漏れ伝わらないよう防御的にリセット
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制

        canConfig.filter.filterId = 0x0220U;
        canConfig.filter.mask     = 0x1FFFU;
        canConfig.csPin           = 10U;
        canConfig.intPin          = 2U;
        canConfig.baudrate        = 500000U;
        canConfig.crystalFreq     = CAN_CRYSTAL_16MHZ;

        Can_Init(&canConfig);
        CanIf_Init(&kNotifStatusCanIfConfig);
        CanIf_SetControllerMode(0U, CAN_CS_STARTED);
        CanSM_Init(NULL);  // CanIf_RxIndication() が無条件に呼ぶため既知の no-op 状態にする

        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        CanSM_DeInit();
        CanIf_DeInit();
    }

    Can_ConfigType canConfig;
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(CanIf_GetTxConfirmationState_Test, GetTxConfirmationState_NG_UninitializedReturnsNoNotificationAndReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanIf_DeInit();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanIf_NotifStatusType status = CanIf_GetTxConfirmationState(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(status, CANIF_NO_NOTIFICATION);
    EXPECT_EQ(FakeDetHw_ReportCount, 1U);   // 未初期化は DET へ報告する（[SWS_CANIF_00901]）
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_UNINIT);
    EXPECT_EQ(FakeDetHw_LastApiId, CANIF_API_ID_GET_TX_CONFIRMATION_STATE);
}

TEST_F(CanIf_GetTxConfirmationState_Test, GetTxConfirmationState_NG_InvalidControllerIdReturnsNoNotificationAndReportsDet)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanIf_NotifStatusType status = CanIf_GetTxConfirmationState(CANIF_CONTROLLER_MAX);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(status, CANIF_NO_NOTIFICATION);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_CONTROLLERID);
}

TEST_F(CanIf_GetTxConfirmationState_Test, GetTxConfirmationState_NG_NotUpdatedWhileControllerStopped)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    /* [SWS_CANIF_00740]: STARTED でなければバッファしない。Can_TxConfQueue
     * の非同期ドレインにより、送信要求時点では STARTED でも通知到達時には
     * 停止済みというケースの回帰防止（/code-review で発見）。 */
    ASSERT_EQ(CanIf_SetControllerMode(0U, CAN_CS_STOPPED), E_OK);

    CanIf_TxConfirmation(0U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanIf_NotifStatusType ret = CanIf_GetTxConfirmationState(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, CANIF_NO_NOTIFICATION);
}

#if 0 // 本番コードからの呼び出し元が無いため無効化（ファイル冒頭コメント参照）
TEST_F(CanIf_GetTxConfirmationState_Test, GetTxConfirmationState_OK_ReturnsNoNotificationAfterInit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    /* CanIf_Init() のゼロクリアを確認する基礎ケース（SetUp() の
     * CanIf_SetControllerMode(STARTED) によるリセットの検証は
     * ResetsOnControllerRestart が別途担う）。 */
    CanIf_NotifStatusType status = CanIf_GetTxConfirmationState(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(status, CANIF_NO_NOTIFICATION);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}
#endif

#if 0 // 本番コードからの呼び出し元が無いため無効化（ファイル冒頭コメント参照）
TEST_F(CanIf_GetTxConfirmationState_Test, GetTxConfirmationState_OK_ReflectsTxConfirmationAndDoesNotClearOnRead)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanIf_TxConfirmation(0U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    /* [SWS_CANIF_00734] は Read*NotifStatus() と異なり読み出し時のクリアを
     * 規定しないため、複数回読んでも状態は変わらないこと。 */
    CanIf_NotifStatusType ret1 = CanIf_GetTxConfirmationState(0U);
    CanIf_NotifStatusType ret2 = CanIf_GetTxConfirmationState(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret1, CANIF_TX_RX_NOTIFICATION);
    EXPECT_EQ(ret2, CANIF_TX_RX_NOTIFICATION);
}

TEST_F(CanIf_GetTxConfirmationState_Test, GetTxConfirmationState_OK_ResetsOnControllerRestart)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanIf_TxConfirmation(0U);
    ASSERT_EQ(CanIf_GetTxConfirmationState(0U), CANIF_TX_RX_NOTIFICATION);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    /* 「直近のコントローラ起動以降」を表すため、再起動（CAN_CS_STARTED への
     * 再遷移）でリセットされること（Table 8.25）。 */
    Std_ReturnType retStart = CanIf_SetControllerMode(0U, CAN_CS_STARTED);

    CanIf_NotifStatusType stateAfterRestart = CanIf_GetTxConfirmationState(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_EQ(retStart, E_OK);
    EXPECT_EQ(stateAfterRestart, CANIF_NO_NOTIFICATION);
}
#endif

}  // namespace
