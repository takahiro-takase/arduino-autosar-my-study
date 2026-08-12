/**
 * \file    Bsw_TxChain_test.cpp
 * \brief   README.md「Tx 処理（Com → PduR → CanIf → Can の順）」コールチェーンの
 *          単体テスト（GoogleTest / PlatformIO `[env:native_chain]`）。
 *
 * \details 本テストは単一モジュールの検証ではなく、README の以下のコールチェーン
 *          図をそのまま実行して確認するもの：
 *
 *              Com_SendSignal()/Com_SendSignalGroup()   ← ASW から呼ばれる
 *                ┊  (Com_TxPending 経由。次回 Com_MainFunction() まで非同期に待機)
 *              Com_MainFunction()
 *                → PduR_Transmit() → CanIf_Transmit() → Can_Write()
 *                  （SPI 送信完了までここで同期完了）
 *
 *          図中の「┊」（Com_TxPending というキュー経由の非同期の切れ目）を境に、
 *          テストを2つのセグメントへ分けている。キューで切れる箇所を無理に
 *          1つのテストで跨がず、そこで終わりとすることで、それぞれが
 *          「そこまでで何が保証されるか」を単独で・個別に実行可能な形で示す
 *          （`--gtest_filter=Bsw_TxChain_Test.ComSendSignal_*` 等で絞り込み可能）：
 *
 *            セグメント①: Com_SendSignal() が Com_TxPending をセットし、値を
 *                         TX バッファへ正しく pack することを検証し、そこで終わる
 *                         （Com_MainFunction() は呼ばない）。
 *            セグメント②: セグメント①の終端状態（Com_TxPending が立った状態）を
 *                         Arrange で用意し、Com_MainFunction() が
 *                         PduR_Transmit()→CanIf_Transmit()→Can_Write() と同期連鎖し、
 *                         最終的に Can_Hw（フェイク）へ正しい CAN ID/DLC/データで
 *                         送信要求が届くことまで検証する。
 *
 *          本番の `Com_PBCfg.c` 等は TxAckCbk/TxTransformCbk が `Rte_*` 関数を
 *          直接参照するため、リンクすると Rte.c 経由で Dem/WdgM/FiM... まで
 *          巨大な依存グラフを引き込んでしまう。ここではコールチェーンという
 *          「機構」の理解・検証に絞るため、1シグナル・1 I-PDU のみを持つ
 *          テスト専用の最小 Com/PduR/CanIf 設定を本ファイル内で定義し、
 *          Com.c/PduR.c/CanIf.c/Can.c は実体をリンクする（フェイクは
 *          `Can_Hw` 層のみ、`test/test_chain/Hal_Can_Hw_fake.c` を使う）。
 *
 *          [env:native]（test/test_native/）は Can.c 単体を CanIf フェイクで
 *          隔離して検証しており、同じバイナリに CanIf.c の本物を混在させると
 *          シンボル多重定義になる。そのため本テストは env（＝ビルド
 *          ディレクトリ・バイナリ）そのものを分けた `[env:native_chain]`
 *          （このファイルが属する `test/test_chain/`）で実行する
 *          （`pio test -e native_chain`）。
 *
 *          CanIf.c は CanSM_RxIndication()/ControllerBusOff()/
 *          ControllerWakeup() をハードコードで呼ぶため、
 *          `Bsw_CanSM_fake.c`（no-op スタブ）でリンクを満たしている
 *          （CanSM 自身のロジックは README「ECU管理層」の別のコールチェーン
 *          であり、本テストの対象外）。
 */
#include <gtest/gtest.h>

extern "C" {
#include "Com.h"
#include "PduR.h"
#include "CanIf.h"
#include "Can.h"
#include "Can_Hw.h"
#include "Hal_Can_Hw_fake.h"
#include "Hal_Det_Hw_fake.h"
}

