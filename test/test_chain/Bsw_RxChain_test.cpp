/**
 * \file    Bsw_RxChain_test.cpp
 * \brief   README.md「Rx 処理（Can → CanIf → PduR → Com の順）」コールチェーンの
 *          単体テスト（GoogleTest / PlatformIO `[env:native_chain]`）。
 *
 * \details README の該当コールチェーン図：
 *
 *              Can_Isr()                        ← 真の割り込み。ペンディングフラグを立てるだけ
 *                ┊  (Can_RxIrqPending 経由。次回 Os スケジューラ tick まで非同期に待機)
 *                ↓
 *              Can_MainFunction_Read()          ← フラグをドレイン、SPI 読み出し
 *                → CanIf_RxIndication()         ← CAN ID → PduId へ変換
 *                  → PduR_CanIfRxIndication() (= PduR_ComRxIndication())
 *                    → Com_RxIndication()       ← マルチキャスト先の1つ（本テストではこれのみ設定）
 *
 *          Tx処理コールチェーン（Bsw_TxChain_test.cpp）と同じ発想で「非同期の
 *          切れ目で2セグメントに分ける」を試みたが、Rx処理では以下の理由で
 *          1セグメントにまとめている（これ自体もコールチェーンの理解の一部）:
 *
 *            - `Can_Isr()` は `Can.c` 内の `static` 関数であり（`Can.h` に
 *              一切宣言がない）、テストコードから直接呼び出せない
 *              （`Can_Hw_AttachRxIsr()` 経由で HAL 層にコールバック登録される
 *              のみ）。
 *            - `Can_MainFunction_Read()` 自身も、実は `Can_RxIrqPending` の
 *              有無に関わらず無条件に `Can_Hw_CheckReceive()` をポーリングする
 *              設計になっている（`Can.c` 冒頭のコメント参照: 実機で
 *              `attachInterrupt` が初回発火しなかった経緯を踏まえた、
 *              意図的な二重防御）。つまり Tx処理の `Com_TxPending` のように
 *              「フラグが立っていることが後続処理の前提条件」ではなく、
 *              フラグは単なる最適化目的（早期 return）に留まる。
 *
 *          したがって本テストは、非同期境界の「向こう側」である
 *          `Can_MainFunction_Read()` を起点とする1つのコールチェーンとして
 *          検証する（フェイクの `Can_Hw` に受信フレームを積んでおき、
 *          最終的に `Com_ReceiveSignal()` で正しい値が取得できることを確認する）。
 *
 *          Bsw_TxChain_test.cpp と同じ理由・同じ最小構成方針（本番の
 *          `*_PBCfg.c` は使わず、1シグナル・1 I-PDU のみのテスト専用設定を
 *          本ファイル内で定義する）。CanTp_RxIndication/SecOC_IfRxIndication
 *          へのマルチキャストは対象外（Com_RxIndication のみを転送先とする）。
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
// テスト専用の最小 Com/PduR/CanIf 設定（Bsw_TxChain_test.cpp と同じ方針）。
// SignalId=0 (RX, 16bit BigEndian) 1本だけを持つ IPduId=0 の RX I-PDU。
// CanIf の RxPduId=0（CAN ID=0x100, Hrh=0）→ PduR の SrcPduId=0 → Com の
// PduRId=0、と1本のパスだけを通す。
// -----------------------------------------------------------------------

const Com_SignalConfigType kTestRxSignal = {
    /* SignalId */                0U,
    /* Direction */                COM_SIGNAL_DIRECTION_RX,
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

const Com_IPduConfigType kTestRxIPdu = {
    /* IPduId */           0U,
    /* DLC */              2U,
    /* PduRId */           0U,
    /* FirstTimeoutMs */   0U,
    /* TimeoutMs */        0U,
    /* IsSignalGroup */    0U,
    /* TxModeMode */       COM_TX_MODE_DIRECT,  /* RX I-PDU では未使用 */
    /* TxPeriodMs */       0U,
    /* TxModeModeTrue */   COM_TX_MODE_DIRECT,
    /* TxPeriodMsTrue */   0U,
    /* MinDelayMs */       0U,
    /* UpdateBitPosition */ 0xFFU,
    /* IpduGroupId */      COM_IPDU_GROUP_NONE,
    /* RxIndicationCbk */  NULL,
    /* TxTransformCbk */   NULL
};

