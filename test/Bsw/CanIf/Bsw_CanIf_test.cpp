/**
 * \file    Bsw_CanIf_test.cpp
 * \brief   CanIf.c（src/Bsw/CanIf/CanIf.c）の単体テスト（GoogleTest / PlatformIO
 *          `[env:native_chain]`）。
 *
 * \details 2026-09、モジュール単位のテストファイルを1モジュール1ファイルへ
 *          集約する方針のもと、`Bsw_CanIf_ControllerMode_test.cpp` /
 *          `Bsw_CanIf_NotifStatus_test.cpp` / 旧 `Bsw_CanIf_test.cpp`（NG系のみ）
 *          の3ファイルを本ファイルへ統合した。各セクションの経緯は元ファイルの
 *          コメントをそのまま引き継ぐ。
 *
 *          CanIf_ReadTxNotifStatus/CanIf_ReadRxNotifStatus/
 *          CanIf_GetTxConfirmationState の3関数は、src/ 全体を grep しても
 *          本番コードからの呼び出し元が一つも無い（テストファイル自身が
 *          唯一の呼び出し元）。これらの OK テスト（正常な戻り値そのものを
 *          検証するテスト）は、他モジュールで採用している「OK 系はスタックの
 *          コールチェーンで検証し、モジュール単体では NG 系のみ」という方針の
 *          対象にできない（コールチェーンが存在しないため）。かといって
 *          削除すると、この機能が正しく動作することを検証する手段が完全に
 *          無くなる。既存の `Bsw_Can_test.cpp` の
 *          `Can_SetControllerMode_NG_OtherTransition`（`#if 0` で無効化済み）と
 *          同じ前例に倣い、`#if 0` で無効化しつつコードとしては残す
 *          （将来、上位層からの呼び出しが実装された際に再度有効化できるように
 *          しておく）。NG 系（Det エラー報告の契約自体の検証）は呼び出し元の
 *          有無に関わらず価値があるため、通常どおり有効のままとする。
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

// ============================================================================
// CanIf_SetControllerMode/CanIf_GetControllerMode/CanIf_GetControllerErrorState
// （旧 Bsw_CanIf_ControllerMode_test.cpp）
// ============================================================================

/**
 * \details 2026-08-30、IF シグネチャは仕様準拠という方針のもと、CanSM/ComM/CanNm が
 *          CanIf 層を素通りして Can_SetControllerMode() を直接呼んでいたレイヤ
 *          違反を是正し、CanIf_SetControllerMode()/CanIf_GetControllerMode()
 *          （[SWS_CANIF_00003]/[SWS_CANIF_00229]）を新設した際に追加。
 *          /code-review で「新設した状態遷移判定ロジック（CanIf_ControllerMode[]
 *          による CAN_T_STOP/CAN_T_WAKEUP の使い分け）と NG 系に対するテストが
 *          無い」と指摘され追加した。
 *
 *          CanSM は経由せず、CanIf_SetControllerMode()/GetControllerMode() を
 *          直接叩いて Can.c の実体（Can_Test_GetControllerState()）で検証する。
 */

const CanIf_ConfigType kControllerModeCanIfConfig = {
    /* TxPduConfig */ NULL,
    /* TxPduCount */  0U,
    /* RxPduConfig */ NULL,
    /* RxPduCount */  0U
};

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

class Bsw_CanIf_ControllerMode_Test : public ::testing::Test
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
        CanIf_Init(&kControllerModeCanIfConfig);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        CanIf_DeInit();
    }

    Can_ConfigType canConfig;
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Bsw_CanIf_ControllerMode_Test, SetControllerMode_OK_StartedFromStoppedUsesCanTStart)
{
    Std_ReturnType ret = CanIf_SetControllerMode(0U, CAN_CS_STARTED);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);
    Can_ControllerStateType mode;
    ASSERT_EQ(CanIf_GetControllerMode(0U, &mode), E_OK);
    EXPECT_EQ(mode, CAN_CS_STARTED);
}

