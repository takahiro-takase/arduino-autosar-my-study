/**
 * \file    E2E_MapStatusToSM_test.cpp
 * \brief   E2E_MapStatusToSM() の単体テスト
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

/* E2E Profile05（E2E_P05Protect / E2E_P05Check など）の単体テスト用フィクスチャ。
 * SetUp(): Profile05 の設定（DataID=0x0220、データ長 5 バイト、最大カウンタ差 1）を用意する。 */
class E2E_MapStatusToSM_Test : public ::testing::Test
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

/* E2E Profile01（E2E_P01Protect / E2E_P01Check など）の単体テスト用フィクスチャ。
 * SetUp(): Profile01 の設定（DataID=0x0110、データ長 5 バイト、最大カウンタ差 1、CRC・カウンタのオフセット、同期カウンタ初期値 2）を用意する。 */
class E2E_MapStatusToSM_P01_Test : public ::testing::Test
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

TEST_F(E2E_MapStatusToSM_Test, MapStatusToSM_NG_NonOkCheckReturnAlwaysMapsToErrorRegardlessOfStatus)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    /* [SWS_E2E_00454] */
    E2E_PCheckStatusType ret = E2E_P05MapStatusToSM(E2E_E_INPUTERR_NULL, E2E_P05STATUS_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E2E_P_ERROR);
}

TEST_F(E2E_MapStatusToSM_P01_Test, MapStatusToSM_NG_NonOkCheckReturnAlwaysMapsToErrorRegardlessOfStatusOrBehavior)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    /* [SWS_E2E_00384] */
    E2E_PCheckStatusType ret1 = E2E_P01MapStatusToSM(E2E_E_INPUTERR_NULL, E2E_P01STATUS_OK, 1U);
    E2E_PCheckStatusType ret2 = E2E_P01MapStatusToSM(E2E_E_INPUTERR_NULL, E2E_P01STATUS_OK, 0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret1, E2E_P_ERROR);
    EXPECT_EQ(ret2, E2E_P_ERROR);
}

TEST_F(E2E_MapStatusToSM_Test, MapStatusToSM_OK_MapsEachStatusPerSpecTable)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    /* [SWS_E2E_00453] */
    E2E_PCheckStatusType retOk            = E2E_P05MapStatusToSM(E2E_E_OK, E2E_P05STATUS_OK);
    E2E_PCheckStatusType retOkSomeLost    = E2E_P05MapStatusToSM(E2E_E_OK, E2E_P05STATUS_OKSOMELOST);
    E2E_PCheckStatusType retError         = E2E_P05MapStatusToSM(E2E_E_OK, E2E_P05STATUS_ERROR);
    E2E_PCheckStatusType retRepeated      = E2E_P05MapStatusToSM(E2E_E_OK, E2E_P05STATUS_REPEATED);
    E2E_PCheckStatusType retNoNewData     = E2E_P05MapStatusToSM(E2E_E_OK, E2E_P05STATUS_NONEWDATA);
    E2E_PCheckStatusType retWrongSequence = E2E_P05MapStatusToSM(E2E_E_OK, E2E_P05STATUS_WRONGSEQUENCE);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(retOk, E2E_P_OK);
    EXPECT_EQ(retOkSomeLost, E2E_P_OK);
    EXPECT_EQ(retError, E2E_P_ERROR);
    EXPECT_EQ(retRepeated, E2E_P_REPEATED);
    EXPECT_EQ(retNoNewData, E2E_P_NONEWDATA);
    EXPECT_EQ(retWrongSequence, E2E_P_WRONGSEQUENCE);
}

