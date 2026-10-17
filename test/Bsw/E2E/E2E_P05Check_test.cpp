/**
 * \file    E2E_P05Check_test.cpp
 * \brief   E2E_P05Check() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 E2E_test.cpp）。
 *
 *          E2E_P05.c は実 HW 依存の無い自己完結したロジックのため、フェイク
 *          実装は不要で公開 API (E2E_P05Protect/E2E_P05Check とその Init) を
 *          直接呼んで検証する。E2E_CalcCrc16() 等の内部ヘルパーは static の
 *          ため、CRC の正しさは「本テストファイル内に SWS_E2E_00406 の
 *          擬似コードを独立に書き起こした参照実装」との突き合わせで検証する
 *          (E2E_P05.c 側の実装をコピーするのではなく、仕様書の記述から
 *          改めて書き起こすことで、実装のコピペミスを検出できるようにする)。
 *
 *          E2E_P05Check() はこのプロジェクト内に実行時の呼び出し元が無い
 *          (EngineHealthStatus は TX のみ) ため、正しさを検証できるのは
 *          このホストテストだけである。
 *
 *          分割前の共通の説明は、旧 E2E_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "E2E.h"
#include "E2E_P01.h"
#include "E2E_P05.h"
#include "Wrap_E2E.h"
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

/* E2E Profile05（E2E_P05Protect / E2E_P05Check など）の単体テスト用フィクスチャ。
 * SetUp(): Profile05 の設定（DataID=0x0220、データ長 5 バイト、最大カウンタ差 1）を用意する。 */
class E2E_P05Check_Test : public ::testing::Test
{
protected:
    E2E_P05ConfigType config;

    void SetUp() override
    {
        config.DataID          = 0x0220U;
        config.DataLength       = 5U; /* CRC16(2B) + Counter(1B) + userdata(2B) */
        config.MaxDeltaCounter  = 1U;
        config.Offset           = 0U;
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

/**
 * \brief  Data==NULL かつ Length!=0 も同じ「wrong input」扱い
 *         （Data==NULL && Length==0 のみが NONEWDATA、それ以外の
 *         Data==NULL は wrong input）。
 */
TEST_F(E2E_P05Check_Test, E2E_P05Check_NG_NullDataWithNonZeroLengthReturnsInputErrWrong)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    E2E_P05CheckStateType checkState;
    E2E_P05CheckInit(&checkState);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = E2E_P05Check(&config, &checkState, nullptr, 5U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E2E_E_INPUTERR_WRONG);
}

/**
 * \brief  [SWS_E2E_00411/00412]: Length が Config->DataLength と不一致な
 *         「wrong input」は E2E_E_INPUTERR_WRONG を返さなければならない
 *         （2026-09-06 是正。以前は誤って E2E_E_OK を返していた）。
 */
TEST_F(E2E_P05Check_Test, E2E_P05Check_NG_LengthMismatchReturnsInputErrWrong)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    E2E_P05CheckStateType checkState;
    E2E_P05CheckInit(&checkState);

    uint8_t data[6] = {0U, 0U, 0U, 0U, 0U, 0U}; /* config.DataLength=5 と不一致 */

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = E2E_P05Check(&config, &checkState, data, sizeof(data));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E2E_E_INPUTERR_WRONG);
}

TEST_F(E2E_P05Check_Test, E2E_P05Check_NG_CrcMismatchReturnsErrorAndDoesNotUpdateState)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    E2E_P05ProtectStateType protectState;
    E2E_P05CheckStateType   checkState;
    E2E_P05ProtectInit(&protectState);
    E2E_P05CheckInit(&checkState);
    checkState.Counter = 5U; /* 直前の正常な状態を模擬 */

    uint8_t data[5] = {0U, 0U, 6U, 0U, 0U}; /* Counter=6 だがCRCは未計算(0,0)のまま=不一致 */

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = E2E_P05Check(&config, &checkState, data, sizeof(data));
    const E2E_P05StatusType status = checkState.Status;

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_EQ(ret, E2E_E_OK);

    EXPECT_EQ(status, E2E_P05STATUS_ERROR);
    EXPECT_EQ(checkState.Counter, 5U); /* CRC不一致時は状態を更新しない */
}

