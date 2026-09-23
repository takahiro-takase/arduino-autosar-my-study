/**
 * \file    Bsw_ComStack_SignalGroup_Tx_test.cpp
 * \brief   README.md「Tx 処理」コールチェーンのうち、Signal Group
 *          （IsSignalGroup=1）関連の送信シナリオ専用の単体テスト
 *          （GoogleTest / CMake native_chain_tests）。
 *
 * \details 2026-09、`Bsw_ComStack_Tx_{TmsTransition,TmsUnchanged,
 *          InvalidateSignalGroup,SendSignalGroupArray,IpduGroupStop,
 *          IpduGroupStopRepeatsRemaining,IpduGroupStopTxTOutInteraction,
 *          TxTOutGroupLevel,SendSignal,TxConfirmation}_test.cpp` の
 *          Signal Group 関連分と、`TriggerIPDUSend_test.cpp` のうち
 *          停止可能グループ（IPduId=3）を使う2件を、Msg内容
 *          （Signal/SignalGroup/E2E/SecOC）× 方向（Tx/Rx）の
 *          `Bsw_ComStack_{MsgType}_{Tx|Rx}_test.cpp` 命名規則へ統合した
 *          （ユーザー指示）。分割時の各ファイルはCOM_TX_IPDU_MAXの制約上、
 *          4 IPdu・7シグナルの共有configをそのまま複製していたが、本ファイルが
 *          実際に使うのは IPduId=1（TMS Pendingシグナルグループ）と
 *          IPduId=3（停止可能グループ、TxErr/TxTOut/MDT/Invalidate検証用）の
 *          みのため、必要な分だけの最小configへ組み直している。
 *
 *          いずれのテストも Com_MainFunctionTx()/PduR より先（実 CAN 送信）
 *          までは進めず、`Com_Test_Get*`系のテスト専用アクセサや
 *          `Com_TxConfirmation()`直接呼び出しで検証する（元ファイルの
 *          コメントの通り、IPduId=1/3 とも PduRId に対応する経路を
 *          `PduR_PBConfigType` へ一切登録していない）。そのため Can/CanIf/
 *          PduR の初期化・実体リンクは不要（`PduR_ComTransmit()`は
 *          `PduR_ConfigPtr==NULL`でも安全に`E_NOT_OK`を返すため、
 *          `PduR_Init()`すら呼ばなくてよい。PduR.c 参照）。
 *
 *          `TxConfirmation_NG_NonGroupIPduDoesNotCallGroupAckCbk` のみ、
 *          「非グループIPduの確認通知がグループ用コールバックを誤って
 *          呼ばないこと」を証明するための比較対象として、IPduId=0
 *          （非グループ、kTestTxIPdu）をこのファイルのconfigにも
 *          追加で含めている。
 */
#include <gtest/gtest.h>

extern "C" {
#include "Com.h"
#include "Fake_Millis.h"
#include "Fake_Det_Hw.h"
}

