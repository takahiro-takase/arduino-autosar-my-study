/**
 * \file    Bsw_Dcm_ReadDtcInfo_test.cpp
 * \brief   UDS SID 0x19 ReadDTCInformation の単体テスト（GoogleTest /
 *          PlatformIO `[env:native_dcm]`）。
 *
 * \details 本プロジェクトで Dcm_Cbk.c/Dem.c を対象とする初めてのユニット
 *          テスト（platformio.ini `[env:native_dcm]` 冒頭のコメント参照）。
 *          GitHub Issue #122（subFunc 0x0A reportSupportedDTC の追加要望）
 *          への対応をきっかけに新設した。
 *
 *          `Dcm_ComIndication()` に生の UDS バイト列を直接渡し、
 *          `CanTp_Transmit()`（`CanTp_fake.h` でキャプチャ）へ渡された応答を
 *          検証する、という「入口と出口だけを見る」ブラックボックステスト。
 *          CanTp/PduR/CanIf/Can は経由しない（`Dcm_ComIndication()` 自体が
 *          「CanTp が組み立てた生 UDS ペイロードを受け取る」入口のため）。
 *
 *          既存 subFunc（0x01/0x02/0x04/0x06）は今回追加した 0x0A との
 *          対比・将来の回帰検知のため最小限のみカバーする。全 UDS サービス
 *          の網羅は本ファイルのスコープ外。
 */
#include <gtest/gtest.h>

extern "C" {
#include "Dcm.h"
#include "Dcm_Cfg.h"
#include "Dem.h"
#include "CanTp_fake.h"
#include "Hal_Millis_fake.h"
#include "Hal_Det_Hw_fake.h"
}

namespace
{

class Bsw_Dcm_ReadDtcInfo_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        FakeCanTp_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制

        Dem_Init(NULL);
        Dcm_Init(NULL);

        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
    }

    /** [0x19, subFunc, ...] を組み立てて Dcm_ComIndication() へ直接渡す。 */
    void SendReadDtcInfo(const uint8* payload, uint8 len)
    {
        PduInfoType pdu = { const_cast<uint8*>(payload), len };
        Dcm_ComIndication(0U, &pdu);
    }
};

// ------------------------------------------------------------
// Dem_GetDTCStatusAvailabilityMask（Dcm_Cbk.c が DEM_STATUS_AVAILABILITY_MASK に
// 直接アクセスしていたレイヤ違反を解消するために追加。上記 SID 0x19 各テストの
// TxBuf[2] 検証が間接的な回帰検知になっているため、ここでは API 自体の
// 直接呼び出しのみ検証する）
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, GetDTCStatusAvailabilityMask_OK_ReturnsConfiguredMask)
{
    uint8 mask = 0U;

    Std_ReturnType ret = Dem_GetDTCStatusAvailabilityMask(DCM_DEM_CLIENT_ID, &mask);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(mask, DEM_STATUS_AVAILABILITY_MASK);
}

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, GetDTCStatusAvailabilityMask_NG_NullPointerReturnsError)
{
    Std_ReturnType ret = Dem_GetDTCStatusAvailabilityMask(DCM_DEM_CLIENT_ID, NULL);

    EXPECT_EQ(ret, E_NOT_OK);
}

// ------------------------------------------------------------
// Dem_EnableDTCSetting/Dem_DisableDTCSetting（実仕様の ClientId 引数・
// Std_ReturnType 戻り値へシグネチャを合わせた際の直接呼び出し検証。
// UDS SID 0x85 経由の挙動は Bsw_Dcm_ControlDTCSetting_test.cpp が担当）
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, EnableDTCSetting_OK_ReturnsOk)
{
    EXPECT_EQ(Dem_EnableDTCSetting(DCM_DEM_CLIENT_ID), E_OK);
}

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, DisableDTCSetting_OK_ReturnsOk)
{
    EXPECT_EQ(Dem_DisableDTCSetting(DCM_DEM_CLIENT_ID), E_OK);
}

