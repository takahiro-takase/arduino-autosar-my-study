/**
 * \file    E2E_P01Check_test.cpp
 * \brief   E2E_P01Check() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 E2E_test.cpp）。
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

/* E2E Profile01（E2E_P01Protect / E2E_P01Check など）の単体テスト用フィクスチャ。
 * SetUp(): Profile01 の設定（DataID=0x0110、データ長 5 バイト、最大カウンタ差 1、CRC・カウンタのオフセット、同期カウンタ初期値 2）を用意する。 */
class E2E_P01Check_Test : public ::testing::Test
{
protected:
    E2E_P01ConfigType config;

    void SetUp() override
    {
        /* AbsInfo (CAN 0x110, DLC=5) 相当: byte[0]=CRC8, byte[1]=Counter, byte[2-4]=データ */
        config.DataID          = 0x0110U;
        config.DataLength       = 5U;
        config.MaxDeltaCounter  = 1U;
        config.CounterOffset    = 1U;
        config.CRCOffset        = 0U;
        config.SyncCounterInit  = 2U;
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(E2E_P01Check_Test, E2E_P01Check_NG_NullPointerReturnsInputErrNullWithoutTouchingState)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    E2E_P01CheckStateType   checkState;
    E2E_P01ProtectStateType protectState;
    ASSERT_EQ(E2E_P01CheckInit(&checkState), E2E_E_OK);
    ASSERT_EQ(E2E_P01ProtectInit(&protectState), E2E_E_OK);
    uint8_t data[5] = {0U, 0U, 0U, 0U, 0U};
    const E2E_P01CheckStateType   checkStateBefore   = checkState;
    const E2E_P01ProtectStateType protectStateBefore = protectState;

    EXPECT_EQ(E2E_P01CheckInit(nullptr), E2E_E_INPUTERR_NULL);
    EXPECT_EQ(E2E_P01ProtectInit(nullptr), E2E_E_INPUTERR_NULL);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret1 = E2E_P01Check(nullptr, &checkState, data);
    Std_ReturnType ret2 = E2E_P01Check(&config, nullptr, data);
    Std_ReturnType ret3 = E2E_P01Check(&config, &checkState, nullptr);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret1, E2E_E_INPUTERR_NULL);
    EXPECT_EQ(ret2, E2E_E_INPUTERR_NULL);
    EXPECT_EQ(ret3, E2E_E_INPUTERR_NULL);
    EXPECT_EQ(E2E_P01Protect(nullptr, &protectState, data), E2E_E_INPUTERR_NULL);
    EXPECT_EQ(E2E_P01Protect(&config, nullptr, data), E2E_E_INPUTERR_NULL);
    EXPECT_EQ(E2E_P01Protect(&config, &protectState, nullptr), E2E_E_INPUTERR_NULL);

    /* NULL 引数エラー時は State を一切書き換えないこと（テスト名の裏付け） */
    EXPECT_EQ(checkState.LastValidCounter, checkStateBefore.LastValidCounter);
    EXPECT_EQ(checkState.Status, checkStateBefore.Status);
    EXPECT_EQ(checkState.WaitForFirstData, checkStateBefore.WaitForFirstData);
    EXPECT_EQ(checkState.SyncCounter, checkStateBefore.SyncCounter);
    EXPECT_EQ(protectState.Counter, protectStateBefore.Counter);
}

TEST_F(E2E_P01Check_Test, E2E_P01Check_NG_CrcMismatchReturnsWrongCrcAndDoesNotUpdateCounter)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    E2E_P01CheckStateType checkState;
    ASSERT_EQ(E2E_P01CheckInit(&checkState), E2E_E_OK);
    checkState.WaitForFirstData = 0U;
    checkState.LastValidCounter = 3U;

    uint8_t data[5] = {0U, 4U, 0U, 0U, 0U}; /* Counter=4 だが CRC は未計算(0)のまま=不一致 */

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = E2E_P01Check(&config, &checkState, data);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_EQ(ret, E2E_E_OK);

    EXPECT_EQ(checkState.Status, E2E_P01STATUS_WRONGCRC);
    EXPECT_EQ(checkState.LastValidCounter, 3U); /* CRC不一致時は状態を更新しない */
}

