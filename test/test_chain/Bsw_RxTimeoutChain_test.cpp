/**
 * \file    Bsw_RxTimeoutChain_test.cpp
 * \brief   README.md「Rx 処理」の「デッドライン監視（受信タイムアウト）」
 *          コールチェーンの単体テスト（GoogleTest / PlatformIO `[env:native_chain]`）。
 *
 * \details README の該当コールチェーン図：
 *
 *              [100ms 周期タスク] Os_SchedulerStep() → Com_MainFunction()
 *                → (now - Com_RxLastMs[id]) がしきい値以上なら
 *                    Com_SigTimedOut[s] を立てる（WARN ログ、ここが検知点）
 *                ┊  (Com_SigTimedOut というフラグ経由。次に Com_ReceiveSignal()
 *                ┊   が呼ばれるまで非同期に待機)
 *                ↓
 *              Com_ReceiveSignal()               ← Rte 等から呼ばれる（同期）
 *                → RxDataTimeoutAction に応じて返す値を決定
 *                    SUBSTITUTE : TimeoutSubstitutionValue で置換
 *                    REPLACE    : InitValue で置換
 *                    NONE       : E_NOT_OK（既定、呼び出し元は自分の初期値を使う）
 *
 *          Tx 処理コールチェーン（Bsw_TxChain_test.cpp）の `Com_TxPending` と
 *          構造は同じ「立てる側／読む側が別々のタイミングで動く」非同期境界だが、
 *          向きが逆になっている: TX は「on-demand 呼び出し（Com_SendSignal）が
 *          立てて、周期タスク（Com_MainFunction）が読む」のに対し、こちらは
 *          「周期タスク（Com_MainFunction）が立てて、on-demand 呼び出し
 *          （Com_ReceiveSignal）が読む」。この非同期境界でテストを2つの
 *          セグメントに分け、それぞれ個別に実行可能な `TEST_F` ケースとしている
 *          （`--gtest_filter=Bsw_RxTimeoutChain_Test.ComMainFunction_*` 等で
 *          絞り込み可）。
 *
 *          この非同期境界の検知はどちらも Com.c 内で完結する（PduR/CanIf/Can/
 *          CanSM は経由しない）ため、Tx/Rx チェーンと異なりフェイクは
 *          `millis()`（`test/test_chain/Hal_Millis_fake.c`）のみで足りる。
 *          `Com_RxIndication()` を直接呼んで受信済み状態を作ってから
 *          `FakeMillis_Value` を進める、という手順で「通信していたが途絶えた」
 *          状況を再現する。
 *
 *          `Com_SigTimedOut[]` はテスト専用アクセサ `Com_Test_GetSigTimedOut()`
 *          （`COM_UNIT_TEST` 定義時のみ、`Com.h`/`Com.c` 参照）で直接観測する。
 *          セグメント②は Tx チェーンの `ComMainFunction_OK_...` と同様、
 *          セグメント①の終端状態（フラグが立った状態）を Arrange で
 *          `Com_RxIndication()`→`FakeMillis_Value` 加算→`Com_MainFunction()`と
 *          再現してから、そこに続く `Com_ReceiveSignal()` を検証する。
 *
 *          本番の `Com_PBCfg.c` は Rte 依存の各種コールバックを持ち依存グラフが
 *          巨大なため（Bsw_TxChain_test.cpp 冒頭コメントと同じ理由）、1シグナル・
 *          1 I-PDU のみのテスト専用の最小 Com 設定を本ファイル内で定義する。
 */
#include <gtest/gtest.h>

extern "C" {
#include "Com.h"
#include "Hal_Millis_fake.h"
#include "Hal_Det_Hw_fake.h"
}