// ------------------------------------------------------------
// Dem_GetFaultDetectionCounter（UDS SID 0x19 subFunc 0x0B
// reportDTCFaultDetectionCounter 用に新設。内部の Dem_DebounceCounter[] を
// そのまま返す薄いgetter）
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, GetFaultDetectionCounter_OK_ReturnsZeroForFreshEvent)
{
    sint8 fdc = 0x7F;  /* 未更新を検出できる初期値 */

    Std_ReturnType ret = Dem_GetFaultDetectionCounter(DEM_EVENT_ENGINE_OVERHEAT, &fdc);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(fdc, 0);
}

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, GetFaultDetectionCounter_OK_ReflectsDebounceCounterAfterFailedReport)
{
    (void)Dem_SetEventStatus(DEM_EVENT_ENGINE_OVERHEAT, DEM_EVENT_STATUS_FAILED);

    sint8 fdc = 0;
    Std_ReturnType ret = Dem_GetFaultDetectionCounter(DEM_EVENT_ENGINE_OVERHEAT, &fdc);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(fdc, 1);  /* 中立(0)から FAILED 方向へ1回分だけ進む */
}

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, GetFaultDetectionCounter_NG_InvalidEventIdReturnsError)
{
    sint8 fdc = 0;

    Std_ReturnType ret = Dem_GetFaultDetectionCounter((Dem_EventIdType)DEM_EVENT_COUNT, &fdc);

    EXPECT_EQ(ret, E_NOT_OK);
}

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, GetFaultDetectionCounter_NG_NullPointerReturnsError)
{
    Std_ReturnType ret = Dem_GetFaultDetectionCounter(DEM_EVENT_ENGINE_OVERHEAT, NULL);

    EXPECT_EQ(ret, E_NOT_OK);
}

// ------------------------------------------------------------
// Dcm_GetVin / DID 0xF190（VIN読み出し新設。実仕様ではDcmが呼び出す側の
// 関数だが、本プロジェクトは固定値を返す簡略実装。Dcm_Init()が起動時に
// 一度だけ呼びキャッシュし、UDS SID 0x22経由の応答はそのキャッシュから
// 返す）
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, GetVin_OK_ReturnsFixedSeventeenByteVin)
{
    uint8 vin[DCM_VIN_LENGTH] = { 0 };

    Std_ReturnType ret = Dcm_GetVin(vin);

    EXPECT_EQ(ret, E_OK);
    /* 全バイトが書き換わっている（0x00埋めのまま残っていない）ことのみ検証。
     * 具体的な文字列内容は固定値の実装詳細のため固定しない。 */
    uint8 nonZeroCount = 0U;
    for (uint8 i = 0U; i < DCM_VIN_LENGTH; i++)
        if (vin[i] != 0U)
            nonZeroCount++;
    EXPECT_EQ(nonZeroCount, DCM_VIN_LENGTH);
}

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, GetVin_NG_NullPointerReturnsError)
{
    Std_ReturnType ret = Dcm_GetVin(NULL);

    EXPECT_EQ(ret, E_NOT_OK);
}

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, ReadDataById_OK_VinReturnsSeventeenBytesMatchingDcmGetVin)
{
    /* 準備 (Arrange): [0x22, 0xF1, 0x90] */
    uint8 req[3] = { DCM_SID_READ_DATA, (uint8)(DCM_DID_VIN >> 8U), (uint8)(DCM_DID_VIN & 0xFFU) };

    /* 実行 (Act) */
    PduInfoType pdu = { req, sizeof(req) };
    Dcm_ComIndication(0U, &pdu);

    /* 評価 (Assert): [0x62, 0xF1, 0x90, VIN(17バイト)] */
    ASSERT_EQ(FakeCanTp_TransmitCount, 1U);
    ASSERT_EQ(FakeCanTp_TxLength, (uint8)(3U + DCM_VIN_LENGTH));
    EXPECT_EQ(FakeCanTp_TxBuf[0], 0x62U);
    EXPECT_EQ(FakeCanTp_TxBuf[1], (uint8)(DCM_DID_VIN >> 8U));
    EXPECT_EQ(FakeCanTp_TxBuf[2], (uint8)(DCM_DID_VIN & 0xFFU));

    uint8 expectedVin[DCM_VIN_LENGTH];
    ASSERT_EQ(Dcm_GetVin(expectedVin), E_OK);
    for (uint8 i = 0U; i < DCM_VIN_LENGTH; i++)
        EXPECT_EQ(FakeCanTp_TxBuf[3U + i], expectedVin[i]) << "byte " << (unsigned)i;
}