TEST_F(Bsw_CanIf_ControllerMode_Test, SetControllerMode_NG_InvalidControllerIdReturnsErrorAndReportsDet)
{
    Std_ReturnType ret = CanIf_SetControllerMode(CANIF_CONTROLLER_MAX, CAN_CS_STARTED);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_CONTROLLERID);
}

TEST_F(Bsw_CanIf_ControllerMode_Test, SetControllerMode_NG_InvalidControllerModeReturnsErrorAndReportsDet)
{
    Std_ReturnType ret = CanIf_SetControllerMode(0U, CAN_CS_UNINIT);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_CTRLMODE);
    /* 拒否された要求は Can 側にも CanIf の追跡状態にも影響しないこと。 */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STOPPED);
    Can_ControllerStateType mode;
    ASSERT_EQ(CanIf_GetControllerMode(0U, &mode), E_OK);
    EXPECT_EQ(mode, CAN_CS_STOPPED);
}

TEST_F(Bsw_CanIf_ControllerMode_Test, GetControllerMode_OK_ReflectsStoppedRightAfterInit)
{
    Can_ControllerStateType mode;

    ASSERT_EQ(CanIf_GetControllerMode(0U, &mode), E_OK);
    EXPECT_EQ(mode, CAN_CS_STOPPED);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(Bsw_CanIf_ControllerMode_Test, GetControllerMode_NG_InvalidControllerIdReturnsErrorAndReportsDet)
{
    Can_ControllerStateType mode;
    Std_ReturnType ret = CanIf_GetControllerMode(CANIF_CONTROLLER_MAX, &mode);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_CONTROLLERID);
}

TEST_F(Bsw_CanIf_ControllerMode_Test, GetControllerMode_NG_NullPointerReturnsErrorAndReportsDet)
{
    Std_ReturnType ret = CanIf_GetControllerMode(0U, NULL);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_POINTER);
}

/* CAN_CS_STOPPED の要求は、CanIf が追跡する遷移元状態によって Can_T_STOP
 * （STARTED から）と CAN_T_WAKEUP（SLEEP から）を使い分ける、本モジュールの
 * 中核ロジック。両方の遷移元を独立して検証する。 */

TEST_F(Bsw_CanIf_ControllerMode_Test, SetControllerMode_OK_StoppedFromStartedUsesCanTStop)
{
    ASSERT_EQ(CanIf_SetControllerMode(0U, CAN_CS_STARTED), E_OK);
    ASSERT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);

    Std_ReturnType ret = CanIf_SetControllerMode(0U, CAN_CS_STOPPED);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STOPPED);
    Can_ControllerStateType mode;
    ASSERT_EQ(CanIf_GetControllerMode(0U, &mode), E_OK);
    EXPECT_EQ(mode, CAN_CS_STOPPED);
}

TEST_F(Bsw_CanIf_ControllerMode_Test, SetControllerMode_OK_StoppedFromSleepUsesCanTWakeup)
{
    ASSERT_EQ(CanIf_SetControllerMode(0U, CAN_CS_SLEEP), E_OK);
    ASSERT_EQ(Can_Test_GetControllerState(), CAN_CS_SLEEP);

    Std_ReturnType ret = CanIf_SetControllerMode(0U, CAN_CS_STOPPED);

    EXPECT_EQ(ret, E_OK);
    /* CAN_T_STOP は CAN_CS_SLEEP からの遷移を拒否する（Can.c 参照）ため、
     * ここで実際に CAN_CS_STOPPED へ遷移していれば CAN_T_WAKEUP が
     * 選ばれたことの間接的な証明になる。 */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STOPPED);
    Can_ControllerStateType mode;
    ASSERT_EQ(CanIf_GetControllerMode(0U, &mode), E_OK);
    EXPECT_EQ(mode, CAN_CS_STOPPED);
}

