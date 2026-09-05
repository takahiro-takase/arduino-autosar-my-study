/**
 * \file    Bsw_CanIf_NotifStatus_test.cpp
 * \brief   CanIf_ReadTxNotifStatus/CanIf_ReadRxNotifStatus/
 *          CanIf_GetTxConfirmationState の単体テスト
 *          （GoogleTest / PlatformIO `[env:native_chain]`）。
 *
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
 *          教訓通り CanSM_Init(NULL)（Bsw_RxChain_test.cpp と同じ安全な
 *          no-op パターン）で既知の状態に初期化してから検証する。
 */
#include <gtest/gtest.h>

extern "C" {
#include "CanIf.h"
#include "Can.h"
#include "Can_Hw.h"
#include "Hal_Can_Hw_fake.h"
#include "Hal_Det_Hw_fake.h"
#include "CanSM.h"
}

namespace
{

const CanIf_TxPduConfigType kTestCanIfTxPdu = {
    /* UpperLayerTxPduId */ 0U,
    /* CanId */             0x999U,
    /* Dlc */               1U,
    /* Hth */               0U,
    /* TxConfirmFct */      NULL
};

const CanIf_RxPduConfigType kTestCanIfRxPdu = {
    /* CanId */                0x998U,
    /* Hrh */                  0U,
    /* UpperLayerRxPduId */    0U,
    /* Dlc */                  1U,
    /* RxIndicationFct */      NULL,
    /* ReadRxPduDataEnabled */ 0U
};

const CanIf_ConfigType kTestCanIfConfig = {
    /* TxPduConfig */ &kTestCanIfTxPdu,
    /* TxPduCount */  1U,
    /* RxPduConfig */ &kTestCanIfRxPdu,
    /* RxPduCount */  1U
};

class Bsw_CanIf_NotifStatus_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeCanHw_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制

        canConfig.filter.filterId = 0x0220U;
        canConfig.filter.mask     = 0x1FFFU;
        canConfig.csPin           = 10U;
        canConfig.intPin          = 2U;
        canConfig.baudrate        = 500000U;
        canConfig.crystalFreq     = CAN_CRYSTAL_16MHZ;

        Can_Init(&canConfig);
        CanIf_Init(&kTestCanIfConfig);
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

TEST_F(Bsw_CanIf_NotifStatus_Test, ReadTxNotifStatus_NG_InvalidIdReturnsNoNotificationAndReportsDet)
{
    CanIf_NotifStatusType status = CanIf_ReadTxNotifStatus(kTestCanIfConfig.TxPduCount);

    EXPECT_EQ(status, CANIF_NO_NOTIFICATION);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_INVALID_TXPDUID);
}

TEST_F(Bsw_CanIf_NotifStatus_Test, ReadTxNotifStatus_NG_UninitializedReturnsNoNotificationWithoutDet)
{
    CanIf_DeInit();

    CanIf_NotifStatusType status = CanIf_ReadTxNotifStatus(0U);

    EXPECT_EQ(status, CANIF_NO_NOTIFICATION);
    /* CanIf_Cfg.h 冒頭コメントの通り、未初期化チェックは DET 報告なしの
     * 早期 return（CanIf の他 API と同じ方針）。 */
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

// ------------------------------------------------------------
// CanIf_ReadRxNotifStatus()
// ------------------------------------------------------------

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

TEST_F(Bsw_CanIf_NotifStatus_Test, ReadRxNotifStatus_NG_InvalidIdReturnsNoNotificationAndReportsDet)
{
    CanIf_NotifStatusType status = CanIf_ReadRxNotifStatus(kTestCanIfConfig.RxPduCount);

    EXPECT_EQ(status, CANIF_NO_NOTIFICATION);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_INVALID_RXPDUID);
}

TEST_F(Bsw_CanIf_NotifStatus_Test, ReadRxNotifStatus_NG_UninitializedReturnsNoNotificationWithoutDet)
{
    CanIf_DeInit();

    CanIf_NotifStatusType status = CanIf_ReadRxNotifStatus(0U);

    EXPECT_EQ(status, CANIF_NO_NOTIFICATION);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

// ------------------------------------------------------------
// CanIf_GetTxConfirmationState()（[SWS_CANIF_00734]、2026-09-05 追加）
// ------------------------------------------------------------

TEST_F(Bsw_CanIf_NotifStatus_Test, GetTxConfirmationState_OK_ReturnsNoNotificationAfterInit)
{
    /* CanIf_Init() のゼロクリアを確認する基礎ケース（SetUp() の
     * CanIf_SetControllerMode(STARTED) によるリセットの検証は
     * ResetsOnControllerRestart が別途担う）。 */
    CanIf_NotifStatusType status = CanIf_GetTxConfirmationState(0U);

    EXPECT_EQ(status, CANIF_NO_NOTIFICATION);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
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

TEST_F(Bsw_CanIf_NotifStatus_Test, GetTxConfirmationState_NG_InvalidControllerIdReturnsNoNotificationAndReportsDet)
{
    CanIf_NotifStatusType status = CanIf_GetTxConfirmationState(CANIF_CONTROLLER_MAX);

    EXPECT_EQ(status, CANIF_NO_NOTIFICATION);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANIF_E_PARAM_CONTROLLERID);
}

TEST_F(Bsw_CanIf_NotifStatus_Test, GetTxConfirmationState_NG_UninitializedReturnsNoNotificationWithoutDet)
{
    CanIf_DeInit();

    CanIf_NotifStatusType status = CanIf_GetTxConfirmationState(0U);

    EXPECT_EQ(status, CANIF_NO_NOTIFICATION);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

}  // namespace