namespace
{

// -----------------------------------------------------------------------
// SWS_Com_00495（TMS 遷移時の無条件即時送信）検証用 I-PDU（IPduId=1）。
// SignalId=1 を持つ Signal Group で、唯一のメンバーは TmsContributor=1 かつ
// TransferProperty=PENDING（TMS には寄与するが、単独では通常の送信トリガー
// Com_GroupTriggerPending を立てない）。WarningStatus の FaultLamp/AbsLamp
// （実運用設定、TmsContributor=1 かつ TRIGGERED_ON_CHANGE）とはあえて
// 異なる組み合わせにすることで、「通常の送信トリガー」を経由せずに
// 「TMS 遷移そのもの」だけで送信が引き起こされることを検証する
// （Com_Notes.md「TMS 変化時の即時送信について」参照）。
// -----------------------------------------------------------------------
const Com_SignalConfigType kTestTmsPendingSignal = {
    /* SignalId */                1U,
    /* Direction */                COM_SIGNAL_DIRECTION_TX,
    /* IPduId */                   1U,
    /* BitPosition */              0U,
    /* BitSize */                  1U,
    /* Endian */                   COM_BIG_ENDIAN,
    /* InitValue */                0U,
    /* FilterAlgorithm */          COM_FILTER_ALWAYS,  // Signal Group メンバーのため未評価（Com_SendSignal() 参照）
    /* Mask */                     0x01U,
    /* FilterX */                  0U,
    /* FilterMin */                0U,
    /* FilterMax */                0U,
    /* FilterRejectCbk */          NULL,
    /* TmsContributor */           1U,
    /* UpdateBitContributor */     0U,
    /* TransferProperty */         COM_TRANSFER_PROPERTY_PENDING,
    /* RxDataTimeoutAction */      COM_RX_TIMEOUT_ACTION_NONE,
    /* TimeoutSubstitutionValue */ 0U,
    /* DataInvalidAction */        COM_DATA_INVALID_ACTION_NONE,
    /* InvalidValue */             1U, // Com_InvalidateSignalGroup（SWS_Com_00557）検証用
    /* InvalidNotificationCbk */   NULL,
    /* FirstTimeoutMs */           0U,
    /* TimeoutMs */                0U,
    /* RxTOutCbk */                NULL,
    /* TxAckCbk */                 NULL,
    /* TxErrCbk */                 NULL,
    /* RxAckCbk */                 NULL,
    /* TxTOutCbk */                NULL,
    /* InvalidValueConfigured */   1U
};

// SWS_Com_00468（Signal Group の TxAckCbk はグループ単位で 1 回だけ呼ばれる）
// 検証用のカウンタ付きコールバック。kTestTmsGroupIPdu に設定する。
static uint8_t s_groupTxAckCount = 0U;
static void TestGroupTxAckCbk(void) { s_groupTxAckCount++; }

const Com_IPduConfigType kTestTmsGroupIPdu = {
    /* IPduId */           1U,
    /* DLC */              1U,
    /* PduRId */           1U,   // 本テストは Com_MainFunctionTx()/PduR まで進めないため未使用
    /* FirstTimeoutMs */   0U,
    /* TimeoutMs */        0U,
    /* IsSignalGroup */    1U,
    /* TxModeMode */       COM_TX_MODE_DIRECT,
    /* TxPeriodMs */       0U,
    /* TxModeModeTrue */   COM_TX_MODE_DIRECT,
    /* TxPeriodMsTrue */   0U,
    /* MinDelayMs */       0U,
    /* UpdateBitPosition */ 7U,  // bit0 は kTestTmsPendingSignal が使うため独立したビットにする
    /* IpduGroupId */      COM_IPDU_GROUP_NONE,
    /* RxIndicationCbk */  NULL,
    /* TxTransformCbk */   NULL,
    /* TxAckCbk */         TestGroupTxAckCbk
};

// -----------------------------------------------------------------------
// SWS_Com_00491（Signal Group の TxErrCbk はグループ単位で 1 回だけ呼ばれる）
// 検証用。Com_IpduGroupStop() が TxErrCbk を発火するのは「送信済み・未確認の
// まま所属 I-PDU Group が停止された」場合のみのため、COM_IPDU_GROUP_NONE
// ではなく実際に停止可能なテスト専用 IpduGroupId を割り当てる
// （kTestStoppableGroupId、本番の Com_Cfg.h の値とは無関係なテストローカル値）。
// -----------------------------------------------------------------------
static const Com_IpduGroupIdType kTestStoppableGroupId = 5U;

static uint8_t s_groupTxErrCount = 0U;
static void TestGroupTxErrCbk(void) { s_groupTxErrCount++; }

// SWS_Com_00878（TX 送信デッドライン監視）のグループ単位発火・
// Com_IpduGroupStop() との二重発火防止を検証するためのカウンタ付き
// コールバック。kTestErrGroupIPdu（Signal Group、停止可能グループ）に設定する。
static uint8_t s_groupTxTOutCount = 0U;
static void TestGroupTxTOutCbk(void) { s_groupTxTOutCount++; }

const Com_IPduConfigType kTestErrGroupIPdu = {
    /* IPduId */           3U,
    /* DLC */              1U,
    /* PduRId */           3U,   // 経路未登録（PduR_ComTransmit は経路なしで
                                 // 安全に E_NOT_OK を返す。Com_TriggerIPDUSend の
                                 // MDT 検証用に実際にディスパッチさせるが、
                                 // 実 CAN 送信までは進めない）
    /* FirstTimeoutMs */   0U,
    /* TimeoutMs */        0U,
    /* IsSignalGroup */    1U,
    /* TxModeMode */       COM_TX_MODE_DIRECT,
    /* TxPeriodMs */       0U,
    /* TxModeModeTrue */   COM_TX_MODE_DIRECT,
    /* TxPeriodMsTrue */   0U,
    /* MinDelayMs */       100U,  // Com_TriggerIPDUSend の MDT 尊重（SWS_Com_00388）検証用
    /* UpdateBitPosition */ 0xFFU,
    /* IpduGroupId */      kTestStoppableGroupId,
    /* RxIndicationCbk */  NULL,
    /* TxTransformCbk */   NULL,
    /* TxAckCbk */         NULL,
    /* TxErrCbk */         TestGroupTxErrCbk,
    /* RxAckCbk */         NULL,
    /* NumberOfRepetitions */ 0U,
    /* RepetitionPeriodMs */  0U,
    /* TxFirstTimeoutMs */    100U,  // Com_CbkTxTOut（SWS_Com_00878）検証用
    /* TxTimeoutMs */          50U,
    /* TxTOutCbk */           TestGroupTxTOutCbk
};

// -----------------------------------------------------------------------
// Com_InvalidateSignalGroup（SWS_Com_00557 等）の all-or-nothing 検証用。
// kTestErrGroupIPdu（IPduId=3、Signal Group）に新規メンバーを 2 本追加する。
// SignalId=6 は InvalidValueConfigured=1（設定済み）、SignalId=7 はあえて
// 未設定のままにし、「1本でも未設定なら全体を E_NOT_OK とする」ことの検証に使う。
// -----------------------------------------------------------------------
const Com_SignalConfigType kTestInvalidateGroupMemberA = {
    /* SignalId */                6U,
    /* Direction */                COM_SIGNAL_DIRECTION_TX,
    /* IPduId */                   3U,
    /* BitPosition */              0U,
    /* BitSize */                  1U,
    /* Endian */                   COM_BIG_ENDIAN,
    /* InitValue */                0U,
    /* FilterAlgorithm */          COM_FILTER_ALWAYS,
    /* Mask */                     0U,
    /* FilterX */                  0U,
    /* FilterMin */                0U,
    /* FilterMax */                0U,
    /* FilterRejectCbk */          NULL,
    /* TmsContributor */           0U,
    /* UpdateBitContributor */     0U,
    /* TransferProperty */         COM_TRANSFER_PROPERTY_PENDING,
    /* RxDataTimeoutAction */      COM_RX_TIMEOUT_ACTION_NONE,
    /* TimeoutSubstitutionValue */ 0U,
    /* DataInvalidAction */        COM_DATA_INVALID_ACTION_NONE,
    /* InvalidValue */             1U,
    /* InvalidNotificationCbk */   NULL,
    /* FirstTimeoutMs */           0U,
    /* TimeoutMs */                0U,
    /* RxTOutCbk */                NULL,
    /* TxAckCbk */                 NULL,
    /* TxErrCbk */                 NULL,
    /* RxAckCbk */                 NULL,
    /* TxTOutCbk */                NULL,
    /* InvalidValueConfigured */   1U
};

const Com_SignalConfigType kTestInvalidateGroupMemberB = {
    /* SignalId */                7U,
    /* Direction */                COM_SIGNAL_DIRECTION_TX,
    /* IPduId */                   3U,
    /* BitPosition */              1U,
    /* BitSize */                  1U,
    /* Endian */                   COM_BIG_ENDIAN,
    /* InitValue */                0U,
    /* FilterAlgorithm */          COM_FILTER_ALWAYS,
    /* Mask */                     0U,
    /* FilterX */                  0U,
    /* FilterMin */                0U,
    /* FilterMax */                0U,
    /* FilterRejectCbk */          NULL,
    /* TmsContributor */           0U,
    /* UpdateBitContributor */     0U,
    /* TransferProperty */         COM_TRANSFER_PROPERTY_PENDING,
    /* RxDataTimeoutAction */      COM_RX_TIMEOUT_ACTION_NONE,
    /* TimeoutSubstitutionValue */ 0U,
    /* DataInvalidAction */        COM_DATA_INVALID_ACTION_NONE,
    /* InvalidValue */             0U,
    /* InvalidNotificationCbk */   NULL,
    /* FirstTimeoutMs */           0U,
    /* TimeoutMs */                0U,
    /* RxTOutCbk */                NULL,
    /* TxAckCbk */                 NULL,
    /* TxErrCbk */                 NULL,
    /* RxAckCbk */                 NULL,
    /* TxTOutCbk */                NULL,
    /* InvalidValueConfigured */   0U  // あえて未設定のまま（all-or-nothing 検証用）
};

// -----------------------------------------------------------------------
// TxConfirmation_NG_NonGroupIPduDoesNotCallGroupAckCbk のためだけに追加する
// 非グループ I-PDU（IPduId=0）。ファイル冒頭コメント参照。
// -----------------------------------------------------------------------
const Com_IPduConfigType kTestTxIPdu = {
    /* IPduId */           0U,
    /* DLC */              2U,
    /* PduRId */           0U,
    /* FirstTimeoutMs */   0U,
    /* TimeoutMs */        0U,
    /* IsSignalGroup */    0U,
    /* TxModeMode */       COM_TX_MODE_DIRECT,
    /* TxPeriodMs */       0U,
    /* TxModeModeTrue */   COM_TX_MODE_DIRECT,
    /* TxPeriodMsTrue */   0U,
    /* MinDelayMs */       0U,
    /* UpdateBitPosition */ 0xFFU,
    /* IpduGroupId */      COM_IPDU_GROUP_NONE,
    /* RxIndicationCbk */  NULL,
    /* TxTransformCbk */   NULL
};

const Com_SignalConfigType kTestSignals[] = {
    kTestTmsPendingSignal, kTestInvalidateGroupMemberA, kTestInvalidateGroupMemberB
};
const Com_IPduConfigType   kTestTxIPdus[] = {
    kTestTxIPdu, kTestTmsGroupIPdu, kTestErrGroupIPdu
};

const Com_ConfigType kTestComConfig = {
    /* RxIPdus */       NULL,
    /* RxIPduCount */   0U,
    /* TxIPdus */       kTestTxIPdus,
    /* TxIPduCount */   3U,
    /* Signals */       kTestSignals,
    /* SignalCount */   3U,
    /* GwMappings */    NULL,
    /* GwMappingCount */ 0U
};

class Bsw_ComStack_SignalGroup_Tx_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制