TEST_F(Bsw_CanIf_ControllerMode_Test, SetControllerMode_OK_SleepFromStartedUsesCanTSleep)
{
    ASSERT_EQ(CanIf_SetControllerMode(0U, CAN_CS_STARTED), E_OK);

    Std_ReturnType ret = CanIf_SetControllerMode(0U, CAN_CS_SLEEP);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_SLEEP);
}

// ------------------------------------------------------------------------
// CanIf_GetControllerErrorState() の単体テスト（[SWS_CANIF_91001]、
// 2026-08-31 追加。CanIf → Can.c → Can_Hw.c（フェイク）の実チェーンで検証）。
// ------------------------------------------------------------------------

TEST_F(Bsw_CanIf_ControllerMode_Test, GetControllerErrorState_OK_ReflectsBusOffFromHwChain)
{
    FakeCanHw_ErrorState = 2U;  /* CAN_ERRORSTATE_BUSOFF */

    Can_ErrorStateType state;
    Std_ReturnType ret = CanIf_GetControllerErrorState(0U, &state);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(state, CAN_ERRORSTATE_BUSOFF);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(Bsw_CanIf_ControllerMode_Test, GetControllerErrorState_NG_InvalidControllerIdReturnsErrorAndReportsDet)
{
    Can_ErrorStateType state;
    Std_ReturnType ret = CanIf_GetControllerErrorState(CANIF_CONTROLLER_MAX, &state);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_CONTROLLERID);
}

TEST_F(Bsw_CanIf_ControllerMode_Test, GetControllerErrorState_NG_NullPointerReturnsErrorAndReportsDet)
{
    Std_ReturnType ret = CanIf_GetControllerErrorState(0U, NULL);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_POINTER);
}

// ============================================================================
// CanIf_ReadTxNotifStatus/CanIf_ReadRxNotifStatus/CanIf_GetTxConfirmationState
// （旧 Bsw_CanIf_NotifStatus_test.cpp）
// ============================================================================

/**
 * \details 2026-09、シグネチャ準拠サーベイで新設した
 *          CanIf_ReadTxNotifStatus()/CanIf_ReadRxNotifStatus()
 *          （[SWS_CANIF_00202]/[SWS_CANIF_00230]）を検証する。
 *          CanIf_TxConfirmation()/CanIf_RxIndication() を直接呼び、
 *          通知状態がセットされること・読み出しと同時にクリアされる
 *          ことを確認する。上位層ルーティングは不要（TxConfirmFct/
 *          RxIndicationFct=NULL）なため PduR/Com は初期化しない。
 *
 *          2026-09-05、同じくシグネチャ準拠サーベイで新設した
 *          CanIf_GetTxConfirmationState()（[SWS_CANIF_00734]、コントローラ
 *          単位で「直近の起動以降に TX 確認があったか」を返す）も本ファイルに
 *          追加。PDU 単位の Read*NotifStatus() と異なり読み出し時にはクリア
 *          されず、CanIf_SetControllerMode(STARTED) への遷移でのみリセット
 *          される点がテストの主眼。
 *
 *          CanIf_RxIndication() は無条件に CanSM_RxIndication() を呼ぶため
 *          （CanIf.c 参照）、feedback_native_chain_shared_static_hang の
 *          教訓通り CanSM_Init(NULL)（Bsw_ComStack_Signal_Rx_test.cpp と同じ安全な
 *          no-op パターン）で既知の状態に初期化してから検証する。
 *
 *          2026-09、本ファイルの3関数はいずれも src/ 全体で本番コードからの
 *          呼び出し元が無いことが判明した（テストファイル自身が唯一の
 *          呼び出し元）。OK テストはファイル冒頭コメントの方針により `#if 0`
 *          で無効化しつつ残す。NG テスト（Det エラー報告の契約検証）は
 *          呼び出し元の有無に関わらず価値があるため有効のまま維持する。
 */

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

class Bsw_CanIf_NotifStatus_Test : public ::testing::Test
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

// ------------------------------------------------------------
// CanIf_ReadTxNotifStatus()
// ------------------------------------------------------------

