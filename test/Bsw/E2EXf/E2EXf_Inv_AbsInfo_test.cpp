/**
 * \file    E2EXf_Inv_AbsInfo_test.cpp
 * \brief   E2EXf_Inv_AbsInfo() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 E2EXf_test.cpp）。
 *
 *          `E2EXf_Inv_EngineInfo()`/`E2EXf_Inv_AbsInfo()` それぞれの
 *          「`E2E_P05Check()` 自体が失敗した場合」の防御分岐
 *          （`E2EXF_E_PARAM_POINTER` 追加報告）は、両関数とも
 *          `E2E_P05Check()` へ常に固定長（`E2EConfig->DataLength` そのもの）を
 *          渡す呼び出し規約のため、`E2E_P05.c` 内の「wrong length」条件
 *          （`Length != Config->DataLength`）に到達せず、かつ Config/State も
 *          常に非NULLの static 構造体を渡すため「NULL」条件にも到達しない
 *          （E2EXf.c 内のコメント「通常は到達しない」、E2E_P05.c 内のコメント
 *          「この分岐自体は現状到達しない」といずれも明記されている）。対象外
 *          とする。
 *
 *          分割前の共通の説明は、旧 E2EXf_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "E2EXf.h"
#include "Fake_Det_Hw.h"
#include "Wrap_E2E.h"
#include "Wrap_E2E_P05.h"
#include "E2E_P05.h"
#include "E2EXf_PBCfg.h"
#include "Dem.h"
#include "Wrap_Dem.h"
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

// 検証対象フレームの組み立て用のローカル E2E 設定（本番の E2EXf_AbsInfoCfgP05 と同じ
// DataID/DataLength/MaxDeltaCounter/Offset）。
const E2E_P05ConfigType kRefAbsInfoCfg = {
    0x0110U,  /* DataID */
    6U,       /* DataLength */
    1U,       /* MaxDeltaCounter */
    0U        /* Offset */
};

// kRefAbsInfoCfg に基づき、独立した Protect 状態でフレームを組み立てる。
void BuildAbsFrame(uint8 (&buf)[6], E2E_P05ProtectStateType* state)
{
    buf[3] = 0x01U;
    buf[4] = 0x02U;
    buf[5] = 0x03U;
    E2E_P05Protect(&kRefAbsInfoCfg, state, buf, 6U);
}

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* E2EXf（E2E Transformer）の単体テスト用フィクスチャ。
 * SetUp(): E2EXf_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): E2EXf_DeInit() で未初期化へ戻す。 */
class E2EXf_Inv_AbsInfo_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        WrapE2E_Reset();               // 他のテストファイルの故障注入（E2E_SMCheck など）が漏れ伝わらないようリセット
        WrapE2EP05_Reset();
        E2EXf_Init(NULL);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        E2EXf_DeInit();  // 既に未初期化でも Det 報告されるだけで無害
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(E2EXf_Inv_AbsInfo_Test, E2EXf_Inv_AbsInfo_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    E2EXf_DeInit();
    uint8  buf[6];
    uint32 bufLen = sizeof(buf);
    E2E_P05StatusType checkStatus;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    uint8 ret = E2EXf_Inv_AbsInfo(buf, &bufLen, NULL, 0U, &checkStatus);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_SAFETY_HARD_RUNTIMEERROR);
    EXPECT_EQ(checkStatus, E2E_P05STATUS_ERROR);
    EXPECT_EQ(FakeDetHw_LastErrorId, E2EXF_E_UNINIT);
}

TEST_F(E2EXf_Inv_AbsInfo_Test, E2EXf_Inv_AbsInfo_NG_NullCheckStatusPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8  buf[6];
    uint32 bufLen = sizeof(buf);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    uint8 ret = E2EXf_Inv_AbsInfo(buf, &bufLen, NULL, 0U, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_SAFETY_HARD_RUNTIMEERROR);
    EXPECT_EQ(FakeDetHw_LastErrorId, E2EXF_E_PARAM_POINTER);
}

TEST_F(E2EXf_Inv_AbsInfo_Test, E2EXf_Inv_AbsInfo_NG_NullBufferPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint32 bufLen = 6U;
    E2E_P05StatusType checkStatus;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    uint8 ret = E2EXf_Inv_AbsInfo(NULL, &bufLen, NULL, 0U, &checkStatus);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_SAFETY_HARD_RUNTIMEERROR);
    EXPECT_EQ(checkStatus, E2E_P05STATUS_ERROR);
    EXPECT_EQ(FakeDetHw_LastErrorId, E2EXF_E_PARAM_POINTER);
}