        Com_Init(&kTestComConfig);
        s_groupTxAckCount = 0U;
        s_groupTxErrCount = 0U;
        s_groupTxTOutCount = 0U;

        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;  // DeInit() のログを抑制
        Com_DeInit();
    }
};

// ------------------------------------------------------------
// SWS_Com_00495（TMS 遷移時の無条件即時送信、Signal Group 側経路）
// ------------------------------------------------------------
TEST_F(Bsw_ComStack_SignalGroup_Tx_Test, TmsTransition_OK_TriggersImmediateSendWithoutGroupTrigger)
{
    /* 準備 (Arrange): TMS 寄与シグナルを 0(false)→1(true) へ変化させる。
     * TransferProperty=PENDING のため、この変化だけでは
     * Com_GroupTriggerPending は立たない。 */
    uint8_t value = 1U;
    Com_SendSignal(1U, &value);

    /* 実行 (Act): シャドウバッファを確定コミットし、TMS を再評価させる */
    Std_ReturnType ret = Com_SendSignalGroup(1U);

    /* 評価 (Assert) */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(Com_Test_GetTxPending(1U), 1U);  // TMS 遷移のみで即時送信要求が立つ

    /* update-bit（bit7）は「値が実際に更新されたか」を示すものであり、
     * TMS 遷移そのもの（PENDING メンバーの変化）とは独立した判断軸のため、
     * このケースではセットされない。
     * ビット番号はネットワーク順（bit0 = byte[0] の MSB=0x80、
     * bit7 = byte[0] の LSB=0x01。Com_PackSignal() 参照）。 */
    const uint8* buf = Com_Test_GetTxBuffer(1U);
    ASSERT_NE(buf, nullptr);
    EXPECT_EQ(buf[0] & 0x80U, 0x80U);  // シグナル値自体（bit0）はコミットされている
    EXPECT_EQ(buf[0] & 0x01U, 0x00U);  // update-bit (bit7) は立たない
}


