/**
 * \file    TimeSyncStack_SyncFup_test.cpp
 * \brief   時刻同期（CanTSyn）の SYNC/FUP が、CanIf と Can を通って HW まで届くコールチェーンのテスト
 * \details CanTSyn の単体テストは CanIf_Transmit() を wrap で成功固定にして、渡したフレームの中身だけを検証している。
 *          本ファイルは CanIf と Can を実体のまま使い、次の2点をつなぎで確認する。
 *            1. SYNC が CAN ID 0x240 で HW へ届き、Can_MainFunction_Write() が送信完了を CanTSyn へ通知すると、
 *               その通知の中（Can_MainFunction_Write() が確認キューをドレインしている最中）で CanTSyn が FUP を
 *               送信し、同じドレインの中で FUP の送信完了も処理される（Can_Write() がドレイン中のキューへ
 *               追加しても取りこぼさない）。
 *            2. 1 組が完了した後、次の送信周期で SC を進めた SYNC が送られる。
 *          EcuM・ComM・CanSM などは不要。CanIf の PduMode とコントローラの状態は、本ファイルで直接設定する。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Can.h"
#include "Can_Hw.h"
#include "CanIf.h"
#include "CanTSyn.h"
#include "CanTSyn_Cfg.h"
#include "Fake_Can_Hw.h"
#include "Fake_Millis.h"
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

/** CANTSYN_CANIF_TX_PDU_ID だけを有効化した TX PDU（それ以前の添字はダミー）。 */
const CanIf_TxPduConfigType kTimeSyncTxPduConfig[CANTSYN_CANIF_TX_PDU_ID + 1U] = {
    { 0U, 0U, 0U, 0U, NULL },
    { 0U, 0U, 0U, 0U, NULL },
    { 0U, 0U, 0U, 0U, NULL },
    { 0U, 0U, 0U, 0U, NULL },
    { 0U, 0U, 0U, 0U, NULL },
    { 0U, 0U, 0U, 0U, NULL },
    { 0U, 0U, 0U, 0U, NULL },
    { /* UpperLayerTxPduId */ CANTSYN_TX_PDU_ID,
      /* CanId */             0x240U,
      /* Dlc */               CANTSYN_DLC,
      /* Hth */               0U,
      /* TxConfirmFct */      CanTSyn_TxConfirmation }
};

const CanIf_ConfigType kTimeSyncCanIfConfig = {
    /* TxPduConfig */ kTimeSyncTxPduConfig,
    /* TxPduCount */  CANTSYN_CANIF_TX_PDU_ID + 1U,
    /* RxPduConfig */ NULL,
    /* RxPduCount */  0U
};

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* 時刻同期のコールチェーンのテスト用フィクスチャ。
 * SetUp(): 模擬 Can_Hw・wrap・時刻を初期化し、Can_Init()・CanIf_Init()（CanTSyn の TX PDU のみ有効）・CanIf_SetControllerMode(STARTED)・CanIf_SetPduMode(ONLINE)・CanTSyn_Init(NULL) の順に呼ぶ。
 * TearDown(): CanIf_DeInit() で CanIf を未初期化へ戻す。 */
class TimeSyncStack_SyncFup_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeCanHw_Reset();
        WrapCanIf_Reset();
        WrapCan_Reset();
        FakeMillis_Reset();
        FakeDetHw_Reset();
        FakeDetHw_LogSuppressed = 1U;
        FakeMillis_Value        = 12345UL;

        canConfig.filter.filterId = 0x0220U;
        canConfig.filter.mask     = 0x1FFFU;
        canConfig.csPin           = 10U;
        canConfig.intPin          = 2U;
        canConfig.baudrate        = 500000U;
        canConfig.crystalFreq     = CAN_CRYSTAL_16MHZ;

        Can_Init(&canConfig);
        CanIf_Init(&kTimeSyncCanIfConfig);
        ASSERT_EQ(CanIf_SetControllerMode(0U, CAN_CS_STARTED), E_OK);
        ASSERT_EQ(CanIf_SetPduMode(0U, CANIF_ONLINE), E_OK);
        CanTSyn_Init(NULL);

        FakeCanHw_Reset();   // ここまでの HW 呼び出しを検証対象から除く
        FakeDetHw_Reset();
        FakeDetHw_LogSuppressed = 0U;
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

TEST_F(TimeSyncStack_SyncFup_Test, OK_SyncReachesHwThenConfirmationSendsFupInSameDrain)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanTSyn_MainFunction();   // SYNC を送る（Can_Write() は確認を保留キューへ積む）
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    ASSERT_EQ(FakeCanHw_LastSendId, 0x240U);
    ASSERT_EQ(FakeCanHw_LastSendData[0], CANTSYN_TYPE_SYNC);
    FakeMillis_Value += 5UL;   // SYNC の送信完了まで 5ms かかった

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_MainFunction_Write();   // 確認を通知 → CanTSyn が FUP を送る → その確認も同じドレインで処理される

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeCanHw_SendCount, 2U);                          // FUP が HW へ届いた
    EXPECT_EQ(FakeCanHw_LastSendId, 0x240U);
    EXPECT_EQ(FakeCanHw_LastSendDlc, CANTSYN_DLC);
    EXPECT_EQ(FakeCanHw_LastSendData[0], CANTSYN_TYPE_FUP);
    EXPECT_EQ(FakeCanHw_LastSendData[2], 0x00U);                 // SYNC と同じ SC
    // T4 = (12345ms の ns 部分 = 345ms) + 5ms = 350ms = 350000000ns = 0x14DC9380（ビッグエンディアン）
    EXPECT_EQ(FakeCanHw_LastSendData[4], 0x14U);
    EXPECT_EQ(FakeCanHw_LastSendData[5], 0xDCU);
    EXPECT_EQ(FakeCanHw_LastSendData[6], 0x93U);
    EXPECT_EQ(FakeCanHw_LastSendData[7], 0x80U);

    // FUP の送信完了も同じドレインで処理済みのため、もう一度ドレインしても何も起きない
    Can_MainFunction_Write();
    EXPECT_EQ(FakeCanHw_SendCount, 2U);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(TimeSyncStack_SyncFup_Test, OK_NextPeriodSendsSyncWithNextSequenceCounter)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanTSyn_MainFunction();     // SYNC (SC=0)
    Can_MainFunction_Write();   // FUP まで完了
    ASSERT_EQ(FakeCanHw_SendCount, 2U);
    FakeMillis_Value += CANTSYN_TX_PERIOD_MS;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTSyn_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeCanHw_SendCount, 3U);
    EXPECT_EQ(FakeCanHw_LastSendData[0], CANTSYN_TYPE_SYNC);
    EXPECT_EQ(FakeCanHw_LastSendData[2], 0x01U);   // SC が進む
}

}  // namespace
