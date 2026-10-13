/**
 * \file    Dcm_TpTxData_Retry_test.cpp
 * \brief   Dcm_CopyTxData() の TP_DATARETRY が、送信位置を戻して同じデータを再びコピーすることの、単体テスト。
 *
 * \details [SWS_Dcm_00092]: retry->TpDataState が TP_DATARETRY のときは、TxTpDataCnt バイト前から再度コピーする。
 *          戻した後の残りのバイト数も、それに合わせて増える。
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
class Dcm_TpTxData_Retry_Test : public ::testing::Test
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

TEST_F(Dcm_TpTxData_Retry_Test, OK_RetryRewindsTxPositionAndCopiesSameDataAgain)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 first[5]  = { 0U };
    uint8 second[5] = { 0U };
    PduInfoType   info1 = { first, 5U };
    PduInfoType   info2 = { second, 5U };
    PduInfoType   none  = { NULL, 0U };
    RetryInfoType rewind = { TP_DATARETRY, 5U };
    PduLengthType avail = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BufReq_ReturnType r1 = Dcm_CopyTxData(0U, &info1, NULL, &avail);
    PduLengthType availAfterFirst = avail;
    BufReq_ReturnType r2 = Dcm_CopyTxData(0U, &none, &rewind, &avail);   // 5 バイト戻す（コピーはしない）
    PduLengthType availAfterRewind = avail;
    BufReq_ReturnType r3 = Dcm_CopyTxData(0U, &info2, NULL, &avail);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(r1, BUFREQ_OK);
    EXPECT_EQ(availAfterFirst, (PduLengthType)(3U + DCM_VIN_LENGTH - 5U));
    EXPECT_EQ(first[0], 0x62U);                           // 応答の先頭は、正応答 SID (0x22 + 0x40)
    EXPECT_EQ(r2, BUFREQ_OK);
    EXPECT_EQ(availAfterRewind, (PduLengthType)(3U + DCM_VIN_LENGTH));   // 位置が先頭へ戻った
    EXPECT_EQ(r3, BUFREQ_OK);
    for (uint8 i = 0U; i < 5U; i++)
        EXPECT_EQ(second[i], first[i]) << "i=" << (unsigned)i;
}

}  // namespace