namespace
{

// -----------------------------------------------------------------------
// テスト専用の最小 Com 設定（Bsw_TxChain_test.cpp/Bsw_RxChain_test.cpp と
// 同じ方針）。SignalId=0 (RX, 16bit BigEndian) 1本だけを持つ IPduId=0 の
// RX I-PDU。RxDataTimeoutAction=SUBSTITUTE、TimeoutSubstitutionValue=0xFFFF
// （本番の VehicleSpeed と同じ実運用パターン）。I-PDU 単位のデッドライン監視
// (FirstTimeoutMs/TimeoutMs=0) は無効化し、シグナル単位のみを対象にする
// （Com_ReceiveSignal() は非 Signal Group シグナルに対して Com_SigTimedOut[]
// のみを見るため、この単純化で本チェーンの検証は完結する。Com.c 該当コメント
// 参照）。
// -----------------------------------------------------------------------

const Com_SignalConfigType kTestRxTimeoutSignal = {
    /* SignalId */                0U,
    /* Direction */                COM_SIGNAL_DIRECTION_RX,
    /* IPduId */                   0U,
    /* BitPosition */              0U,
    /* BitSize */                  16U,
    /* Endian */                   COM_BIG_ENDIAN,
    /* InitValue */                0xAAAAU,
    /* FilterAlgorithm */          COM_FILTER_ALWAYS,
    /* Mask */                     0U,
    /* FilterX */                  0U,
    /* FilterMin */                0U,
    /* FilterMax */                0U,
    /* FilterRejectCbk */          NULL,
    /* TmsContributor */           0U,
    /* UpdateBitContributor */     0U,
    /* TransferProperty */         COM_TRANSFER_PROPERTY_PENDING,
    /* RxDataTimeoutAction */      COM_RX_TIMEOUT_ACTION_SUBSTITUTE,
    /* TimeoutSubstitutionValue */ 0xFFFFU,
    /* DataInvalidAction */        COM_DATA_INVALID_ACTION_NONE,
    /* InvalidValue */             0U,
    /* InvalidNotificationCbk */   NULL,
    /* FirstTimeoutMs */           500U,
    /* TimeoutMs */                500U,
    /* TimeoutNotificationCbk */   NULL,
    /* TxAckCbk */                 NULL,
    /* TxErrCbk */                 NULL
};

const Com_IPduConfigType kTestRxTimeoutIPdu = {
    /* IPduId */           0U,
    /* DLC */              2U,
    /* PduRId */           0U,
    /* FirstTimeoutMs */   0U,  /* I-PDU 単位の監視は無効化（本テストの対象外） */
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

const Com_ConfigType kTestComRxTimeoutConfig = {
    /* RxIPdus */       &kTestRxTimeoutIPdu,
    /* RxIPduCount */   1U,
    /* TxIPdus */       NULL,
    /* TxIPduCount */   0U,
    /* Signals */       &kTestRxTimeoutSignal,
    /* SignalCount */   1U,
    /* GwMappings */    NULL,
    /* GwMappingCount */ 0U
};

class Bsw_RxTimeoutChain_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制

        Com_Init(&kTestComRxTimeoutConfig);

        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;  // DeInit() のログを抑制
        Com_DeInit();
    }

    /** 「受信していたが、その後途絶えた」状況を作る共通ヘルパー。
     *  Com_RxIndication() で実データを1回受信させ（Com_RxLastMs 更新・
     *  Com_SigTimedOut クリア）、Com_RxUsingFirstTimeout を steady 状態へ
     *  遷移させる。以降は sig->TimeoutMs（本テストでは 500ms）が基準になる。 */
    void ReceiveOnce(uint16_t value)
    {
        uint8 data[2] = { (uint8)(value >> 8), (uint8)(value & 0xFFU) };
        PduInfoType pdu = { data, 2U };
        Com_RxIndication(0U, &pdu);
    }
};

// ------------------------------------------------------------
// セグメント①: Com_MainFunction() ─ Com_SigTimedOut というフラグで切れるまで
// ------------------------------------------------------------
TEST_F(Bsw_RxTimeoutChain_Test, ComMainFunction_OK_DetectsTimeoutAfterThresholdElapsed)
{
    /* 準備 (Arrange): 一度受信させてから、しきい値(500ms)を超えて時間を進める */
    ReceiveOnce(0x1234U);
    FakeMillis_Value = 600UL;

    /* 実行 (Act) */
    Com_MainFunction();

    /* 評価 (Assert) */
    EXPECT_EQ(Com_Test_GetSigTimedOut(0U), 1U);
}

TEST_F(Bsw_RxTimeoutChain_Test, ComMainFunction_NG_BeforeThreshold_LeavesSigTimedOutClear)
{
    /* 準備 (Arrange): 一度受信させるが、しきい値(500ms)未満しか時間を進めない */
    ReceiveOnce(0x1234U);
    FakeMillis_Value = 400UL;

    /* 実行 (Act) */
    Com_MainFunction();

    /* 評価 (Assert): まだ検知しない */
    EXPECT_EQ(Com_Test_GetSigTimedOut(0U), 0U);
}

// ------------------------------------------------------------
// セグメント②: Com_ReceiveSignal() ─ フラグの続きから RxDataTimeoutAction 適用まで
// ------------------------------------------------------------
TEST_F(Bsw_RxTimeoutChain_Test, ComReceiveSignal_OK_SubstitutesValueAfterTimeout)
{
    /* 準備 (Arrange): セグメント①の終端状態（Com_SigTimedOut が立った状態）を用意する */
    ReceiveOnce(0x1234U);
    FakeMillis_Value = 600UL;
    Com_MainFunction();
    ASSERT_EQ(Com_Test_GetSigTimedOut(0U), 1U);

    /* 実行 (Act) */
    uint16_t value = 0U;
    uint8 ret = Com_ReceiveSignal(0U, &value);

    /* 評価 (Assert): 実受信値(0x1234)ではなく TimeoutSubstitutionValue が返る */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(value, 0xFFFFU);
}

TEST_F(Bsw_RxTimeoutChain_Test, ComReceiveSignal_NG_BeforeTimeout_ReturnsLastReceivedValue)
{
    /* 準備 (Arrange): 受信直後、まだタイムアウトしきい値に達していない */
    ReceiveOnce(0x1234U);
    FakeMillis_Value = 400UL;
    Com_MainFunction();
    ASSERT_EQ(Com_Test_GetSigTimedOut(0U), 0U);

    /* 実行 (Act) */
    uint16_t value = 0U;
    uint8 ret = Com_ReceiveSignal(0U, &value);

    /* 評価 (Assert): 実受信値がそのまま返る（置換されない） */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(value, 0x1234U);
}

}  // namespace