TEST_F(Bsw_ComStack_SignalGroup_Tx_Test, TmsUnchanged_OK_DoesNotTriggerSendWithoutGroupTrigger)
{
    /* 準備 (Arrange): 初期状態（TMS=false）から変化させない
     * （0 のままシグナルグループをコミットする） */
    uint8_t value = 0U;
    Com_SendSignal(1U, &value);

    /* 実行 (Act) */
    Std_ReturnType ret = Com_SendSignalGroup(1U);

    /* 評価 (Assert): TMS が変化せず、通常トリガーも立たないため送信要求は立たない */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(Com_Test_GetTxPending(1U), 0U);
}

// ------------------------------------------------------------
// Com_SwitchIpduTxMode（SWS_Com_00244、TMS の外部トグル、Signal Group側）
// ------------------------------------------------------------
TEST_F(Bsw_ComStack_SignalGroup_Tx_Test, SwitchIpduTxMode_OK_FlipsStateAndTriggersImmediateSend)
{
    /* 準備 (Arrange): Com_Init() 直後は Com_TmsState[1]==0（既定 false）。 */
    ASSERT_EQ(Com_Test_GetTmsState(1U), 0U);

    /* 実行 (Act) */
    Com_SwitchIpduTxMode(1U, 1U);

    /* 評価 (Assert): [SWS_Com_00881] 状態が切り替わり、[SWS_Com_00239]/
     * [SWS_Com_00495] と同じ経路（Com_RequestTxOnChange()）で次回
     * Com_MainFunctionTx() 向けの送信要求が立つ。 */
    EXPECT_EQ(Com_Test_GetTmsState(1U), 1U);
    EXPECT_EQ(Com_Test_GetTxPending(1U), 1U);
}

TEST_F(Bsw_ComStack_SignalGroup_Tx_Test, SwitchIpduTxMode_NG_NoEffectWhenModeAlreadyActive)
{
    /* 準備 (Arrange): 既定状態(false)と同じ Mode=0 を明示的に要求する。 */

    /* 実行 (Act) */
    Com_SwitchIpduTxMode(1U, 0U);

    /* 評価 (Assert): spec 原文 "the call will have no effect"。送信要求も
     * 立たない（状態が変化していないため Com_RequestTxOnChange() は
     * 呼ばれない）。 */
    EXPECT_EQ(Com_Test_GetTmsState(1U), 0U);
    EXPECT_EQ(Com_Test_GetTxPending(1U), 0U);
}

TEST_F(Bsw_ComStack_SignalGroup_Tx_Test, SwitchIpduTxMode_OK_TogglingBackTriggersAnotherSend)
{
    /* 準備 (Arrange): 一旦 true へ切り替え、Com_MainFunctionTx() で
     * 送信要求を消費させておく。 */
    Com_SwitchIpduTxMode(1U, 1U);
    Com_MainFunctionTx();
    ASSERT_EQ(Com_Test_GetTxPending(1U), 0U);

    /* 実行 (Act): false へ戻す（再び実際の変化） */
    Com_SwitchIpduTxMode(1U, 0U);

    /* 評価 (Assert): 戻すのも「変化」であるため、再度送信要求が立つ */
    EXPECT_EQ(Com_Test_GetTmsState(1U), 0U);
    EXPECT_EQ(Com_Test_GetTxPending(1U), 1U);
}