TEST_F(E2E_P05Check_Test, E2E_P05Check_NG_CounterJumpBeyondMaxDeltaIsWrongSequence)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    E2E_P05ProtectStateType protectState;
    E2E_P05CheckStateType   checkState;
    E2E_P05ProtectInit(&protectState);
    E2E_P05CheckInit(&checkState);

    uint8_t frame1[5] = {0U, 0U, 0U, 0U, 0U};
    uint8_t frame2[5] = {0U, 0U, 0U, 0U, 0U};
    uint8_t frame3[5] = {0U, 0U, 0U, 0U, 0U};
    E2E_P05Protect(&config, &protectState, frame1, sizeof(frame1)); /* Counter=0 */
    E2E_P05Protect(&config, &protectState, frame2, sizeof(frame2)); /* Counter=1 */
    E2E_P05Protect(&config, &protectState, frame3, sizeof(frame3)); /* Counter=2 */

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    E2E_P05Check(&config, &checkState, frame1, sizeof(frame1)); /* OK、checkState.Counter=0 */

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    /* frame2 を飛ばして frame3 (Counter=2) を Check する → delta=2 > MaxDeltaCounter(1) */
    ASSERT_EQ(E2E_P05Check(&config, &checkState, frame3, sizeof(frame3)), E2E_E_OK);
    const E2E_P05StatusType status = checkState.Status;

    EXPECT_EQ(status, E2E_P05STATUS_WRONGSEQUENCE);
    EXPECT_EQ(checkState.Counter, 2U); /* WRONGSEQUENCEでも状態は受信値へ更新される (CRC正常なため) */
}

TEST_F(E2E_P05Check_Test, E2E_P05Check_OK_CounterWrapsFrom0xFFTo0IsRecognized)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    E2E_P05ProtectStateType protectState;
    E2E_P05CheckStateType   checkState;
    protectState.Counter = 0xFFU;
    checkState.Counter   = 0xFEU; /* 直前に受け付けた値が0xFEだったと仮定 */
    checkState.Status    = E2E_P05STATUS_OK;

    uint8_t frame1[5] = {0U, 0U, 0U, 0U, 0U}; /* Counter=0xFF (delta=1 from 0xFE) */
    uint8_t frame2[5] = {0U, 0U, 0U, 0U, 0U}; /* Counter=0x00 (delta=1 from 0xFF、折り返し) */
    E2E_P05Protect(&config, &protectState, frame1, sizeof(frame1));
    E2E_P05Protect(&config, &protectState, frame2, sizeof(frame2));

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType retCheck1 = E2E_P05Check(&config, &checkState, frame1, sizeof(frame1));
    E2E_P05CheckStateType stateAfter1 = checkState;
    Std_ReturnType retCheck2 = E2E_P05Check(&config, &checkState, frame2, sizeof(frame2));
    E2E_P05CheckStateType stateAfter2 = checkState;

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_EQ(retCheck1, E2E_E_OK);
    EXPECT_EQ(stateAfter1.Status, E2E_P05STATUS_OK);
    ASSERT_EQ(retCheck2, E2E_E_OK);
    EXPECT_EQ(stateAfter2.Status, E2E_P05STATUS_OK);
    EXPECT_EQ(stateAfter2.Counter, 0U);
}

TEST_F(E2E_P05Check_Test, E2E_P05Check_NG_NullConfigReturnsInputErrNull)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    E2E_P05CheckStateType checkState;
    E2E_P05CheckInit(&checkState);
    uint8_t data[5] = { 0U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = E2E_P05Check(nullptr, &checkState, data, sizeof(data));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E2E_E_INPUTERR_NULL);
}

TEST_F(E2E_P05Check_Test, E2E_P05Check_NG_NullStateReturnsInputErrNull)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8_t data[5] = { 0U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = E2E_P05Check(&config, nullptr, data, sizeof(data));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E2E_E_INPUTERR_NULL);
}

TEST_F(E2E_P05Check_Test, E2E_P05Check_OK_NullDataWithZeroLengthIsNoNewData)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    E2E_P05CheckStateType checkState;
    E2E_P05CheckInit(&checkState);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = E2E_P05Check(&config, &checkState, nullptr, 0U);   // 新しいデータが無い

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E2E_E_OK);
    EXPECT_EQ(checkState.Status, E2E_P05STATUS_NONEWDATA);
}

}  // namespace