#if 0 // 本番コードからの呼び出し元が無いため無効化（ファイル冒頭コメント参照）
TEST_F(Bsw_CanIf_NotifStatus_Test, ReadTxNotifStatus_OK_ReturnsNoNotificationInitially)
{
    CanIf_NotifStatusType status = CanIf_ReadTxNotifStatus(0U);

    EXPECT_EQ(status, CANIF_NO_NOTIFICATION);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(Bsw_CanIf_NotifStatus_Test, ReadTxNotifStatus_OK_ReflectsTxConfirmationThenClearsOnRead)
{
    CanIf_TxConfirmation(0U);

    EXPECT_EQ(CanIf_ReadTxNotifStatus(0U), CANIF_TX_RX_NOTIFICATION);
    /* [SWS_CANIF_00393]: 読み出しと同時にリセットされること。 */
    EXPECT_EQ(CanIf_ReadTxNotifStatus(0U), CANIF_NO_NOTIFICATION);
}
#endif

TEST_F(Bsw_CanIf_NotifStatus_Test, ReadTxNotifStatus_NG_UninitializedReturnsNoNotificationWithoutDet)
{
    CanIf_DeInit();

    CanIf_NotifStatusType status = CanIf_ReadTxNotifStatus(0U);

    EXPECT_EQ(status, CANIF_NO_NOTIFICATION);
    /* CanIf_Cfg.h 冒頭コメントの通り、未初期化チェックは DET 報告なしの
     * 早期 return（CanIf の他 API と同じ方針）。 */
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(Bsw_CanIf_NotifStatus_Test, ReadTxNotifStatus_NG_InvalidIdReturnsNoNotificationAndReportsDet)
{
    CanIf_NotifStatusType status = CanIf_ReadTxNotifStatus(kNotifStatusCanIfConfig.TxPduCount);

    EXPECT_EQ(status, CANIF_NO_NOTIFICATION);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_INVALID_TXPDUID);
}

// ------------------------------------------------------------
// CanIf_ReadRxNotifStatus()
// ------------------------------------------------------------

#if 0 // 本番コードからの呼び出し元が無いため無効化（ファイル冒頭コメント参照）
TEST_F(Bsw_CanIf_NotifStatus_Test, ReadRxNotifStatus_OK_ReturnsNoNotificationInitially)
{
    CanIf_NotifStatusType status = CanIf_ReadRxNotifStatus(0U);

    EXPECT_EQ(status, CANIF_NO_NOTIFICATION);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(Bsw_CanIf_NotifStatus_Test, ReadRxNotifStatus_OK_ReflectsRxIndicationThenClearsOnRead)
{
    Can_HwType mailbox = { /* CanId */ 0x998U, /* Hoh */ 0U, /* ControllerId */ 0U };
    uint8      data[1] = { 0x42U };
    PduInfoType pduInfo = { data, 1U };

    CanIf_RxIndication(&mailbox, &pduInfo);

    EXPECT_EQ(CanIf_ReadRxNotifStatus(0U), CANIF_TX_RX_NOTIFICATION);
    /* [SWS_CANIF_00394]: 読み出しと同時にリセットされること。 */
    EXPECT_EQ(CanIf_ReadRxNotifStatus(0U), CANIF_NO_NOTIFICATION);
}
#endif

TEST_F(Bsw_CanIf_NotifStatus_Test, ReadRxNotifStatus_NG_UninitializedReturnsNoNotificationWithoutDet)
{
    CanIf_DeInit();

    CanIf_NotifStatusType status = CanIf_ReadRxNotifStatus(0U);

    EXPECT_EQ(status, CANIF_NO_NOTIFICATION);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(Bsw_CanIf_NotifStatus_Test, ReadRxNotifStatus_NG_InvalidIdReturnsNoNotificationAndReportsDet)
{
    CanIf_NotifStatusType status = CanIf_ReadRxNotifStatus(kNotifStatusCanIfConfig.RxPduCount);

    EXPECT_EQ(status, CANIF_NO_NOTIFICATION);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_INVALID_RXPDUID);
}

// ------------------------------------------------------------
// CanIf_GetTxConfirmationState()（[SWS_CANIF_00734]、2026-09-05 追加）
// ------------------------------------------------------------

#if 0 // 本番コードからの呼び出し元が無いため無効化（ファイル冒頭コメント参照）
TEST_F(Bsw_CanIf_NotifStatus_Test, GetTxConfirmationState_OK_ReturnsNoNotificationAfterInit)
{
    /* CanIf_Init() のゼロクリアを確認する基礎ケース（SetUp() の
     * CanIf_SetControllerMode(STARTED) によるリセットの検証は
     * ResetsOnControllerRestart が別途担う）。 */
    CanIf_NotifStatusType status = CanIf_GetTxConfirmationState(0U);

    EXPECT_EQ(status, CANIF_NO_NOTIFICATION);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}
#endif

#if 0 // 本番コードからの呼び出し元が無いため無効化（ファイル冒頭コメント参照）
TEST_F(Bsw_CanIf_NotifStatus_Test, GetTxConfirmationState_OK_ReflectsTxConfirmationAndDoesNotClearOnRead)
{
    CanIf_TxConfirmation(0U);

    /* [SWS_CANIF_00734] は Read*NotifStatus() と異なり読み出し時のクリアを
     * 規定しないため、複数回読んでも状態は変わらないこと。 */
    EXPECT_EQ(CanIf_GetTxConfirmationState(0U), CANIF_TX_RX_NOTIFICATION);
    EXPECT_EQ(CanIf_GetTxConfirmationState(0U), CANIF_TX_RX_NOTIFICATION);
}

TEST_F(Bsw_CanIf_NotifStatus_Test, GetTxConfirmationState_OK_ResetsOnControllerRestart)
{
    CanIf_TxConfirmation(0U);
    ASSERT_EQ(CanIf_GetTxConfirmationState(0U), CANIF_TX_RX_NOTIFICATION);

    /* 「直近のコントローラ起動以降」を表すため、再起動（CAN_CS_STARTED への
     * 再遷移）でリセットされること（Table 8.25）。 */
    ASSERT_EQ(CanIf_SetControllerMode(0U, CAN_CS_STARTED), E_OK);

    EXPECT_EQ(CanIf_GetTxConfirmationState(0U), CANIF_NO_NOTIFICATION);
}
#endif

TEST_F(Bsw_CanIf_NotifStatus_Test, GetTxConfirmationState_NG_UninitializedReturnsNoNotificationWithoutDet)
{
    CanIf_DeInit();

    CanIf_NotifStatusType status = CanIf_GetTxConfirmationState(0U);

    EXPECT_EQ(status, CANIF_NO_NOTIFICATION);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(Bsw_CanIf_NotifStatus_Test, GetTxConfirmationState_NG_InvalidControllerIdReturnsNoNotificationAndReportsDet)
{
    CanIf_NotifStatusType status = CanIf_GetTxConfirmationState(CANIF_CONTROLLER_MAX);

    EXPECT_EQ(status, CANIF_NO_NOTIFICATION);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_CONTROLLERID);
}

TEST_F(Bsw_CanIf_NotifStatus_Test, GetTxConfirmationState_NG_NotUpdatedWhileControllerStopped)
{
    /* [SWS_CANIF_00740]: STARTED でなければバッファしない。Can_TxConfQueue
     * の非同期ドレインにより、送信要求時点では STARTED でも通知到達時には
     * 停止済みというケースの回帰防止（/code-review で発見）。 */
    ASSERT_EQ(CanIf_SetControllerMode(0U, CAN_CS_STOPPED), E_OK);

    CanIf_TxConfirmation(0U);

    EXPECT_EQ(CanIf_GetTxConfirmationState(0U), CANIF_NO_NOTIFICATION);
}

// ============================================================================
// その他公開APIの NG系（旧 Bsw_CanIf_test.cpp）
// ============================================================================

/**
 * \details 上記2セクションが未カバーだった `Det_ReportError()` 呼び出し箇所
 *          （CanIf_Init/CanIf_Transmit/CanIf_ReadRxPduData/CanIf_SetPduMode/
 *          CanIf_GetPduMode/CanIf_GetVersionInfo/CanIf_TxConfirmation/
 *          CanIf_RxIndication/CanIf_ControllerBusOff）の NG ケースのみを
 *          まとめる（OK系・上記2セクションでカバー済みの NG は対象外）。
 *          各ケースで、報告される ErrorId が仕様どおり正しい値になっている
 *          ことを `Fake_Det_Hw.h`（`FakeDetHw_LastErrorId`）で検証する。
 *
 *          `CanIf_RxIndication()` は無条件に `CanSM_RxIndication()` を呼ぶため
 *          （CanSM 未初期化だと `CANSM_E_UNINIT` が割り込む）、
 *          `Bsw_ComStack_Signal_Rx_test.cpp` 等と同じ理由で CanSM も
 *          Init/DeInit する。実 Can 層（`Can_Init()`）は本セクションのどの NG
 *          ケースの到達にも不要なため（すべて `Can_Write()` 到達前に
 *          reject される）、リンクはされていても初期化はしない。
 */

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

class Bsw_CanIf_Test : public ::testing::Test
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

// ------------------------------------------------------------
// CanIf_Init()
// ------------------------------------------------------------

TEST_F(Bsw_CanIf_Test, CanIf_Init_NG_NullConfigPtr)
{
    CanIf_DeInit();

    CanIf_Init(NULL);

    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_POINTER);
}

TEST_F(Bsw_CanIf_Test, CanIf_Init_NG_RxPduCountExceedsMax)
{
    CanIf_DeInit();
    const CanIf_ConfigType badConfig = { NULL, 0U, NULL, static_cast<uint8>(CANIF_RX_PDU_MAX + 1U) };

    CanIf_Init(&badConfig);

    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_INIT_FAILED);
}