TEST_F(Bsw_ComStack_SignalGroup_Tx_Test, SwitchIpduTxMode_NG_UnknownPduIdHasNoEffect)
{
    /* 実行 (Act) + 評価 (Assert): 戻り値が無い（void）API のため、
     * クラッシュしないこと・既存の状態に影響しないことを確認する。 */
    Com_SwitchIpduTxMode(99U, 1U);
    EXPECT_EQ(Com_Test_GetTmsState(1U), 0U);
    EXPECT_EQ(Com_Test_GetTxPending(1U), 0U);
}


// ------------------------------------------------------------
// Com_InvalidateSignalGroup（SWS_Com_00557/SWS_Com_00645）
// ------------------------------------------------------------
TEST_F(Bsw_ComStack_SignalGroup_Tx_Test, InvalidateSignalGroup_OK_WritesMemberInvalidValueAndCommitsToBuffer)
{
    /* 準備 (Arrange): kTestTmsGroupIPdu（IPduId=1）の唯一のメンバー
     * kTestTmsPendingSignal（SignalId=1、BitPosition=0、BigEndian 1bit＝
     * 0x80）は InvalidValue=1・InvalidValueConfigured=1。 */

    /* 実行 (Act) */
    uint8 ret = Com_InvalidateSignalGroup(1U);

    /* 評価 (Assert): [SWS_Com_00645] 内部で Com_SendSignal()（シャドウ
     * バッファへ書き込み）→ Com_SendSignalGroup()（実バッファへコミット）
     * の順に呼ばれ、メンバーの InvalidValue が実バッファへ反映される。 */
    EXPECT_EQ(ret, E_OK);
    const uint8* buf = Com_Test_GetTxBuffer(1U);
    ASSERT_NE(buf, nullptr);
    EXPECT_EQ(buf[0] & 0x80U, 0x80U);
}


TEST_F(Bsw_ComStack_SignalGroup_Tx_Test, InvalidateSignalGroup_NG_AnyMemberUnconfiguredReturnsServiceNotAvailableWithoutPartialCommit)
{
    /* 準備 (Arrange): kTestErrGroupIPdu（IPduId=3）に、InvalidValueConfigured=1
     * の SignalId=6 と、あえて未設定のままの SignalId=7 の 2 メンバーを設定
     * 済み（ファイル冒頭のシグナル定義参照）。 */

    /* 実行 (Act) */
    uint8 ret = Com_InvalidateSignalGroup(3U);

    /* 評価 (Assert): [SWS_Com_00557] 原文どおり "no ComSignalDataInvalidValue
     * is configured for any of the group signals" は COM_SERVICE_NOT_AVAILABLE
     * （2026-09-20 是正、Com_InvalidateSignal() と同様以前は E_NOT_OK で
     * 代用していた）。1本でも未設定なら all-or-nothing で全体を拒否し、
     * 設定済みの SignalId=6 側も含めて一切バッファへ書き込まない。 */
    EXPECT_EQ(ret, COM_SERVICE_NOT_AVAILABLE);
    const uint8* buf = Com_Test_GetTxBuffer(3U);
    ASSERT_NE(buf, nullptr);
    EXPECT_EQ(buf[0], 0x00U);
}


// ------------------------------------------------------------
// Com_SendSignalGroupArray（SWS_Com_00348 等）
// ------------------------------------------------------------
TEST_F(Bsw_ComStack_SignalGroup_Tx_Test, SendSignalGroupArray_OK_WritesBufferTriggersSendAndSetsUpdateBit)
{
    /* 準備 (Arrange): TMS ビット(bit0=0x80)を立てた生バイト列 */
    uint8_t raw = 0x80U;

    /* 実行 (Act) */
    Std_ReturnType ret = Com_SendSignalGroupArray(1U, &raw);

    /* 評価 (Assert): 個々の Com_SendSignal() を経由しなくても
     * Com_RecalcTms() が Com_TxBuffer を直接読むため TMS 遷移が検出され、
     * 無条件で送信要求・update-bit セットが行われる
     * （Com_SendSignalGroup() は Com_GroupTriggerPending が立っていないと
     * update-bit をセットしないが、本関数は常にセットする——ドキュメント
     * コメント参照）。 */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(Com_Test_GetTxPending(1U), 1U);
    const uint8* buf = Com_Test_GetTxBuffer(1U);
    ASSERT_NE(buf, nullptr);
    EXPECT_EQ(buf[0], 0x81U);  // bit0(TMS, 書き込んだ値) + bit7(update-bit, 自動セット)
}


TEST_F(Bsw_ComStack_SignalGroup_Tx_Test, SendSignalGroupArray_OK_AlwaysTriggersEvenWithoutChange)
{
    /* 準備 (Arrange): Init 直後の値（0x00、TMS=false のまま）と全く同じ
     * 内容を書き込む。上の TmsUnchanged_OK_DoesNotTriggerSendWithoutGroupTrigger
     * （通常経路 Com_SendSignal()+Com_SendSignalGroup()）では、この
     * 「変化なし」ケースは送信要求を立てない。 */
    uint8_t raw = 0x00U;

    /* 実行 (Act) */
    Std_ReturnType ret = Com_SendSignalGroupArray(1U, &raw);

    /* 評価 (Assert): 本関数は個々のシグナルの変化検知を経由しないため、
     * 値が変化していなくても常に送信要求が立つ（ドキュメントコメント
     * 「呼ばれるたびに常に『新しいデータがある』ものとして扱う」の
     * とおり。通常経路との対比が本テストの主張）。 */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(Com_Test_GetTxPending(1U), 1U);
}


