/**
 * \file    Bsw_RxE2EChain_test.cpp
 * \brief   README.md「Rx 処理」→「E2E（EngineInfo/AbsInfo 受信）」コールチェーンの
 *          単体テスト（GoogleTest / PlatformIO `[env:native_chain]`）。
 *
 * \details README の該当コールチェーン図：
 *
 *              Com_RxIndication()                 ← EngineInfo/AbsInfo（RxIndicationCbk 経由）
 *                → Rte_COMCbk_EngineInfo/AbsInfo()
 *                  → E2EXf_InverseTransformP05() → E2E_P05Check()
 *
 *          「通常」の Rx チェーン（Can_Isr() → … → Com_RxIndication()）は
 *          Bsw_RxChain_test.cpp が既に検証済みのため、本テストは
 *          RxIndicationCbk フックの部分（E2EXf_InverseTransformP05() →
 *          E2E_P05Check() が CRC・カウンタ連続性を正しく検証すること）に絞り、
 *          `Com_RxIndication()` を直接呼ぶところから始める
 *          （README の図もこの粒度で揃えている）。
 *
 *          本番の RxIndicationCbk（`Rte_COMCbk_EngineInfo()`）は `Rte.c` に
 *          あるが、`Rte.c` 自体は IoHwAb/FiM/App_EngineManager/
 *          App_WarningIndicator まで巨大な依存グラフを引き込むため
 *          （Bsw_TxChain_test.cpp 冒頭コメントと同じ理由）リンクしない。
 *          本ファイル内に、本番の Rte_COMCbk_EngineInfo() と同じ処理
 *          （Com_ReceiveSignalGroupArray() で生バイト列を取得し
 *          E2EXf_InverseTransformP05() へ渡す）をテスト専用の
 *          RxIndicationCbk として定義し、そこから先（E2EXf.c/
 *          E2EXf_PBCfg.c/E2E_P05.c）は実体をそのまま検証する。E2EXf_PBCfg.c
 *          の本番設定（`E2EXf_EngineInfoRxCfg`、DataID=0x100、DataLength=7）
 *          をそのまま使う。E2EMon への通知（`E2EMon_NotifyCheckResultP05()`）
 *          は README の「E2E」節の図に含めていないため対象外
 *          （E2EMon 自体はテレメトリ集計という別軸の話であり、CRC/カウンタ
 *          検証というこのコールチェーンの本題ではないため）。
 *
 *          検証対象のフレームは、本テストの中で独立した
 *          `E2E_P05ProtectStateType`（新規初期化）と、本番と同じ
 *          DataID/DataLength/Offset を持つローカル `E2E_P05ConfigType` を使い
 *          `E2E_P05Protect()` で組み立てる（E2E_P05.c 自体の CRC16 正しさは
 *          `test/test_native/` の `E2EP05Test.*` が別途検証済み）。
 */
#include <gtest/gtest.h>

extern "C" {
#include "Com.h"
#include "E2EXf.h"
#include "E2EXf_PBCfg.h"
#include "E2E_P05.h"
#include "Hal_Det_Hw_fake.h"
}

namespace
{

// -----------------------------------------------------------------------
// テスト専用の RxIndicationCbk。本番の Rte_COMCbk_EngineInfo() と同じ処理
// （ファイル冒頭コメント参照）。呼び出し回数・直近の検証結果を記録する。
// -----------------------------------------------------------------------
uint32             g_CallbackCallCount = 0U;
E2E_P05StatusType  g_LastCheckStatus   = E2E_P05STATUS_ERROR;

void TestRxIndication_EngineInfo(void)
{
    g_CallbackCallCount++;

    uint8 buf[7];
    if (Com_ReceiveSignalGroupArray(0U, buf) != E_OK)
        return;

    E2E_P05StatusType checkStatus = E2E_P05STATUS_ERROR;
    (void)E2EXf_InverseTransformP05(&E2EXf_EngineInfoRxCfg, buf, 7U, &checkStatus);
    g_LastCheckStatus = checkStatus;
}

// -----------------------------------------------------------------------
// テスト専用の最小 Com 設定。IPduId=0・DLC=7 の RX I-PDU
// （E2EXf_EngineInfoRxCfg の DataLength=7 と一致させる）。
// Com_ReceiveSignalGroupArray() は Signal 設定を参照しないため
// （Com.c 参照）、Signals は空でよい。
// -----------------------------------------------------------------------

const Com_IPduConfigType kTestRxIPdu = {
    /* IPduId */           0U,
    /* DLC */              7U,  // E2EXf_EngineInfoRxCfg の DataLength と一致
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
    /* RxIndicationCbk */  TestRxIndication_EngineInfo,
    /* TxTransformCbk */   NULL
};

const Com_ConfigType kTestComConfig = {
    /* RxIPdus */       &kTestRxIPdu,
    /* RxIPduCount */   1U,
    /* TxIPdus */       NULL,
    /* TxIPduCount */   0U,
    /* Signals */       NULL,
    /* SignalCount */   0U,
    /* GwMappings */    NULL,
    /* GwMappingCount */ 0U
};

// 検証対象フレームの組み立て用のローカル E2E 設定（本番の
// E2EXf_EngineInfoCfgP05 と同じ DataID/DataLength/Offset。
// ファイル冒頭コメント参照）。
const E2E_P05ConfigType kRefEngineInfoCfg = {
    0x0100U,  /* DataID */
    7U,       /* DataLength */
    1U,       /* MaxDeltaCounter */
    0U        /* Offset */
};

class Bsw_RxE2EChain_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制