// ------------------------------------------------------------
// Dcm_GetSesCtrlType/Dcm_GetSecurityLevel（Dcm_Cbk.c 内部の static フィールド
// Dcm_CurrentSession/Dcm_SecurityLevel を読み出すだけの新規 getter API）
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, GetSesCtrlType_OK_ReturnsDefaultSessionAfterInit)
{
    Dcm_SesCtrlType session = 0xFFU;  /* 未更新を検出できる初期値 */

    Std_ReturnType ret = Dcm_GetSesCtrlType(&session);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(session, DCM_SESSION_DEFAULT);
}

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, GetSesCtrlType_OK_ReflectsExtendedSessionAfterRequest)
{
    uint8 req[2] = { DCM_SID_SESSION_CTRL, DCM_SESSION_EXTENDED };
    SendReadDtcInfo(req, sizeof(req));
    ASSERT_EQ(FakeCanTp_TxBuf[0], 0x50U);  // 正応答確認（前提が崩れていないこと）

    Dcm_SesCtrlType session = 0U;
    Std_ReturnType ret = Dcm_GetSesCtrlType(&session);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(session, DCM_SESSION_EXTENDED);
}

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, GetSesCtrlType_NG_NullPointerReturnsError)
{
    EXPECT_EQ(Dcm_GetSesCtrlType(NULL), E_NOT_OK);
}

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, GetSecurityLevel_OK_ReturnsLockedByDefault)
{
    Dcm_SecLevelType level = 0xFFU;  /* 未更新を検出できる初期値 */

    Std_ReturnType ret = Dcm_GetSecurityLevel(&level);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(level, 0U);  /* Locked */
}

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, GetSecurityLevel_NG_NullPointerReturnsError)
{
    EXPECT_EQ(Dcm_GetSecurityLevel(NULL), E_NOT_OK);
}

// ------------------------------------------------------------
// Dcm_ResetToDefaultSession（[SWS_Dcm_00520]。既存の3箇所（明示的な0x10
// defaultSession要求・S3タイムアウト・0x11 ECUReset後）に重複していた
// セッションリセット処理列を集約した新規公開API。3箇所からの呼び出しの
// 回帰は Bsw_Dcm_ControlDTCSetting_test.cpp の AutoReEnables* 系テストが
// 引き続き担当する。ここでは API 自体の直接呼び出しのみ検証する）
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, ResetToDefaultSession_OK_ReturnsSessionToDefault)
{
    uint8 req[2] = { DCM_SID_SESSION_CTRL, DCM_SESSION_EXTENDED };
    SendReadDtcInfo(req, sizeof(req));
    ASSERT_EQ(FakeCanTp_TxBuf[0], 0x50U);  // 前提: extendedSessionへ遷移済み
    Dcm_SesCtrlType sessionBefore = 0U;
    ASSERT_EQ(Dcm_GetSesCtrlType(&sessionBefore), E_OK);
    ASSERT_EQ(sessionBefore, DCM_SESSION_EXTENDED);

    Std_ReturnType ret = Dcm_ResetToDefaultSession();

    Dcm_SesCtrlType sessionAfter = 0xFFU;  /* 未更新を検出できる初期値 */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(Dcm_GetSesCtrlType(&sessionAfter), E_OK);
    EXPECT_EQ(sessionAfter, DCM_SESSION_DEFAULT);
}

// ------------------------------------------------------------
// subFunc 0x0A reportSupportedDTC（今回の追加分、GitHub Issue #122）
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, ReadDtcSupported_OK_ReturnsAllConfiguredDtcsRegardlessOfStatus)
{
    /* 準備 (Arrange): [0x19, 0x0A]（追加パラメータなし） */
    uint8 req[2] = { DCM_SID_READ_DTC_INFO, DCM_DTC_SUBFUNC_REPORT_SUPPORTED };

    /* 実行 (Act) */
    SendReadDtcInfo(req, sizeof(req));

    /* 評価 (Assert): [0x59, 0x0A, availMask, (DTC_H,DTC_M,DTC_L,status) x DEM_EVENT_COUNT]
     * を、Dem_Init() 直後の状態（1件も FAILED になっていない）でも
     * DEM_EVENT_COUNT 件全て返す（reportDTCByStatusMask との違いそのもの）。 */
    ASSERT_EQ(FakeCanTp_TransmitCount, 1U);
    ASSERT_EQ(FakeCanTp_TxLength, (uint8)(3U + DEM_EVENT_COUNT * 4U));
    EXPECT_EQ(FakeCanTp_TxBuf[0], 0x59U);
    EXPECT_EQ(FakeCanTp_TxBuf[1], DCM_DTC_SUBFUNC_REPORT_SUPPORTED);
    EXPECT_EQ(FakeCanTp_TxBuf[2], DEM_STATUS_AVAILABILITY_MASK);
}

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, ReadDtcSupported_OK_DiffersFromReportByStatusMaskWithImpossibleMask)
{
    /* 準備 (Arrange): reportDTCByStatusMask (0x02) を、どの DTC のステータス
     * とも一致しないマスク (0x00) で送る。AND 演算の定義上、0x00 マスクは
     * 何にも一致しないため 0 件になるはず。 */
    uint8 reqByMask[3] = { DCM_SID_READ_DTC_INFO, DCM_DTC_SUBFUNC_REPORT_BY_MASK, 0x00U };
    SendReadDtcInfo(reqByMask, sizeof(reqByMask));
    ASSERT_EQ(FakeCanTp_TransmitCount, 1U);
    /* 応答: [0x59, 0x02, availMask] のみ（0 件時は DTC 列挙部分が無い） */
    ASSERT_EQ(FakeCanTp_TxLength, 3U);

    /* 実行 (Act): 同じ Dem 状態のまま reportSupportedDTC (0x0A) を送る */
    FakeCanTp_Reset();
    uint8 reqSupported[2] = { DCM_SID_READ_DTC_INFO, DCM_DTC_SUBFUNC_REPORT_SUPPORTED };
    SendReadDtcInfo(reqSupported, sizeof(reqSupported));

    /* 評価 (Assert): マスクによる絞り込みを一切行わないため、0x02/mask=0x00 が
     * 0 件だったのと対照的に DEM_EVENT_COUNT 件全て返る。これが
     * reportSupportedDTC の存在意義そのもの（Dem_GetSupportedDTCs() の
     * 実装コメント参照）。 */
    ASSERT_EQ(FakeCanTp_TransmitCount, 1U);
    EXPECT_EQ(FakeCanTp_TxLength, (uint8)(3U + DEM_EVENT_COUNT * 4U));
}