TEST_F(Bsw_ComStack_SignalGroup_Tx_Test, SendSignalGroupArray_NG_NullDataPtrReturnsError)
{
    /* 実行 (Act) + 評価 (Assert) */
    EXPECT_EQ(Com_SendSignalGroupArray(1U, NULL), E_NOT_OK);
    EXPECT_EQ(Com_Test_GetTxPending(1U), 0U);  // 何も変化しない
}


TEST_F(Bsw_ComStack_SignalGroup_Tx_Test, SendSignalGroupArray_NG_NonSignalGroupIPduReturnsError)
{
    /* 準備 (Arrange): kTestTxIPdu（IPduId=0）は IsSignalGroup=0 */
    uint8_t raw[2] = { 0x12U, 0x34U };

    /* 実行 (Act) + 評価 (Assert) */
    EXPECT_EQ(Com_SendSignalGroupArray(0U, raw), E_NOT_OK);
    EXPECT_EQ(Com_Test_GetTxPending(0U), 0U);
}


TEST_F(Bsw_ComStack_SignalGroup_Tx_Test, SendSignalGroupArray_OK_SyncsShadowBufferPreventingStaleOverwrite)
{
    /* 準備 (Arrange): 配列APIで一括コミット（個々の Com_SendSignal() は
     * 一切呼ばない） */
    uint8_t raw = 0x80U;  // TMS ビット(bit0)のみ
    ASSERT_EQ(Com_SendSignalGroupArray(1U, &raw), E_OK);
    const uint8* bufAfterArray = Com_Test_GetTxBuffer(1U);
    ASSERT_NE(bufAfterArray, nullptr);
    ASSERT_EQ(bufAfterArray[0], 0x81U);  // TMS ビット + update-bit(自動セット)

    /* 実行 (Act): 通常経路のコミット関数を、個別の Com_SendSignal() を
     * 挟まずそのまま呼ぶ（呼び出し側が API を混在させた状況を再現）。 */
    Std_ReturnType ret = Com_SendSignalGroup(1U);

    /* 評価 (Assert): シャドウバッファが Com_SendSignalGroupArray() 内で
     * 既に同期済みのため、Com_SendSignalGroup() は同じ内容をそのまま
     * 再コミットするだけになり、TMS ビットの値（bit0）が保持される。 */
    EXPECT_EQ(ret, E_OK);
    const uint8* bufAfterGroup = Com_Test_GetTxBuffer(1U);
    ASSERT_NE(bufAfterGroup, nullptr);
    EXPECT_EQ(bufAfterGroup[0] & 0x80U, 0x80U);
}


TEST_F(Bsw_ComStack_SignalGroup_Tx_Test, SendSignalGroupArray_OK_ReturnsServiceNotAvailableWhenGroupStoppedButStillWrites)
{
    /* 準備 (Arrange): kTestErrGroupIPdu は既定で停止状態。 */

    /* 実行 (Act) */
    uint8_t raw = 0xAAU;
    uint8 ret = Com_SendSignalGroupArray(3U, &raw);

    /* 評価 (Assert): [SWS_Com_00334] 停止中でも書き込み自体は行うが、
     * 戻り値は COM_SERVICE_NOT_AVAILABLE。 */
    EXPECT_EQ(ret, COM_SERVICE_NOT_AVAILABLE);
    const uint8* buf = Com_Test_GetTxBuffer(3U);
    ASSERT_NE(buf, nullptr);
    EXPECT_EQ(buf[0], 0xAAU);
}


// ------------------------------------------------------------
// Com_SendSignal（停止中グループのメンバーへの書き込み、SWS_Com_00334）
// ------------------------------------------------------------
TEST_F(Bsw_ComStack_SignalGroup_Tx_Test, SendSignal_OK_ReturnsServiceNotAvailableWhenGroupStoppedButStillWritesShadowBuffer)
{
    /* 準備 (Arrange): kTestErrGroupIPdu は既定で停止状態。SignalId=6 は
     * そのメンバー（BitPosition=0、BitSize=1、big-endian）。 */

    /* 実行 (Act) */
    uint8_t value = 1U;
    uint8 ret = Com_SendSignal(6U, &value);

    /* 評価 (Assert): [SWS_Com_00334] 停止中でも戻り値は
     * COM_SERVICE_NOT_AVAILABLE。シャドウバッファへの書き込み自体は
     * 停止中でも行われることを、Com_SendSignalGroup() でのコミット結果
     * （これも停止中は同じく COM_SERVICE_NOT_AVAILABLE を返す）で確認する。 */
    EXPECT_EQ(ret, COM_SERVICE_NOT_AVAILABLE);

    uint8 groupRet = Com_SendSignalGroup(3U);
    EXPECT_EQ(groupRet, COM_SERVICE_NOT_AVAILABLE);
    const uint8* buf = Com_Test_GetTxBuffer(3U);
    ASSERT_NE(buf, nullptr);
    EXPECT_EQ(buf[0] & 0x80U, 0x80U);  // bit0 (BitPosition=0, big-endian) = MSB
}


