/**
 * \file    Bsw_TxE2EChain_test.cpp
 * \brief   README.md「Tx 処理」→「E2E（E2EHealthStatus 送信）」コールチェーンの
 *          単体テスト（GoogleTest / PlatformIO `[env:native_chain]`）。
 *
 * \details README の該当コールチェーン図：
 *
 *              Com_MainFunctionTx()
 *                → TxTransformCbk があれば呼ぶ    ← Rte_COMTransform_E2EHealthStatus()
 *                                                    → E2EXf_TransformP05() → E2E_P05Protect()
 *                → PduR_ComTransmit() → CanIf_Transmit() → Can_Write()   （以降は「通常」と同じ）
 *
 *          「通常」の Tx チェーン（Com_MainFunctionTx() → PduR_ComTransmit() →
 *          CanIf_Transmit() → Can_Write()）は Bsw_TxChain_test.cpp が既に検証
 *          済みのため、本テストは TxTransformCbk フックの部分（E2EXf_TransformP05()
 *          → E2E_P05Protect() が Counter・CRC16 を正しく書き込むこと）に絞る。
 *          ただし「フックが正しく呼ばれて最終的に CAN フレームまで届くこと」
 *          自体は Bsw_TxChain_test.cpp の対象外（TxTransformCbk=NULL の設定）
 *          のため、本テストでも Can_Hw（フェイク）まで通して確認する。
 *
 *          本番の TxTransformCbk（`Rte_COMTransform_E2EHealthStatus()`）は
 *          `Rte.c` にあるが、`Rte.c` 自体は IoHwAb/FiM/App_EngineManager/
 *          App_WarningIndicator まで巨大な依存グラフを引き込むため
 *          （Bsw_TxChain_test.cpp 冒頭コメントと同じ理由）リンクしない。
 *          本ファイル内に、本番と同じ1行の委譲呼び出し
 *          （`E2EXf_TransformP05(&E2EXf_E2EHealthStatusTxCfgP05, Data, Length)`）
 *          をテスト専用の TxTransformCbk として定義し、そこから先
 *          （E2EXf.c/E2EXf_PBCfg.c/E2E_P05.c）は実体をそのまま検証する。
 *          E2EXf_PBCfg.c の本番設定（`E2EXf_E2EHealthStatusTxCfgP05`,
 *          DataID=0x220, DataLength=5）をそのまま使う（Rte.c と異なり
 *          E2EXf_PBCfg.c 自体は Rte 依存を持たないため、Bsw_TxChain_test.cpp
 *          のように専用の最小設定を別途定義する必要がない）。
 *
 *          期待値の算出は、E2E_P05.c の CRC16 実装を手でコピーせず、本テストの
 *          中で独立した `E2E_P05ProtectStateType`（新規初期化）と、本番と同じ
 *          DataID/DataLength/Offset を持つローカル `E2E_P05ConfigType` を使って
 *          `E2E_P05Protect()` を直接呼び、同じ入力から得られる基準値と比較する
 *          （E2E_P05.c 自体の CRC16 正しさは `test/test_native/` の
 *          `E2EP05Test.*` が別途検証済み。ここでは「コールチェーンの配線が
 *          正しいか」だけを見る）。
 */
#include <gtest/gtest.h>

extern "C" {
#include "Com.h"
#include "PduR.h"
#include "CanIf.h"
#include "Can.h"
#include "Can_Hw.h"
#include "E2EXf.h"
#include "E2EXf_PBCfg.h"
#include "E2E_P05.h"
#include "Hal_Can_Hw_fake.h"
#include "Hal_Det_Hw_fake.h"
}

