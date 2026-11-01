/**
 * \file    CanNm_TriggerTransmit_test.cpp
 * \brief   CanNm_TriggerTransmit() の単体テスト
 * \details 下位層（CanIf）が、送信の直前に、NM フレームの内容を取りに来る経路（[SWS_CanNm_00351]）。
 *          CBV と Source Node ID の複製、バッファが小さいとき（何も複製しない）、不正な TxPduId
 *          （CANNM_E_INVALID_PDUID。[SWS_CanNm_00195]）、NULL ポインタ、未初期化を確認する。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "ComM.h"
#include "CanNm.h"
#include "CanNm_Cfg.h"
#include "Nm.h"
#include "Fake_Millis.h"
#include "Fake_Det_Hw.h"
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

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* CanNm_TriggerTransmit() の単体テスト用フィクスチャ。
 * SetUp(): ComM_DeInit()（RxIndication のカスケードを防ぐ）の後、CanNm_Init(NULL) と Nm_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): CanNm_DeInit() で未初期化へ戻す。 */
class CanNm_TriggerTransmit_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        ComM_DeInit();
        CanNm_Init(NULL);
        Nm_Init(NULL);
        WrapOs_Reset();
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        CanNm_DeInit();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(CanNm_TriggerTransmit_Test, CanNm_TriggerTransmit_OK_CopiesNmPduAndSetsLength)
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
    const Std_ReturnType ret = CanNm_TriggerTransmit(CanNmConf_CanNmTxPdu_CanNm_Tx, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(info.SduLength, (PduLengthType)CANNM_DLC);
    EXPECT_EQ(buf[0], 0x00U);                       // CBV（Repeat Message Bit は立っていない）
    EXPECT_EQ(buf[1], (uint8)CANNM_SOURCE_NODE_ID); // Source Node ID
    EXPECT_EQ(buf[2], 0xEEU);                       // NM フレームの長さを超える部分には、触れない
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);
}

TEST_F(CanNm_TriggerTransmit_Test, CanNm_TriggerTransmit_OK_CbvReflectsRepeatMessageBit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 buf[8];
    for (uint8 i = 0U; i < 8U; i++) { buf[i] = 0xEEU; }
    PduInfoType info = { buf, 8U };
    CanNm_Test_SetState(CANNM_STATE_NORMAL_OPERATION);
    ASSERT_EQ(CanNm_RepeatMessageRequest(CANNM_MAIN_NETWORK_HANDLE), E_OK);   // Repeat Message Bit を立てる
    FakeDetHw_Reset();   // 下位層（CanIf）が未初期化であることによる記録を、除く

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanNm_TriggerTransmit(CanNmConf_CanNmTxPdu_CanNm_Tx, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(buf[0], (uint8)CANNM_CBV_BIT_REPEAT_MESSAGE_REQUEST);   // CanNm_TransmitPdu() と同じ内容になる
    EXPECT_EQ(buf[1], (uint8)CANNM_SOURCE_NODE_ID);
}

TEST_F(CanNm_TriggerTransmit_Test, CanNm_TriggerTransmit_NG_BufferTooSmallCopiesNothing)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 buf[8];
    for (uint8 i = 0U; i < 8U; i++) { buf[i] = 0xEEU; }
    PduInfoType info = { buf, 1U };   // NM フレームの長さ（CANNM_DLC=2）より小さい

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanNm_TriggerTransmit(CanNmConf_CanNmTxPdu_CanNm_Tx, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(buf[0], 0xEEU);          // 何もコピーしない
    EXPECT_EQ(info.SduLength, 1U);     // PduInfoPtr を変えない
}

TEST_F(CanNm_TriggerTransmit_Test, CanNm_TriggerTransmit_NG_InvalidTxPduId)
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
    const Std_ReturnType ret = CanNm_TriggerTransmit(CanNmConf_CanNmTxPdu_CanNm_Tx + 1U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_INVALID_PDUID);   // [SWS_CanNm_00195]
    EXPECT_EQ(buf[0], 0xEEU);
}

TEST_F(CanNm_TriggerTransmit_Test, CanNm_TriggerTransmit_NG_NullPduInfoPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanNm_TriggerTransmit(CanNmConf_CanNmTxPdu_CanNm_Tx, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_PARAM_POINTER);
}

TEST_F(CanNm_TriggerTransmit_Test, CanNm_TriggerTransmit_NG_NullSduDataPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PduInfoType info = { NULL, 8U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanNm_TriggerTransmit(CanNmConf_CanNmTxPdu_CanNm_Tx, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_PARAM_POINTER);
}

TEST_F(CanNm_TriggerTransmit_Test, CanNm_TriggerTransmit_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 buf[8];
    for (uint8 i = 0U; i < 8U; i++) { buf[i] = 0xEEU; }
    PduInfoType info = { buf, 8U };
    CanNm_DeInit();   // 未初期化にする
    FakeDetHw_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = CanNm_TriggerTransmit(CanNmConf_CanNmTxPdu_CanNm_Tx, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANNM_E_UNINIT);
    EXPECT_EQ(buf[0], 0xEEU);
}

}  // namespace