// ------------------------------------------------------------
// SWS_Com_00468/SWS_Com_00491: Signal Group の TxAckCbk/TxErrCbk はグループ
// 単位で1回だけ呼ばれる（非グループとの比較を含む）
// ------------------------------------------------------------
TEST_F(Bsw_ComStack_SignalGroup_Tx_Test, TxConfirmation_OK_CallsSignalGroupAckCbkExactlyOnce)
{
    /* 準備 (Arrange): 不要（SetUp() で s_groupTxAckCount は 0 にリセット済み） */

    /* 実行 (Act): IPduId=1（Signal Group）の送信成功を通知する */
    Com_TxConfirmation(1U, E_OK);

    /* 評価 (Assert): メンバー数（このテストでは 1）に関わらず、
     * グループ単位で厳密に 1 回だけ呼ばれる */
    EXPECT_EQ(s_groupTxAckCount, 1U);
}


TEST_F(Bsw_ComStack_SignalGroup_Tx_Test, TxConfirmation_NG_NonGroupIPduDoesNotCallGroupAckCbk)
{
    /* 準備 (Arrange): 不要。IPduId=0 は非 Signal Group（kTestTxIPdu） */

    /* 実行 (Act) */
    Com_TxConfirmation(0U, E_OK);

    /* 評価 (Assert): 無関係な Signal Group（IPduId=1）の TxAckCbk は呼ばれない */
    EXPECT_EQ(s_groupTxAckCount, 0U);
}


TEST_F(Bsw_ComStack_SignalGroup_Tx_Test, IpduGroupStop_OK_CallsSignalGroupErrCbkExactlyOnceWhenUnconfirmed)
{
    /* 準備 (Arrange): 「PduR へは渡した（実送信済み）が Com_TxConfirmation()
     * がまだ届いていない」状態を直接作る（Com_MainFunctionTx()/PduR を経由
     * しないための test-only setter、Com.h 参照）。 */
    Com_Test_SetTxConfPending(3U, 1U);

    /* 実行 (Act): kTestErrGroupIPdu（IPduId=3）が所属する I-PDU Group を
     * 未確認のまま停止する。 */
    Com_IpduGroupStop(kTestStoppableGroupId);

    /* 評価 (Assert): メンバー数に関わらず、グループ単位で厳密に 1 回だけ
     * 呼ばれる。 */
    EXPECT_EQ(s_groupTxErrCount, 1U);
}


TEST_F(Bsw_ComStack_SignalGroup_Tx_Test, IpduGroupStop_NG_DoesNotCallErrCbkWhenAlreadyConfirmed)
{
    /* 準備 (Arrange): 不要。「送信済み・未確認」状態を一切作らない
     * （Com_TxConfPending は Com_Init() で 0 のまま）。 */

    /* 実行 (Act) */
    Com_IpduGroupStop(kTestStoppableGroupId);

    /* 評価 (Assert): 未確認の送信が無いため TxErrCbk は呼ばれない。 */
    EXPECT_EQ(s_groupTxErrCount, 0U);
}


// ------------------------------------------------------------
// Com_IpduGroupStop の再送シーケンス/送信デッドライン監視への影響
// ------------------------------------------------------------
TEST_F(Bsw_ComStack_SignalGroup_Tx_Test, IpduGroupStopRepeatsRemaining_OK_ClearsRepeatsRemaining)
{
    /* 準備 (Arrange): 再送シーケンス進行中の状態を、実際に NumberOfRepetitions
     * を設定した停止可能グループの I-PDU を新規に用意しなくても、test-only
     * setter で直接作る（kTestErrGroupIPdu/kTestStoppableGroupId を流用）。 */
    Com_Test_SetTxRepeatsRemaining(3U, 2U);

    /* 実行 (Act): [SWS_Com_00392] I-PDU Group の停止は再送シーケンスも
     * キャンセルする */
    Com_IpduGroupStop(kTestStoppableGroupId);

    /* 評価 (Assert) */
    EXPECT_EQ(Com_Test_GetTxRepeatsRemaining(3U), 0U);
}


TEST_F(Bsw_ComStack_SignalGroup_Tx_Test, IpduGroupStopTxTOutInteraction_OK_PreventsDoubleFireWithTxErrCbk)
{
    /* 準備 (Arrange): TxTOutGroupLevel_OK_FiresWhenStartedAndOverdue と同じ
     * 「送信済み・未確認のまま閾値超過」状態を作るが、Com_MainFunctionTx() で
     * 評価される前に Com_IpduGroupStop() を先に呼ぶ。 */
    Com_IpduGroupStart(kTestStoppableGroupId, 0U);
    Com_Test_SetTxConfPending(3U, 1U);
    Com_Test_SetTxConfPendingSinceMs(3U, FakeMillis_Value);
    FakeMillis_Value += 101U;

    /* 実行 (Act) */
    Com_IpduGroupStop(kTestStoppableGroupId);  // TxErrCbk が発火、Started=0 に
    Com_MainFunctionTx();  // Com_TxIPduStarted[3]==0 のため監視ループ自体が対象外

    /* 評価 (Assert): TxErrCbk は発火するが、TxTOutCbk とは二重発火しない */
    EXPECT_EQ(s_groupTxErrCount, 1U);
    EXPECT_EQ(s_groupTxTOutCount, 0U);
}