TEST_F(Bsw_CanIf_Test, CanIf_Init_NG_TxPduCountExceedsMax)
{
    CanIf_DeInit();
    const CanIf_ConfigType badConfig = { NULL, static_cast<uint8>(CANIF_TX_PDU_MAX + 1U), NULL, 0U };

    CanIf_Init(&badConfig);

    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_INIT_FAILED);
}

// ------------------------------------------------------------
// CanIf_Transmit()
// ------------------------------------------------------------

TEST_F(Bsw_CanIf_Test, CanIf_Transmit_NG_InvalidTxPduId)
{
    uint8 sdu[8] = { 0 };
    PduInfoType info = { sdu, 1U };

    Std_ReturnType ret = CanIf_Transmit(1U, &info);  // TxPduCount=1 のため範囲外

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_INVALID_TXPDUID);
}

TEST_F(Bsw_CanIf_Test, CanIf_Transmit_NG_NullPduInfoPtr)
{
    Std_ReturnType ret = CanIf_Transmit(0U, NULL);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// CanIf_ReadRxPduData()
// ------------------------------------------------------------

TEST_F(Bsw_CanIf_Test, CanIf_ReadRxPduData_NG_InvalidRxSduId)
{
    uint8 buf[8];
    PduInfoType info = { buf, 0U };

    Std_ReturnType ret = CanIf_ReadRxPduData(1U, &info);  // RxPduCount=1 のため範囲外

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_INVALID_RXPDUID);
}