const Com_ConfigType kTestComRxConfig = {
    /* RxIPdus */       &kTestRxIPdu,
    /* RxIPduCount */   1U,
    /* TxIPdus */       NULL,
    /* TxIPduCount */   0U,
    /* Signals */       &kTestRxSignal,
    /* SignalCount */   1U,
    /* GwMappings */    NULL,
    /* GwMappingCount */ 0U
};

const PduR_RxDestType kTestPduRRxDest = {
    /* Module */    PDUR_MODULE_COM,
    /* DestPduId */ 0U,
    /* RxIndFct */  Com_RxIndication
};

const PduR_RxRoutingPathType kTestPduRRxPath = {
    /* SrcPduId */  0U,
    /* Dests */     &kTestPduRRxDest,
    /* DestCount */ 1U
};

const PduR_PBConfigType kTestPduRRxConfig = {
    /* RxPaths */     &kTestPduRRxPath,
    /* RxPathCount */ 1U,
    /* TxPaths */     NULL,
    /* TxPathCount */ 0U
};

const CanIf_RxPduConfigType kTestCanIfRxPdu = {
    /* CanId */             0x100U,
    /* Hrh */               0U,  /* Can_MainFunction_Read() が構築する Mailbox は常に Hoh=0 */
    /* UpperLayerRxPduId */ 0U,
    /* Dlc */               2U,
    /* RxIndicationFct */   PduR_ComRxIndication  /* = PduR_CanIfRxIndication（#define エイリアス） */
};

const CanIf_ConfigType kTestCanIfRxConfig = {
    /* TxPduConfig */ NULL,
    /* TxPduCount */  0U,
    /* RxPduConfig */ &kTestCanIfRxPdu,
    /* RxPduCount */  1U
};

class Bsw_RxChain_Test : public ::testing::Test
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
        CanIf_Init(&kTestCanIfRxConfig);
        PduR_Init(&kTestPduRRxConfig);
        Com_Init(&kTestComRxConfig);

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
// Can_MainFunction_Read() ─ Can_Hw から Com_ReceiveSignal() まで
// （ファイル冒頭のコメントの通り、Can_Isr() 側は非同期境界だが
//  static かつ MainFunction_Read が依存しないため単独では検証しない）
// ------------------------------------------------------------
TEST_F(Bsw_RxChain_Test, CanMainFunctionRead_OK_DrivesChainToComReceiveSignal)
{
    /* 準備 (Arrange): フェイク Can_Hw に受信フレーム1件を積む */
    FakeCanHw_RxPendingCount = 1U;
    FakeCanHw_RxId  = 0x100U;
    FakeCanHw_RxDlc = 2U;
    FakeCanHw_RxData[0] = 0x56U;
    FakeCanHw_RxData[1] = 0x78U;

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert) */
    EXPECT_EQ(FakeCanHw_RxPendingCount, 0U);  // ドレインし尽くしたこと
    uint16_t value = 0U;
    uint8 ret = Com_ReceiveSignal(0U, &value);
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(value, 0x5678U);  // BigEndian: byte[0]=MSB
}

TEST_F(Bsw_RxChain_Test, CanMainFunctionRead_NG_NothingReceived_LeavesInitValue)
{
    /* 準備 (Arrange): 受信フレームなし */
    FakeCanHw_RxPendingCount = 0U;

    /* 実行 (Act) */
    Can_MainFunction_Read();

    /* 評価 (Assert): Com_SignalConfigType.InitValue（既定 0）のまま */
    uint16_t value = 0xFFFFU;  // 上書きされていないことが分かるよう非0で初期化
    uint8 ret = Com_ReceiveSignal(0U, &value);
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(value, 0U);
}

}  // namespace