// ------------------------------------------------------------
// Com_CbkTxTOut のグループ単位経路（SWS_Com_00878）
// ------------------------------------------------------------
TEST_F(Bsw_ComStack_SignalGroup_Tx_Test, TxTOutGroupLevel_OK_FiresWhenStartedAndOverdue)
{
    /* 準備 (Arrange): kTestErrGroupIPdu（IPduId=3、Signal Group、
     * TxFirstTimeoutMs=100U）を起動し、test-only setter で
     * 「送信済み・未確認」状態を直接注入する（実際に Com_MainFunctionTx()/PduR
     * を経由させる配線は用意していないため）。 */
    Com_IpduGroupStart(kTestStoppableGroupId, 0U);
    Com_Test_SetTxConfPending(3U, 1U);
    Com_Test_SetTxConfPendingSinceMs(3U, FakeMillis_Value);

    /* 実行 (Act): TxFirstTimeoutMs(100) を超過させる */
    FakeMillis_Value += 101U;
    Com_MainFunctionTx();

    /* 評価 (Assert): グループ単位で発火する。TxErrCbk とは無関係 */
    EXPECT_EQ(s_groupTxTOutCount, 1U);
    EXPECT_EQ(s_groupTxErrCount, 0U);
}


// ------------------------------------------------------------
// Com_TriggerIPDUSend（SWS_Com_00861/SWS_Com_00388、停止可能グループ側）
// ------------------------------------------------------------
TEST_F(Bsw_ComStack_SignalGroup_Tx_Test, TriggerIPDUSend_OK_RespectsMinDelayTimeAndDispatchesOnceElapsed)
{
    /* 準備 (Arrange): kTestErrGroupIPdu（IPduId=3）は IpduGroupId=
     * kTestStoppableGroupId を持つため、Com_Init() 直後は既定で停止状態
     * （[SWS_Com_00444]/[SWS_Com_00840]、Com_Init() の実装コメント参照）。
     * 明示的に起動してから使う（TxTOut_OK_GroupLevelFiresWhenStartedAndOverdue
     * と同じ手順）。MinDelayMs=100U。Com_IpduGroupStart() が
     * Com_TxLastSentMs[3] を現在時刻にリセットするため、経過時間はここから
     * 0 スタートになる。 */
    Com_IpduGroupStart(kTestStoppableGroupId, 0U);

    /* 実行 (Act 1): トリガーは受け付けるが、MDT 未経過のため今回は送信しない
     * （[SWS_Com_00388] "shall postpone transmissions if necessary"）。 */
    ASSERT_EQ(Com_TriggerIPDUSend(3U), E_OK);
    Com_MainFunctionTx();

    /* 評価 (Assert 1): トリガーは破棄されず、次回以降のために保持される */
    EXPECT_EQ(Com_Test_GetTxTriggerPending(3U), 1U);

    /* 実行 (Act 2): MDT 経過後に再度 Com_MainFunctionTx() を呼ぶ */
    FakeMillis_Value += 100U;
    Com_MainFunctionTx();

    /* 評価 (Assert 2): MDT 経過により消費される（実際の PduR ルートは
     * 未登録のため CAN 送信までは進まないが、ディスパッチ自体は試行される
     * ことをフラグのクリアで確認する） */
    EXPECT_EQ(Com_Test_GetTxTriggerPending(3U), 0U);
}


TEST_F(Bsw_ComStack_SignalGroup_Tx_Test, TriggerIPDUSend_NG_StoppedIpduReturnsErrorWithoutTriggering)
{
    /* 準備 (Arrange): kTestErrGroupIPdu（IPduId=3）は IpduGroupId=
     * kTestStoppableGroupId を持つため、Com_Init() 直後は既定で停止状態
     * （[SWS_Com_00444]/[SWS_Com_00840]）。Com_IpduGroupStart() を一度も
     * 呼ばないことで「起動されたことがない」状態を明示的に表す
     * （念のため Com_IpduGroupStop() も呼び、Start 後に Stop された場合と
     * 同じ経路であることも合わせて確認する）。 */
    Com_IpduGroupStop(kTestStoppableGroupId);

    /* 実行 (Act) + 評価 (Assert): [SWS_Com_00861] stopped I-PDU は E_NOT_OK。
     * トリガー自体も記録されない（後で started になっても自動実行されない）。 */
    EXPECT_EQ(Com_TriggerIPDUSend(3U), E_NOT_OK);
    EXPECT_EQ(Com_Test_GetTxTriggerPending(3U), 0U);
}


}  // namespace