namespace
{

// -----------------------------------------------------------------------
// テスト専用の最小 Com/PduR/CanIf 設定（本番の *_PBCfg.c は使わない。
// ファイル冒頭のコメント参照）。
// SignalId=0 (TX, 16bit BigEndian) 1本だけを持つ IPduId=0 の TX I-PDU。
// PduR は SrcPduId=0 を CanIfTxPduId=0 へ直結。CanIf の TxPduId=0 は
// CAN ID=0x100, DLC=2 で Can_Write(Hth=0, ...) を呼ぶ。
// -----------------------------------------------------------------------

const Com_SignalConfigType kTestSignal = {
    /* SignalId */                0U,
    /* Direction */                COM_SIGNAL_DIRECTION_TX,
    /* IPduId */                   0U,
    /* BitPosition */              0U,
    /* BitSize */                  16U,
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
    /* TimeoutNotificationCbk */   NULL,
    /* TxAckCbk */                 NULL,
    /* TxErrCbk */                 NULL
};

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

const Com_ConfigType kTestComConfig = {
    /* RxIPdus */       NULL,
    /* RxIPduCount */   0U,
    /* TxIPdus */       &kTestTxIPdu,
    /* TxIPduCount */   1U,
    /* Signals */       &kTestSignal,
    /* SignalCount */   1U,
    /* GwMappings */    NULL,
    /* GwMappingCount */ 0U
};

const PduR_TxRoutingPathType kTestPduRTxPath = {
    /* SrcPduId */             0U,
    /* CanIfTxPduId */         0U,
    /* ConfDestPduId */        0U,
    /* ConfFct */              NULL,
    /* TransmitOverrideFct */  NULL,
    /* TransmitOverrideId */   0U
};

const PduR_PBConfigType kTestPduRConfig = {
    /* RxPaths */     NULL,
    /* RxPathCount */ 0U,
    /* TxPaths */     &kTestPduRTxPath,
    /* TxPathCount */ 1U
};

const CanIf_TxPduConfigType kTestCanIfTxPdu = {
    /* UpperLayerTxPduId */ 0U,
    /* CanId */             0x100U,
    /* Dlc */               2U,
    /* Hth */               0U,
    /* TxConfirmFct */      NULL
};

const CanIf_ConfigType kTestCanIfConfig = {
    /* TxPduConfig */ &kTestCanIfTxPdu,
    /* TxPduCount */  1U,
    /* RxPduConfig */ NULL,
    /* RxPduCount */  0U
};

class Bsw_TxChain_Test : public ::testing::Test
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
        Can_SetControllerMode(0U, CAN_T_START);
        CanIf_Init(&kTestCanIfConfig);
        PduR_Init(&kTestPduRConfig);
        Com_Init(&kTestComConfig);

        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;  // DeInit() のログを抑制
        Com_DeInit();
        CanIf_DeInit();
    }

    Can_ConfigType canConfig;
};

// ------------------------------------------------------------
// セグメント①: Com_SendSignal() ─ Com_TxPending というキューで切れるまで
// ------------------------------------------------------------
TEST_F(Bsw_TxChain_Test, ComSendSignal_OK_SetsPendingAndPacksBuffer)
{
    /* 準備 (Arrange) */
    uint16_t value = 0x1234U;

    /* 実行 (Act) */
    uint8 ret = Com_SendSignal(0U, &value);

    /* 評価 (Assert) */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(Com_Test_GetTxPending(0U), 1U);
    const uint8* buf = Com_Test_GetTxBuffer(0U);
    ASSERT_NE(buf, nullptr);
    EXPECT_EQ(buf[0], 0x12U);  // BigEndian: bit0(MSB)側が byte[0]
    EXPECT_EQ(buf[1], 0x34U);
}

TEST_F(Bsw_TxChain_Test, ComSendSignal_NG_UnknownSignalId_DoesNotSetPending)
{
    /* 準備 (Arrange) */
    uint16_t value = 0x1234U;

    /* 実行 (Act) */
    uint8 ret = Com_SendSignal(99U, &value);  // 設定に存在しない SignalId

    /* 評価 (Assert) */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(Com_Test_GetTxPending(0U), 0U);
}

// ------------------------------------------------------------
// セグメント②: Com_MainFunction() ─ キューの続きから Can_Hw まで
// ------------------------------------------------------------
TEST_F(Bsw_TxChain_Test, ComMainFunction_OK_DrivesChainToCanHwSend)
{
    /* 準備 (Arrange): セグメント①の終端状態（Com_TxPending が立った状態）を用意する */
    uint16_t value = 0x1234U;
    Com_SendSignal(0U, &value);
    ASSERT_EQ(Com_Test_GetTxPending(0U), 1U);

    /* 実行 (Act) */
    Com_MainFunction();

    /* 評価 (Assert) */
    EXPECT_EQ(Com_Test_GetTxPending(0U), 0U);  // 送信要求が消費された
    EXPECT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x100U);   // CanIf_TxPduConfigType.CanId
    EXPECT_EQ(FakeCanHw_LastSendDlc, 2U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], 0x12U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x34U);
}

TEST_F(Bsw_TxChain_Test, ComMainFunction_NG_NothingPending_DoesNotReachCanHw)
{
    /* 準備 (Arrange): Com_SendSignal() を呼ばない（Com_TxPending が立っていない） */

    /* 実行 (Act) */
    Com_MainFunction();

    /* 評価 (Assert) */
    EXPECT_EQ(FakeCanHw_SendCount, 0U);
}

}  // namespace
