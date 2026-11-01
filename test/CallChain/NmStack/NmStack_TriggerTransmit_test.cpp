/**
 * \file    NmStack_TriggerTransmit_test.cpp
 * \brief   NM フレームのトリガ送信（下位層が送信データを取りに来る経路）のコールチェーンの単体テスト
 *          （GoogleTest / CMake native_chain_tests）。
 *
 * \details CanIf_TriggerTransmit() → CanNm_TriggerTransmit() の順に、実体のモジュールを結んで、
 *          CanNm が組み立てる NM フレーム（CBV と Source Node ID）が呼び出し元のバッファへ届くことを確認する。
 *          CanNm は CanIf の直接の上位層なので、PduR は経由しない。PDU ID は、CanIf の TxPduId（0）が、
 *          CanNm の TX PDU の ID（CanNmConf_CanNmTxPdu_CanNm_Tx）へ翻訳される。
 *          CanNm_TransmitPdu() が CanIf_Transmit() へ送る通常の経路とは逆向きで、Can_Write() は呼ばれない。
 *
 *          本番の設定は、CanNm の TX PDU をトリガ送信に対応させない（CanIf_TxPduConfigType.TriggerTransmitFct が NULL）
 *          ので、この経路は本テスト専用の設定で結ぶ。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "CanNm.h"
#include "CanNm_Cfg.h"
#include "CanIf.h"
#include "CanSM.h"
#include "ComM.h"
#include "Nm.h"
#include "Can.h"
#include "Fake_Det_Hw.h"
#include "Fake_Millis.h"
#include "Wrap_Can.h"
#include "Wrap_CanIf.h"
#include "Wrap_Os.h"
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

const PduIdType kCanIfTxPduId = 0U;   // CanIf の TX PDU（上位層は CanNm）

// CanIf: TX PDU 0（上位層の PDU ID は CanNm の TX PDU）が、CanNm_TriggerTransmit へ転送する。
CanIf_TxPduConfigType gCanIfTxPdu;
CanIf_RxPduConfigType gCanIfRxPdu;
CanIf_ConfigType      gCanIfConfig;

void InitStack()
{
    gCanIfTxPdu = {};
    gCanIfTxPdu.UpperLayerTxPduId  = CanNmConf_CanNmTxPdu_CanNm_Tx;
    gCanIfTxPdu.CanId              = 0x500U;
    gCanIfTxPdu.Dlc                = CANNM_DLC;
    gCanIfTxPdu.Hth                = 0U;
    gCanIfTxPdu.TriggerTransmitFct = CanNm_TriggerTransmit;
    gCanIfRxPdu = {};
    gCanIfRxPdu.CanId = 0x200U;
    gCanIfRxPdu.Dlc   = 1U;

    gCanIfConfig = {};
    gCanIfConfig.TxPduConfig = &gCanIfTxPdu;
    gCanIfConfig.TxPduCount  = 1U;
    gCanIfConfig.RxPduConfig = &gCanIfRxPdu;
    gCanIfConfig.RxPduCount  = 1U;

    FakeDetHw_LogSuppressed = 1U;
    FakeMillis_Reset();
    WrapCan_Reset();
    WrapCanIf_Reset();
    WrapOs_Reset();
    ComM_DeInit();   // CanNm_RxIndication のカスケードを防ぐ
    CanSM_Init(NULL);
    CanIf_Init(&gCanIfConfig);
    CanNm_Init(NULL);
    Nm_Init(NULL);
    FakeDetHw_Reset();
    FakeDetHw_LogSuppressed = 0U;
}

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* SetUp(): CanIf → CanNm を、本テスト専用の設定で初期化する（Init 中の DET ログは抑制する）。
 * TearDown(): CanNm_DeInit()、CanIf_DeInit()、CanSM_DeInit() で未初期化へ戻す。 */
class NmStack_TriggerTransmit_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        InitStack();
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        CanNm_DeInit();
        CanIf_DeInit();
        CanSM_DeInit();
        WrapCan_Reset();
        WrapCanIf_Reset();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(NmStack_TriggerTransmit_Test, NmStack_TriggerTransmit_OK_CanNmFrameReachesCaller)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 buf[8];
    for (uint8 i = 0U; i < 8U; i++) { buf[i] = 0xEEU; }
    PduInfoType info = { buf, 8U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    // CanIf → CanNm（CanIf が、CanNm の TX PDU の ID へ翻訳する）
    const Std_ReturnType ret = CanIf_TriggerTransmit(kCanIfTxPduId, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(info.SduLength, (PduLengthType)CANNM_DLC);
    EXPECT_EQ(buf[0], 0x00U);                        // CBV
    EXPECT_EQ(buf[1], (uint8)CANNM_SOURCE_NODE_ID);  // Source Node ID
    EXPECT_EQ(buf[2], 0xEEU);                        // NM フレームの長さを超える部分には触れない
    EXPECT_EQ(CallCount_Can_Write, 0U);              // 通常の送信（Can_Write）は、行わない
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(NmStack_TriggerTransmit_Test, NmStack_TriggerTransmit_NG_SmallBufferRejectedByCanNmAndReportedToCaller)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 buf[8];
    for (uint8 i = 0U; i < 8U; i++) { buf[i] = 0xEEU; }
    PduInfoType info = { buf, 1U };   // NM フレームの長さ（CANNM_DLC）より小さい

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanIf_TriggerTransmit(kCanIfTxPduId, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);        // CanNm の E_NOT_OK が、CanIf を通って、そのまま届く
    EXPECT_EQ(buf[0], 0xEEU);        // 何もコピーされない
    EXPECT_EQ(info.SduLength, 1U);
}

}  // namespace