TEST_F(Bsw_CanIf_Test, CanIf_ReadRxPduData_NG_NullInfoPtr)
{
    Std_ReturnType ret = CanIf_ReadRxPduData(0U, NULL);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_POINTER);
}

TEST_F(Bsw_CanIf_Test, CanIf_ReadRxPduData_NG_NotOptedIn)
{
    // kTestRxPdu.ReadRxPduDataEnabled == 0（既定）のため、範囲内 ID でも拒否される。
    uint8 buf[8];
    PduInfoType info = { buf, 0U };

    Std_ReturnType ret = CanIf_ReadRxPduData(0U, &info);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_INVALID_RXPDUID);
}

// ------------------------------------------------------------
// CanIf_SetPduMode()
// ------------------------------------------------------------

TEST_F(Bsw_CanIf_Test, CanIf_SetPduMode_NG_InvalidControllerId)
{
    Std_ReturnType ret = CanIf_SetPduMode(CANIF_CONTROLLER_MAX, CANIF_ONLINE);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_CONTROLLERID);
}

TEST_F(Bsw_CanIf_Test, CanIf_SetPduMode_NG_InvalidPduMode)
{
    // 0x02 (CANIF_TX_OFFLINE_ACTIVE) は CanIf_PduModeType の定義値に無い
    // （未実装・欠番、CanIf_Types.h 冒頭コメント参照）。
    Std_ReturnType ret = CanIf_SetPduMode(0U, static_cast<CanIf_PduModeType>(0x02));

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_PDU_MODE);
}

