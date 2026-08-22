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
 *
 *          `CanIf_RxIndication()` は無条件に `CanSM_RxIndication()` を呼ぶ
 *          （CanIf.c 参照）。同じ `[env:native_chain]` は CanSM.c の実体を
 *          リンクしている（Bsw_SleepChain_test.cpp / Bsw_WakeupChain_test.cpp
 *          参照）ため、本テストの SetUp()/TearDown() でも CanSM_Init()/
 *          CanSM_DeInit() を呼ぶ（呼ばないと毎回 DET_E_UNINIT が報告される）。
 *          本テストは CanSM_State を WAKEUP_VALIDATING にしないため、
 *          CanSM_RxIndication() 自体は何もしない no-op として通過するだけ。
 */
#include <gtest/gtest.h>

extern "C" {
#include "Com.h"
#include "PduR.h"
#include "CanIf.h"
#include "CanSM.h"
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
// PduRId=0、と1本のパスだけを通す。中心となる2テスト（末尾）はこの
// IPduId=0 のみを使う。IPduId=1/2 は SWS_Com_00555（Com_CbkRxAck）専用の
// 追加 I-PDU で、CanIf/PduR 側にルーティングは設定していない
// （Bsw_TxChain_test.cpp の TMS/TxAckCbk/TxErrCbk テスト群と同じ理由で、
// Com_RxIndication() を直接呼ぶ形で検証する）。
// -----------------------------------------------------------------------

// SWS_Com_00555 検証用カウンタ・コールバック。
static uint8_t s_rxAckCount = 0U;
static void TestRxAckCbk(void) { s_rxAckCount++; }

static uint8_t s_groupRxAckCount = 0U;
static void TestGroupRxAckCbk(void) { s_groupRxAckCount++; }

static uint8_t s_partialAckCount0 = 0U;
static void TestPartialAckCbk0(void) { s_partialAckCount0++; }

static uint8_t s_partialAckCount1 = 0U;
static void TestPartialAckCbk1(void) { s_partialAckCount1++; }

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
    /* TxErrCbk */                 NULL,
    /* RxAckCbk */                 TestRxAckCbk
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

// -----------------------------------------------------------------------
// SWS_Com_00555 の Signal Group 側経路（Com_RxIndication() 内、シグナル単位
// デッドライン監視リセットループへ入る前のグループ単位分岐）用。メンバーが
// 2本でもグループ単位で1回だけ呼ばれることを検証する。
// -----------------------------------------------------------------------
const Com_SignalConfigType kTestRxGroupSignalA = {
    /* SignalId */                1U,
    /* Direction */                COM_SIGNAL_DIRECTION_RX,
    /* IPduId */                   1U,
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
    /* InvalidValue */             0U,
    /* InvalidNotificationCbk */   NULL,
    /* FirstTimeoutMs */           0U,
    /* TimeoutMs */                0U,
    /* TimeoutNotificationCbk */   NULL,
    /* TxAckCbk */                 NULL,
    /* TxErrCbk */                 NULL,
    /* RxAckCbk */                 NULL  // Signal Group メンバーには設定しない（呼ばれないことの裏付け）
};

const Com_SignalConfigType kTestRxGroupSignalB = {
    /* SignalId */                2U,
    /* Direction */                COM_SIGNAL_DIRECTION_RX,
    /* IPduId */                   1U,
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
    /* TimeoutNotificationCbk */   NULL,
    /* TxAckCbk */                 NULL,
    /* TxErrCbk */                 NULL,
    /* RxAckCbk */                 NULL
};

const Com_IPduConfigType kTestRxGroupIPdu = {
    /* IPduId */           1U,
    /* DLC */              1U,
    /* PduRId */           1U,  // 本テストは Com_MainFunction()/CanIf/PduR まで進めないため未使用
    /* FirstTimeoutMs */   0U,
    /* TimeoutMs */        0U,
    /* IsSignalGroup */    1U,
    /* TxModeMode */       COM_TX_MODE_DIRECT,  /* RX I-PDU では未使用 */
    /* TxPeriodMs */       0U,
    /* TxModeModeTrue */   COM_TX_MODE_DIRECT,
    /* TxPeriodMsTrue */   0U,
    /* MinDelayMs */       0U,
    /* UpdateBitPosition */ 0xFFU,
    /* IpduGroupId */      COM_IPDU_GROUP_NONE,
    /* RxIndicationCbk */  NULL,
    /* TxTransformCbk */   NULL,
    /* TxAckCbk */         NULL,
    /* TxErrCbk */         NULL,
    /* RxAckCbk */         TestGroupRxAckCbk
};

// -----------------------------------------------------------------------
// SWS_Com_00574 の部分受信ゲーティング（lastByte <= recvLen）検証用。
// 非 Signal Group の I-PDU（DLC=2）に byte0/byte1 それぞれ専用のシグナルを
// 置き、SduLength=1（byte0 のみ受信）で呼んだときに byte0 側のみ RxAckCbk
// が発火することを確認する。部分受信自体は実機では到達しない経路だが
// （CanIf の DLC が常に Com の DLC と同値のため）、Com_RxIndication() を
// 直接呼ぶユニットテストなら CanIf の制約を経由せず検証できる。
// -----------------------------------------------------------------------
const Com_SignalConfigType kTestPartialSignalByte0 = {
    /* SignalId */                3U,
    /* Direction */                COM_SIGNAL_DIRECTION_RX,
    /* IPduId */                   2U,
    /* BitPosition */              0U,
    /* BitSize */                  8U,
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
    /* TxErrCbk */                 NULL,
    /* RxAckCbk */                 TestPartialAckCbk0
};

const Com_SignalConfigType kTestPartialSignalByte1 = {
    /* SignalId */                4U,
    /* Direction */                COM_SIGNAL_DIRECTION_RX,
    /* IPduId */                   2U,
    /* BitPosition */              8U,
    /* BitSize */                  8U,
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
    /* TxErrCbk */                 NULL,
    /* RxAckCbk */                 TestPartialAckCbk1
};

const Com_IPduConfigType kTestPartialIPdu = {
    /* IPduId */           2U,
    /* DLC */              2U,
    /* PduRId */           2U,  // 本テストは Com_MainFunction()/CanIf/PduR まで進めないため未使用
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

const Com_SignalConfigType kTestRxSignals[] = {
    kTestRxSignal, kTestRxGroupSignalA, kTestRxGroupSignalB,
    kTestPartialSignalByte0, kTestPartialSignalByte1
};
const Com_IPduConfigType kTestRxIPdus[] = { kTestRxIPdu, kTestRxGroupIPdu, kTestPartialIPdu };

const Com_ConfigType kTestComRxConfig = {
    /* RxIPdus */       kTestRxIPdus,
    /* RxIPduCount */   3U,
    /* TxIPdus */       NULL,
    /* TxIPduCount */   0U,
    /* Signals */       kTestRxSignals,
    /* SignalCount */   5U,
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
        s_rxAckCount = 0U;
        s_groupRxAckCount = 0U;
        s_partialAckCount0 = 0U;
        s_partialAckCount1 = 0U;
        // CanIf_RxIndication() は無条件に CanSM_RxIndication() を呼ぶため
        // （CanIf.c 参照）、CanSM 未初期化のままだと毎回 DET_E_UNINIT が
        // 報告されてしまう。本テストは CanSM_State を FULL_COM/NO_COM のまま
        // （WAKEUP_VALIDATING にしない）保つため、CanSM 自体は何もしない
        // no-op として通過するだけになる。
        CanSM_Init(NULL);

        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;  // DeInit() のログを抑制
        CanSM_DeInit();
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

// ------------------------------------------------------------
// SWS_Com_00555（Com_CbkRxAck）の回帰テスト。Com_MainFunction_Read()/CanIf/
// PduR は経由せず、Com_RxIndication() を直接呼ぶ（ファイル冒頭コメント参照）。
// ------------------------------------------------------------
TEST_F(Bsw_RxChain_Test, ComRxIndication_OK_NonGroupAck_FiresOnFullReception)
{
    /* 準備 (Arrange): IPduId=0 を DLC 分フルで受信 */
    uint8 buf[2] = { 0x12U, 0x34U };
    PduInfoType pduInfo = { buf, 2U };

    /* 実行 (Act) */
    Com_RxIndication(0U, &pduInfo);

    /* 評価 (Assert) */
    EXPECT_EQ(s_rxAckCount, 1U);
}

TEST_F(Bsw_RxChain_Test, ComRxIndication_OK_NonGroupAck_FiresOncePerFrame)
{
    /* 準備 (Arrange) */
    uint8 buf[2] = { 0x12U, 0x34U };
    PduInfoType pduInfo = { buf, 2U };

    /* 実行 (Act): 2フレーム受信 */
    Com_RxIndication(0U, &pduInfo);
    Com_RxIndication(0U, &pduInfo);

    /* 評価 (Assert): フレーム数と同じ回数だけ呼ばれる */
    EXPECT_EQ(s_rxAckCount, 2U);
}

TEST_F(Bsw_RxChain_Test, ComRxIndication_OK_GroupAck_FiresOnceRegardlessOfMemberCount)
{
    /* 準備 (Arrange): IPduId=1（Signal Group、メンバー2本）をフル DLC で受信 */
    uint8 buf[1] = { 0x03U };
    PduInfoType pduInfo = { buf, 1U };

    /* 実行 (Act) */
    Com_RxIndication(1U, &pduInfo);

    /* 評価 (Assert): メンバー数（2）に関わらず、グループ単位で厳密に1回 */
    EXPECT_EQ(s_groupRxAckCount, 1U);
}

TEST_F(Bsw_RxChain_Test, ComRxIndication_NG_GroupAck_DoesNotFireOnShortFrameDiscard)
{
    /* 準備 (Arrange): IPduId=1 の DLC(1) 未満の受信長
     * （[SWS_Com_00575] によりグループ全体が不採用になる） */
    uint8 buf[1] = { 0x00U };
    PduInfoType pduInfo = { buf, 0U };

    /* 実行 (Act) */
    Com_RxIndication(1U, &pduInfo);

    /* 評価 (Assert): バッファへ格納されていないため RxAckCbk も呼ばれない */
    EXPECT_EQ(s_groupRxAckCount, 0U);
}

TEST_F(Bsw_RxChain_Test, ComRxIndication_OK_PartialReception_OnlyAcksSignalsWithinRecvLen)
{
    /* 準備 (Arrange): IPduId=2（DLC=2、byte0/byte1 それぞれ専用シグナル）を
     * byte0 のみ（SduLength=1）で受信する部分受信シナリオ */
    uint8 buf[1] = { 0xABU };
    PduInfoType pduInfo = { buf, 1U };

    /* 実行 (Act) */
    Com_RxIndication(2U, &pduInfo);

    /* 評価 (Assert): recvLen(1) 以内に収まる byte0 側のみ RxAckCbk が発火し、
     * 範囲外の byte1 側は発火しない（[SWS_Com_00574]）。 */
    EXPECT_EQ(s_partialAckCount0, 1U);
    EXPECT_EQ(s_partialAckCount1, 0U);
}

}  // namespace
