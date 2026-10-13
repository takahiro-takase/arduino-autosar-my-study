/**
 * \file    Dcm_TpTxData_AfterConfirmation_test.cpp
 * \brief   Dcm_TpTxConfirmation() の後は、送信が進行中でなくなり、Dcm_CopyTxData() が拒否されることの、単体テスト。
 *
 * \details [SWS_Dcm_00351]: 送信の終了（E_OK / E_NOT_OK）を受けた Dcm は、送信の進行中の状態を解除する。
 *          以降の Dcm_CopyTxData() は BUFREQ_E_NOT_OK になる。
 */

#include <gtest/gtest.h>

extern "C" {
#include "Dcm.h"
#include "Dcm_Cfg.h"
#include "Dem.h"
#include "CanTp.h"
#include "Wrap_CanTp.h"
#include "Wrap_Dcm.h"
#include "Fake_Millis.h"
#include "Fake_Det_Hw.h"
#include "Wrap_ComM.h"
}

namespace
{

/* Dcm の TP 送信（Dcm_CopyTxData / Dcm_TpTxConfirmation）の単体テスト用フィクスチャ。CanTp は実体を使わず、Wrap_CanTp で CanTp_Transmit() を強制的に E_OK にする
 * （送信完了の通知は抑制し、Dcm を「送信が進行中」のまま止める）。VIN の読み出し（0x22 0xF190、応答 20 バイト）を要求して、Dcm_TxBuf に応答を作らせる。
 * SetUp(): 時刻・wrap を初期化し、CanTp_Transmit() を強制 E_OK（確認通知の抑制つき）にして、Dem_Init(NULL)・Dcm_Init(NULL) を呼び、VIN の要求を渡す。
 * TearDown(): ComM の wrap を戻して、他のテストファイルへ影響を残さない。 */
class Dcm_TpTxData_AfterConfirmation_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        WrapCanTp_Reset();
        WrapDcm_Reset();
        Suppressed_ComM_DcmDiagnostic = 1U;  // 本テストは通信管理(ComM/CanSM/CanNm)が対象外
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制

        FailFromCallCount_CanTp_Transmit    = 1U;   // CanTp_Transmit() を、常に強制値で返す
        ForcedReturn_CanTp_Transmit         = E_OK;
        SuppressConfirmation_CanTp_Transmit = 1U;   // Dcm_TpTxConfirmation() を呼ばない（送信が進行中のまま）

        CanTp_Init(NULL);
        Dem_Init(NULL);
        Dcm_Init(NULL);

        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間

        const uint8 req[3] = { DCM_SID_READ_DATA, (uint8)(DCM_DID_VIN >> 8U), (uint8)(DCM_DID_VIN & 0xFFU) };
        ASSERT_EQ(WrapDcm_InjectRequest(req, (uint8)sizeof(req)), E_OK);
        ASSERT_EQ(LastLength_CanTp_Transmit, (PduLengthType)(3U + DCM_VIN_LENGTH));  // 前提: 応答 20 バイトの送信を依頼した
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        WrapComM_Reset();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Dcm_TpTxData_AfterConfirmation_Test, OK_CopyTxDataIsRejectedAfterTxConfirmation)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 buf[3] = { 0U };
    PduInfoType   info = { buf, 3U };
    PduLengthType avail = 0U;
    ASSERT_EQ(Dcm_CopyTxData(0U, &info, NULL, &avail), BUFREQ_OK);   // 前提: 送信の進行中は、コピーできる
    RetryInfoType rewind = { TP_DATARETRY, 3U };
    PduInfoType   none = { NULL, 0U };
    ASSERT_EQ(Dcm_CopyTxData(0U, &none, &rewind, &avail), BUFREQ_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dcm_TpTxConfirmation(0U, E_OK);
    BufReq_ReturnType after = Dcm_CopyTxData(0U, &info, NULL, &avail);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(after, BUFREQ_E_NOT_OK);
}

}  // namespace