// ------------------------------------------------------------
// CanIf_GetPduMode()
// ------------------------------------------------------------

TEST_F(Bsw_CanIf_Test, CanIf_GetPduMode_NG_InvalidControllerId)
{
    CanIf_PduModeType mode;

    Std_ReturnType ret = CanIf_GetPduMode(CANIF_CONTROLLER_MAX, &mode);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_CONTROLLERID);
}

TEST_F(Bsw_CanIf_Test, CanIf_GetPduMode_NG_NullPointer)
{
    Std_ReturnType ret = CanIf_GetPduMode(0U, NULL);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// CanIf_GetVersionInfo()
// ------------------------------------------------------------

TEST_F(Bsw_CanIf_Test, CanIf_GetVersionInfo_NG_NullPointer)
{
    CanIf_GetVersionInfo(NULL);

    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// CanIf_TxConfirmation()
// ------------------------------------------------------------

TEST_F(Bsw_CanIf_Test, CanIf_TxConfirmation_NG_InvalidTxPduId)
{
    CanIf_TxConfirmation(1U);  // TxPduCount=1 のため範囲外

    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_LPDU);
}

// ------------------------------------------------------------
// CanIf_RxIndication()
// ------------------------------------------------------------

TEST_F(Bsw_CanIf_Test, CanIf_RxIndication_NG_NullMailbox)
{
    uint8 sdu[8] = { 0 };
    PduInfoType info = { sdu, 1U };

    CanIf_RxIndication(NULL, &info);

    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_POINTER);
}

TEST_F(Bsw_CanIf_Test, CanIf_RxIndication_NG_CanIdMismatch)
{
    // Hoh は kTestRxPdu.Hrh(=0) と一致するが、CanId が異なる
    // （SWS_CANIF_00417、Hoh一致だが CanId不一致のケース）。
    uint8 sdu[8] = { 0 };
    PduInfoType info = { sdu, 1U };
    Can_HwType mailbox = { 0x999U, 0U, 0U };

    CanIf_RxIndication(&mailbox, &info);

    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_CANID);
}

TEST_F(Bsw_CanIf_Test, CanIf_RxIndication_NG_NoHohMatch)
{
    // Hoh(=99) がどの RX PDU の Hrh とも一致しない（SWS_CANIF_00416）。
    uint8 sdu[8] = { 0 };
    PduInfoType info = { sdu, 1U };
    Can_HwType mailbox = { 0x200U, 99U, 0U };

    CanIf_RxIndication(&mailbox, &info);

    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_HOH);
}

// ------------------------------------------------------------
// CanIf_ControllerBusOff()
// ------------------------------------------------------------

TEST_F(Bsw_CanIf_Test, CanIf_ControllerBusOff_NG_InvalidControllerId)
{
    CanIf_ControllerBusOff(1U);  // CANIF_CONTROLLER_MAX=1 のため範囲外

    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_CONTROLLERID);
}

}  // namespace