        Com_Init(&kTestComConfig);
        // E2EXf_PBCfg_Init() は E2EXf_EngineInfoRxCfg が指す CheckState
        // （Counter）と WaitForFirstData を毎回リセットする。
        E2EXf_PBCfg_Init();

        g_CallbackCallCount = 0U;
        g_LastCheckStatus   = E2E_P05STATUS_ERROR;

        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;  // DeInit() のログを抑制
        E2EXf_DeInit();
        Com_DeInit();
    }

    // kRefEngineInfoCfg に基づき、独立した Protect 状態でフレームを組み立てる。
    static void BuildFrame(uint8 (&buf)[7], uint8 speedHigh, uint8 speedLow,
                            E2E_P05ProtectStateType* state)
    {
        buf[3] = speedHigh;
        buf[4] = speedLow;
        buf[5] = 0U;
        buf[6] = 0U;
        E2E_P05Protect(&kRefEngineInfoCfg, state, buf, 7U);
    }
};

// ------------------------------------------------------------
TEST_F(Bsw_RxE2EChain_Test, ComRxIndication_OK_ValidFirstFrameE2EChecksOk)
{
    /* 準備 (Arrange): 独立した基準状態で正しい CRC/Counter を持つフレームを組み立てる */
    uint8 buf[7] = { 0U };
    E2E_P05ProtectStateType refState;
    E2E_P05ProtectInit(&refState);
    BuildFrame(buf, 0x01U, 0xF4U, &refState);  // EngineSpeed=500rpm 相当

    PduInfoType pduInfo = { .SduDataPtr = buf, .SduLength = 7U };

    /* 実行 (Act) */
    Com_RxIndication(0U, &pduInfo);

    /* 評価 (Assert) */
    EXPECT_EQ(g_CallbackCallCount, 1U);
    // 初回受信は E2EXf_InverseTransformP05() の WaitForFirstData 特別扱いで
    // OK に格上げされる（CRC が正しい前提。E2EXf.c 参照）。
    EXPECT_EQ(g_LastCheckStatus, E2E_P05STATUS_OK);
}

TEST_F(Bsw_RxE2EChain_Test, ComRxIndication_OK_SecondConsecutiveFrameE2EChecksOk)
{
    /* 準備 (Arrange): 連続する2フレーム（Counter 0→1）を用意する */
    uint8 buf1[7] = { 0U };
    uint8 buf2[7] = { 0U };
    E2E_P05ProtectStateType refState;
    E2E_P05ProtectInit(&refState);
    BuildFrame(buf1, 0x01U, 0xF4U, &refState);  // Counter=0
    BuildFrame(buf2, 0x01U, 0xF4U, &refState);  // Counter=1

    PduInfoType pduInfo1 = { .SduDataPtr = buf1, .SduLength = 7U };
    PduInfoType pduInfo2 = { .SduDataPtr = buf2, .SduLength = 7U };

    /* 実行 (Act) */
    Com_RxIndication(0U, &pduInfo1);  // 1回目: WaitForFirstData により OK
    Com_RxIndication(0U, &pduInfo2);  // 2回目: 純粋な delta=1 判定で OK

    /* 評価 (Assert) */
    EXPECT_EQ(g_CallbackCallCount, 2U);
    EXPECT_EQ(g_LastCheckStatus, E2E_P05STATUS_OK);
}

TEST_F(Bsw_RxE2EChain_Test, ComRxIndication_NG_CorruptedCrcE2EChecksError)
{
    /* 準備 (Arrange): 正しいフレームを組み立てた後、CRC バイトを破壊する */
    uint8 buf[7] = { 0U };
    E2E_P05ProtectStateType refState;
    E2E_P05ProtectInit(&refState);
    BuildFrame(buf, 0x01U, 0xF4U, &refState);
    buf[0] ^= 0xFFU;  // CRC16 下位バイトを破壊

    PduInfoType pduInfo = { .SduDataPtr = buf, .SduLength = 7U };

    /* 実行 (Act) */
    Com_RxIndication(0U, &pduInfo);

    /* 評価 (Assert): CRC 不一致は WaitForFirstData の対象外（E2EXf.c 参照）で
     * 必ず ERROR になる */
    EXPECT_EQ(g_CallbackCallCount, 1U);
    EXPECT_EQ(g_LastCheckStatus, E2E_P05STATUS_ERROR);
}

}  // namespace