namespace
{

// -----------------------------------------------------------------------
// テスト専用の TxTransformCbk。本番の Rte_COMTransform_E2EHealthStatus()
// と同じ1行の委譲呼び出し（ファイル冒頭コメント参照）。
// -----------------------------------------------------------------------
void TestTxTransform_E2EHealthStatus(uint8* Data, uint8 Length)
{
    E2EXf_TransformP05(&E2EXf_E2EHealthStatusTxCfgP05, Data, Length);
}

// -----------------------------------------------------------------------
// テスト専用の最小 Com/PduR/CanIf 設定（Bsw_TxChain_test.cpp と同じ方針）。
// SignalId=0 (TX, 16bit BigEndian) をバイト3-4（E2E ヘッダ CRC16(2B)+
// Counter(1B) の直後）に配置した、IPduId=0・DLC=5 の TX I-PDU
// （E2EXf_E2EHealthStatusTxCfgP05 の DataLength=5 と一致させる）。
// -----------------------------------------------------------------------

const Com_SignalConfigType kTestSignal = {
    /* SignalId */                0U,
    /* Direction */                COM_SIGNAL_DIRECTION_TX,
    /* IPduId */                   0U,
    /* BitPosition */              24U,  // byte3 (E2E ヘッダ 3 バイト分の後)
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
    /* RxTOutCbk */                NULL,
    /* TxAckCbk */                 NULL,
    /* TxErrCbk */                 NULL
};

const Com_IPduConfigType kTestTxIPdu = {
    /* IPduId */           0U,
    /* DLC */              5U,  // E2EXf_E2EHealthStatusTxCfgP05 の DataLength と一致
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
    /* TxTransformCbk */   TestTxTransform_E2EHealthStatus
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
    /* CanId */             0x220U,  // 本番の E2EHealthStatus と同じ CAN ID
    /* Dlc */               5U,
    /* Hth */               0U,
    /* TxConfirmFct */      NULL
};

const CanIf_ConfigType kTestCanIfConfig = {
    /* TxPduConfig */ &kTestCanIfTxPdu,
    /* TxPduCount */  1U,
    /* RxPduConfig */ NULL,
    /* RxPduCount */  0U
};

// 期待値算出用のローカル E2E 設定（本番の E2EXf_E2EHealthStatusCfgP05 と
// 同じ DataID/DataLength/Offset。ファイル冒頭コメント参照）。
const E2E_P05ConfigType kRefE2EHealthStatusCfg = {
    0x0220U,  /* DataID */
    5U,       /* DataLength */
    0U,       /* MaxDeltaCounter（Protect 側では未使用） */
    0U        /* Offset */
};

class Bsw_TxE2EChain_Test : public ::testing::Test
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
        /* CanIf_Init() より前に Can_SetControllerMode() を直接呼ぶと、CanIf が
         * 追跡するコントローラ状態（CanIf_ControllerMode[]、CanIf.c 参照）が
         * Init() で CAN_CS_STOPPED に巻き戻され、実際の Can 側の状態
         * （CAN_CS_STARTED）と食い違ったままになる。本テストは CanSM を
         * 経由しないためこの食い違い自体は実害が無いが、CanIf_SetControllerMode()
         * 経由に統一しておく方が事故が起きない（/code-review 指摘）。 */
        CanIf_SetControllerMode(0U, CAN_CS_STARTED);
        /* 本テストは CanSM を経由しない（CanSM_Init() を呼ばない）ため、
         * CanIf_Init() 直後の既定値 CANIF_OFFLINE のままでは CanIf_Transmit()
         * が常に E_NOT_OK になってしまう。CanSM が FULL_COM 確立時に行う
         * CanIf_SetPduMode(CANIF_ONLINE) を代わりにここで行う
         * （2026-08 追加、CanIf.c 参照）。 */
        CanIf_SetPduMode(0U, CANIF_ONLINE);
        PduR_Init(&kTestPduRConfig);
        Com_Init(&kTestComConfig);
        // E2EXf_PBCfg_Init() は E2EXf_E2EHealthStatusTxCfgP05 が指す
        // ProtectState（Counter）を毎回リセットする。本番同様、
        // EcuM_Init() が Com_Init() の直後に呼ぶのと同じ順序。
        E2EXf_PBCfg_Init();

        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;  // DeInit() のログを抑制
        E2EXf_DeInit();
        Com_DeInit();
        CanIf_DeInit();
    }

    Can_ConfigType canConfig;
};

// ------------------------------------------------------------
TEST_F(Bsw_TxE2EChain_Test, ComMainFunction_OK_E2EProtectsAndReachesCanHw)
{
    /* 準備 (Arrange) */
    uint16_t value = 0xABCDU;
    Com_SendSignal(0U, &value);

    // 独立した基準状態で同じ入力を Protect し、期待値とする
    // （ファイル冒頭コメント参照）。
    uint8 refBuf[5] = { 0U, 0U, 0U, 0xABU, 0xCDU };
    E2E_P05ProtectStateType refState;
    E2E_P05ProtectInit(&refState);
    E2E_P05Protect(&kRefE2EHealthStatusCfg, &refState, refBuf, 5U);

    /* 実行 (Act) */
    Com_MainFunctionTx();

    /* 評価 (Assert) */
    EXPECT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendId, 0x220U);
    EXPECT_EQ(FakeCanHw_LastSendDlc, 5U);
    for (uint8 i = 0U; i < 5U; i++)
        EXPECT_EQ(FakeCanHw_LastSendData[i], refBuf[i]) << "byte index " << (int)i;
}

TEST_F(Bsw_TxE2EChain_Test, ComMainFunction_OK_CounterIncrementsAcrossSends)
{
    /* 準備 (Arrange) */
    uint16_t value = 0x0000U;

    /* 実行 (Act) 1 回目 */
    Com_SendSignal(0U, &value);
    Com_MainFunctionTx();
    const uint8 firstCounter = FakeCanHw_LastSendData[2];  // Offset+2 = Counter

    /* 実行 (Act) 2 回目 */
    Com_SendSignal(0U, &value);
    Com_MainFunctionTx();
    const uint8 secondCounter = FakeCanHw_LastSendData[2];

    /* 評価 (Assert) */
    EXPECT_EQ(firstCounter, 0U);
    EXPECT_EQ(secondCounter, 1U);
}

}  // namespace