// ------------------------------------------------------------
// subFunc 0x14 reportDTCFaultDetectionCounter
// （Dem_GetFaultDetectionCounter() 新設に伴う追加。DTC 一覧取得は 0x0A と
// 同じ Dem_GetSupportedDTCs() を使うが、応答に statusAvailMask を含まない
// 点が 0x02/0x0A と異なる（ISO 14229-1、/code-review で当初の subFunc
// 0x0B 誤割当ても合わせて訂正済み。docs/modules/Dcm_Notes.md 参照）。
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, ReadDtcFaultDetectionCounter_OK_ReturnsZeroForFreshEvents)
{
    /* 準備 (Arrange): [0x19, 0x14]（追加パラメータなし） */
    uint8 req[2] = { DCM_SID_READ_DTC_INFO, DCM_DTC_SUBFUNC_REPORT_FDC };

    /* 実行 (Act) */
    SendReadDtcInfo(req, sizeof(req));

    /* 評価 (Assert): [0x59, 0x14, (DTC_H,DTC_M,DTC_L,FDC) x DEM_EVENT_COUNT]
     * （availMask バイトは含まない）。Dem_Init() 直後は全イベントの
     * Fault Detection Counter が 0。 */
    ASSERT_EQ(FakeCanTp_TransmitCount, 1U);
    ASSERT_EQ(FakeCanTp_TxLength, (uint8)(2U + DEM_EVENT_COUNT * 4U));
    EXPECT_EQ(FakeCanTp_TxBuf[0], 0x59U);
    EXPECT_EQ(FakeCanTp_TxBuf[1], DCM_DTC_SUBFUNC_REPORT_FDC);
    EXPECT_EQ(FakeCanTp_TxBuf[5], 0U);  // 1件目(EventId=0)のFDC
}

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, ReadDtcFaultDetectionCounter_OK_ReflectsDebounceCounterAfterFailedReport)
{
    /* 準備 (Arrange): EventId=0 (DEM_EVENT_ENGINE_OVERHEAT) を1回 FAILED 報告 */
    (void)Dem_SetEventStatus(DEM_EVENT_ENGINE_OVERHEAT, DEM_EVENT_STATUS_FAILED);

    uint8 req[2] = { DCM_SID_READ_DTC_INFO, DCM_DTC_SUBFUNC_REPORT_FDC };

    /* 実行 (Act) */
    SendReadDtcInfo(req, sizeof(req));

    /* 評価 (Assert): 1件目(EventId=0)のFDCが中立(0)からFAILED方向へ1進む */
    ASSERT_EQ(FakeCanTp_TransmitCount, 1U);
    EXPECT_EQ(FakeCanTp_TxBuf[5], 1U);
}