TEST_F(E2E_P01Check_Test, E2E_P01Check_NG_CounterJumpBeyondMaxDeltaTriggersWrongSequenceThenSyncUntilRelocked)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    E2E_P01ProtectStateType protectState;
    E2E_P01CheckStateType   checkState;
    ASSERT_EQ(E2E_P01ProtectInit(&protectState), E2E_E_OK);
    ASSERT_EQ(E2E_P01CheckInit(&checkState), E2E_E_OK);

    uint8_t frame0[5] = {0U, 0U, 0U, 0U, 0U};
    ASSERT_EQ(E2E_P01Protect(&config, &protectState, frame0), E2E_E_OK); /* Counter=0 */

    protectState.Counter = 3U; /* frame0(基準0)からdelta=3 > MaxDeltaCounter(1) */
    uint8_t frameJump[5] = {0U, 0U, 0U, 0U, 0U};
    ASSERT_EQ(E2E_P01Protect(&config, &protectState, frameJump), E2E_E_OK);

    /* 再ロック用のフレーム (SyncCounterInit=2回分) と、再ロック完了後のフレーム */
    enum { SYNC_FRAMES_MAX = 4 };
    ASSERT_LE(config.SyncCounterInit, SYNC_FRAMES_MAX);
    uint8_t frameSync[SYNC_FRAMES_MAX][5] = {};
    for (uint8_t i = 0U; i < config.SyncCounterInit; i++)
    {
        ASSERT_EQ(E2E_P01Protect(&config, &protectState, frameSync[i]), E2E_E_OK);
    }
    uint8_t frameRelocked[5] = {0U, 0U, 0U, 0U, 0U};
    ASSERT_EQ(E2E_P01Protect(&config, &protectState, frameRelocked), E2E_E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType retInitial = E2E_P01Check(&config, &checkState, frame0);  /* INITIAL、基準値=0 */

    Std_ReturnType retJump = E2E_P01Check(&config, &checkState, frameJump);
    E2E_P01CheckStateType stateJump = checkState;

    /* 再ロック中 (SyncCounterInit=2回分) は CRC/Counter が正常でも SYNC を返す */
    Std_ReturnType        retSync[SYNC_FRAMES_MAX];
    E2E_P01CheckStateType stateSync[SYNC_FRAMES_MAX];
    for (uint8_t i = 0U; i < config.SyncCounterInit; i++)
    {
        retSync[i]   = E2E_P01Check(&config, &checkState, frameSync[i]);
        stateSync[i] = checkState;
    }

    /* 再ロック完了後は通常の OK に戻る */
    Std_ReturnType retRelocked = E2E_P01Check(&config, &checkState, frameRelocked);
    E2E_P01CheckStateType stateRelocked = checkState;

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_EQ(retInitial, E2E_E_OK);

    ASSERT_EQ(retJump, E2E_E_OK);
    EXPECT_EQ(stateJump.Status, E2E_P01STATUS_WRONGSEQUENCE);
    EXPECT_EQ(stateJump.LastValidCounter, 3U);
    EXPECT_EQ(stateJump.SyncCounter, config.SyncCounterInit);

    for (uint8_t i = 0U; i < config.SyncCounterInit; i++)
    {
        ASSERT_EQ(retSync[i], E2E_E_OK);
        EXPECT_EQ(stateSync[i].Status, E2E_P01STATUS_SYNC);
    }

    ASSERT_EQ(retRelocked, E2E_E_OK);
    EXPECT_EQ(stateRelocked.Status, E2E_P01STATUS_OK);
}

TEST_F(E2E_P01Check_Test, E2E_P01Check_OK_FirstCheckAfterInitReturnsInitial)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    E2E_P01ProtectStateType protectState;
    E2E_P01CheckStateType   checkState;
    ASSERT_EQ(E2E_P01ProtectInit(&protectState), E2E_E_OK);
    ASSERT_EQ(E2E_P01CheckInit(&checkState), E2E_E_OK);

    uint8_t data[5] = {0U, 0U, 0x01U, 0x02U, 0x03U};
    ASSERT_EQ(E2E_P01Protect(&config, &protectState, data), E2E_E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = E2E_P01Check(&config, &checkState, data);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_EQ(ret, E2E_E_OK);
    EXPECT_EQ(checkState.Status, E2E_P01STATUS_INITIAL);
    EXPECT_EQ(checkState.LastValidCounter, 0U);
}

TEST_F(E2E_P01Check_Test, E2E_P01Check_OK_SecondConsecutiveFrame)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    E2E_P01ProtectStateType protectState;
    E2E_P01CheckStateType   checkState;
    ASSERT_EQ(E2E_P01ProtectInit(&protectState), E2E_E_OK);
    ASSERT_EQ(E2E_P01CheckInit(&checkState), E2E_E_OK);

    uint8_t frame1[5] = {0U, 0U, 0U, 0U, 0U};
    uint8_t frame2[5] = {0U, 0U, 0U, 0U, 0U};
    ASSERT_EQ(E2E_P01Protect(&config, &protectState, frame1), E2E_E_OK);
    ASSERT_EQ(E2E_P01Protect(&config, &protectState, frame2), E2E_E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret1 = E2E_P01Check(&config, &checkState, frame1);
    Std_ReturnType ret2 = E2E_P01Check(&config, &checkState, frame2);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    ASSERT_EQ(ret1, E2E_E_OK); /* INITIAL */
    ASSERT_EQ(ret2, E2E_E_OK);

    EXPECT_EQ(checkState.Status, E2E_P01STATUS_OK);
    EXPECT_EQ(checkState.LastValidCounter, 1U);
}

}  // namespace