TEST_F(E2E_MapStatusToSM_P01_Test, MapStatusToSM_OK_R42BehaviorGroupsSyncWithOkAndInitialWithWrongSequence)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    /* [SWS_E2E_00383]: profileBehavior=1 (TRUE) */
    E2E_PCheckStatusType retOk            = E2E_P01MapStatusToSM(E2E_E_OK, E2E_P01STATUS_OK, 1U);
    E2E_PCheckStatusType retOkSomeLost    = E2E_P01MapStatusToSM(E2E_E_OK, E2E_P01STATUS_OKSOMELOST, 1U);
    E2E_PCheckStatusType retSync          = E2E_P01MapStatusToSM(E2E_E_OK, E2E_P01STATUS_SYNC, 1U);
    E2E_PCheckStatusType retWrongCrc      = E2E_P01MapStatusToSM(E2E_E_OK, E2E_P01STATUS_WRONGCRC, 1U);
    E2E_PCheckStatusType retRepeated      = E2E_P01MapStatusToSM(E2E_E_OK, E2E_P01STATUS_REPEATED, 1U);
    E2E_PCheckStatusType retNoNewData     = E2E_P01MapStatusToSM(E2E_E_OK, E2E_P01STATUS_NONEWDATA, 1U);
    E2E_PCheckStatusType retWrongSequence = E2E_P01MapStatusToSM(E2E_E_OK, E2E_P01STATUS_WRONGSEQUENCE, 1U);
    E2E_PCheckStatusType retInitial       = E2E_P01MapStatusToSM(E2E_E_OK, E2E_P01STATUS_INITIAL, 1U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(retOk, E2E_P_OK);
    EXPECT_EQ(retOkSomeLost, E2E_P_OK);
    EXPECT_EQ(retSync, E2E_P_OK);
    EXPECT_EQ(retWrongCrc, E2E_P_ERROR);
    EXPECT_EQ(retRepeated, E2E_P_REPEATED);
    EXPECT_EQ(retNoNewData, E2E_P_NONEWDATA);
    EXPECT_EQ(retWrongSequence, E2E_P_WRONGSEQUENCE);
    EXPECT_EQ(retInitial, E2E_P_WRONGSEQUENCE);
}

TEST_F(E2E_MapStatusToSM_P01_Test, MapStatusToSM_OK_PreR42BehaviorGroupsInitialWithOkAndSyncWithWrongSequence)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    /* [SWS_E2E_00476]: profileBehavior=0 (FALSE)。TRUEだとちょうど
     * INITIAL/SYNCの帰属が入れ替わる点のみ異なる。 */
    E2E_PCheckStatusType retOk            = E2E_P01MapStatusToSM(E2E_E_OK, E2E_P01STATUS_OK, 0U);
    E2E_PCheckStatusType retOkSomeLost    = E2E_P01MapStatusToSM(E2E_E_OK, E2E_P01STATUS_OKSOMELOST, 0U);
    E2E_PCheckStatusType retInitial       = E2E_P01MapStatusToSM(E2E_E_OK, E2E_P01STATUS_INITIAL, 0U);
    E2E_PCheckStatusType retWrongCrc      = E2E_P01MapStatusToSM(E2E_E_OK, E2E_P01STATUS_WRONGCRC, 0U);
    E2E_PCheckStatusType retRepeated      = E2E_P01MapStatusToSM(E2E_E_OK, E2E_P01STATUS_REPEATED, 0U);
    E2E_PCheckStatusType retNoNewData     = E2E_P01MapStatusToSM(E2E_E_OK, E2E_P01STATUS_NONEWDATA, 0U);
    E2E_PCheckStatusType retWrongSequence = E2E_P01MapStatusToSM(E2E_E_OK, E2E_P01STATUS_WRONGSEQUENCE, 0U);
    E2E_PCheckStatusType retSync          = E2E_P01MapStatusToSM(E2E_E_OK, E2E_P01STATUS_SYNC, 0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(retOk, E2E_P_OK);
    EXPECT_EQ(retOkSomeLost, E2E_P_OK);
    EXPECT_EQ(retInitial, E2E_P_OK);
    EXPECT_EQ(retWrongCrc, E2E_P_ERROR);
    EXPECT_EQ(retRepeated, E2E_P_REPEATED);
    EXPECT_EQ(retNoNewData, E2E_P_NONEWDATA);
    EXPECT_EQ(retWrongSequence, E2E_P_WRONGSEQUENCE);
    EXPECT_EQ(retSync, E2E_P_WRONGSEQUENCE);
}

}  // namespace