TEST_F(E2EXf_Inv_AbsInfo_Test, E2EXf_Inv_AbsInfo_NG_NullBufferLengthPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 buf[6] = { 0U };
    E2E_P05StatusType checkStatus;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    uint8 ret = E2EXf_Inv_AbsInfo(buf, NULL, NULL, 0U, &checkStatus);   // CheckStatus は有効、bufferLength だけ NULL

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_SAFETY_HARD_RUNTIMEERROR);
    EXPECT_EQ(FakeDetHw_LastErrorId, E2EXF_E_PARAM_POINTER);
}

TEST_F(E2EXf_Inv_AbsInfo_Test, E2EXf_Inv_AbsInfo_NG_FirstFrameWithBadCrcStaysErrorAndKeepsWaitingForFirstData)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    WrapDem_Reset();
    Dem_Init(NULL);
    E2EXf_PBCfg_Init();   // 初回受信の特別扱い（WaitForFirstData）と状態機械を初期状態へ戻す
    E2E_P05ProtectStateType refState;
    E2E_P05ProtectInit(&refState);
    uint8 bad[6] = { 0U };
    BuildAbsFrame(bad, &refState);
    bad[3] ^= 0x01U;   // CRC と合わなくなる
    uint8 good[6] = { 0U };
    BuildAbsFrame(good, &refState);
    uint32 bufLen = 0U;
    E2E_P05StatusType status = E2E_P05STATUS_OK;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    (void)E2EXf_Inv_AbsInfo(bad, &bufLen, NULL, 0U, &status);
    const E2E_P05StatusType statusOfBadFrame = status;
    (void)E2EXf_Inv_AbsInfo(good, &bufLen, NULL, 0U, &status);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(statusOfBadFrame, E2E_P05STATUS_ERROR);   // 初回でも CRC エラーは OK に格上げしない
    EXPECT_EQ(status, E2E_P05STATUS_OK);                // 次の正しいフレームが、初回として受理される（待ちが残っている）
    EXPECT_EQ(bufLen, 6U);
}

TEST_F(E2EXf_Inv_AbsInfo_Test, E2EXf_Inv_AbsInfo_OK_FirstValidFrameIsAcceptedThenNormalCounterCheck)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    WrapDem_Reset();
    Dem_Init(NULL);
    E2EXf_PBCfg_Init();   // 初回受信の特別扱い（WaitForFirstData）と状態機械を初期状態へ戻す
    E2E_P05ProtectStateType refState;
    E2E_P05ProtectInit(&refState);
    uint8 frame1[6] = { 0U };
    uint8 frame2[6] = { 0U };
    BuildAbsFrame(frame1, &refState);
    BuildAbsFrame(frame2, &refState);   // カウンタは frame1 の次の値
    uint32 bufLen = 0U;
    E2E_P05StatusType status1 = E2E_P05STATUS_ERROR;
    E2E_P05StatusType status2 = E2E_P05STATUS_ERROR;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    (void)E2EXf_Inv_AbsInfo(frame1, &bufLen, NULL, 0U, &status1);
    (void)E2EXf_Inv_AbsInfo(frame2, &bufLen, NULL, 0U, &status2);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(status1, E2E_P05STATUS_OK);   // 初回の正しいフレームは OK（待ちを解除する）
    EXPECT_EQ(status2, E2E_P05STATUS_OK);   // 2 回目以降は通常のカウンタ判定（差 1 は OK）
    EXPECT_EQ(bufLen, 6U);
}

TEST_F(E2EXf_Inv_AbsInfo_Test, E2EXf_Inv_AbsInfo_OK_RepeatedCrcErrorsReportDemFailed)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    WrapDem_Reset();
    Dem_Init(NULL);
    E2EXf_PBCfg_Init();
    E2E_P05ProtectStateType refState;
    E2E_P05ProtectInit(&refState);
    uint32 bufLen = 0U;
    E2E_P05StatusType status = E2E_P05STATUS_OK;
    bool failedReported = false;
    uint8 first[6] = { 0U };
    BuildAbsFrame(first, &refState);
    (void)E2EXf_Inv_AbsInfo(first, &bufLen, NULL, 0U, &status);   // 最初の正しいフレーム（状態機械が NODATA から出る）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    for (uint8 i = 0U; (i < 60U) && !failedReported; i++)
    {
        uint8 bad[6] = { 0U };
        BuildAbsFrame(bad, &refState);
        bad[3] ^= 0x01U;   // CRC と合わなくなるフレームを、繰り返し受信する
        (void)E2EXf_Inv_AbsInfo(bad, &bufLen, NULL, 0U, &status);
        failedReported = (CallCount_Dem_SetEventStatus > 0U) && (LastEventStatus_Dem_SetEventStatus == DEM_EVENT_STATUS_FAILED);
    }

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_TRUE(failedReported);   // 状態機械が INVALID になり、Dem へ FAILED を報告する
}

}  // namespace