// ------------------------------------------------------------
// 既存 subFunc の最小回帰（0x0A 追加による既存ディスパッチへの影響がないこと）
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, ReadDtcCount_OK_ReturnsZeroFailedWhenNothingFailedYet)
{
    /* 準備 (Arrange): [0x19, 0x01, statusMask=DEM_STATUS_TEST_FAILED]。
     * Dem_Init() 直後は DEM_STATUS_NOT_COMPLETED_SINCE_CLEAR ビットが全
     * イベントで立っている（まだ一度もテストが完了していないため）ので、
     * statusMask=0xFF だと全件ヒットしてしまう。「実際に FAILED した
     * DTC の件数」を問うテストにするため、testFailed ビットのみを
     * マスクに使う。 */
    uint8 req[3] = { DCM_SID_READ_DTC_INFO, DCM_DTC_SUBFUNC_REPORT_COUNT, DEM_STATUS_TEST_FAILED };

    /* 実行 (Act) */
    SendReadDtcInfo(req, sizeof(req));

    /* 評価 (Assert): [0x59, 0x01, availMask, format, countH, countL]。
     * Dem_Init() 直後は 1 件も FAILED になっていないため countL=0。 */
    ASSERT_EQ(FakeCanTp_TransmitCount, 1U);
    ASSERT_EQ(FakeCanTp_TxLength, 6U);
    EXPECT_EQ(FakeCanTp_TxBuf[0], 0x59U);
    EXPECT_EQ(FakeCanTp_TxBuf[1], DCM_DTC_SUBFUNC_REPORT_COUNT);
    EXPECT_EQ(FakeCanTp_TxBuf[5], 0U);  // countL
}

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, ReadDtcCount_OK_StatusMask0xFFMatchesNotCompletedSinceClear)
{
    /* 準備 (Arrange): [0x19, 0x01, statusMask=0xFF]。上のテストとの対比
     * （0x0A reportSupportedDTC が「ステータスに関わらず全件」を返すのとは
     * 異なり、0x01/0x02 はあくまでステータスマスクによる絞り込みである
     * ことを裏付ける）。 */
    uint8 req[3] = { DCM_SID_READ_DTC_INFO, DCM_DTC_SUBFUNC_REPORT_COUNT, 0xFFU };

    /* 実行 (Act) */
    SendReadDtcInfo(req, sizeof(req));

    /* 評価 (Assert): DEM_STATUS_NOT_COMPLETED_SINCE_CLEAR ビットが
     * DEM_STATUS_AVAILABILITY_MASK に含まれるため、Dem_Init() 直後の
     * 全イベントがこのビットを立てており、0xFF マスクには全件ヒットする。 */
    ASSERT_EQ(FakeCanTp_TransmitCount, 1U);
    ASSERT_EQ(FakeCanTp_TxLength, 6U);
    EXPECT_EQ(FakeCanTp_TxBuf[5], (uint8)DEM_EVENT_COUNT);  // countL
}

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, ReadDtcInfo_NG_UnsupportedSubFuncReturnsNegativeResponse)
{
    /* 準備 (Arrange): 未対応の subFunc（0x0A と離れた値を使い、将来 0x0B 等が
     * 追加されても意図せず衝突しないようにする） */
    uint8 req[2] = { DCM_SID_READ_DTC_INFO, 0x55U };

    /* 実行 (Act) */
    SendReadDtcInfo(req, sizeof(req));

    /* 評価 (Assert): [0x7F, 0x19, 0x12 subFunctionNotSupported] */
    ASSERT_EQ(FakeCanTp_TransmitCount, 1U);
    ASSERT_EQ(FakeCanTp_TxLength, 3U);
    EXPECT_EQ(FakeCanTp_TxBuf[0], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(FakeCanTp_TxBuf[1], DCM_SID_READ_DTC_INFO);
    EXPECT_EQ(FakeCanTp_TxBuf[2], DCM_NRC_SUB_FUNC_NOT_SUPPORTED);
}

TEST_F(Bsw_Dcm_ReadDtcInfo_Test, ReadDtcInfo_NG_TooShortRequestReturnsNegativeResponse)
{
    /* 準備 (Arrange): SID のみ（subFunc すら無い） */
    uint8 req[1] = { DCM_SID_READ_DTC_INFO };

    /* 実行 (Act) */
    SendReadDtcInfo(req, sizeof(req));

    /* 評価 (Assert): [0x7F, 0x19, 0x22 conditionsNotCorrect] */
    ASSERT_EQ(FakeCanTp_TransmitCount, 1U);
    ASSERT_EQ(FakeCanTp_TxLength, 3U);
    EXPECT_EQ(FakeCanTp_TxBuf[0], DCM_SID_NEGATIVE_RESP);
    EXPECT_EQ(FakeCanTp_TxBuf[2], DCM_NRC_CONDITIONS_NOT_CORRECT);
}

}  // namespace
