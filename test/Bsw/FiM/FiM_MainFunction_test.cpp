/**
 * \file    FiM_MainFunction_test.cpp
 * \brief   FiM_MainFunction() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 FiM_test.cpp）。
 *
 *          FiM.c は本プロジェクトでこれまでどの native テスト環境にもリンク
 *          されておらず、テストカバレッジが皆無だった。新設時、
 *          `FiM_Init()`が全FIDを無条件で「許可」初期化していた乖離
 *          （[SWS_Fim_00102]/[SWS_Fim_00104]。以前は起動直後から最初の
 *          `FiM_MainFunction()`呼び出しまでの間、既に確定済みのDTCがあって
 *          も誤って許可扱いになっていた）を是正したのに合わせ、専用環境を
 *          新設した。
 *
 *          分割前の共通の説明は、旧 FiM_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "FiM.h"
#include "Dem.h"
#include "Dem_Cfg.h"
#include "Fake_Millis.h"
#include "Fake_Det_Hw.h"
#include "NvM_TestUtil.h"
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

/* 1 つの FID（RUNNING_LED）に、2 つのイベント（CAN_BUSOFF、BUTTON_STUCK）の抑止の条件を並べた設定。
 * どちらか 1 つが成り立てば、RUNNING_LED を抑止する（OR、[SWS_Fim_00064]）。BUTTON_ACK には条件が無い（常に許可）。 */
const FiM_FunctionCfgType kTwoEventsRows[2] = {
    { FIM_FID_RUNNING_LED, DemConf_DemEventParameter_CAN_BUSOFF,    DEM_STATUS_CONFIRMED },
    { FIM_FID_RUNNING_LED, DemConf_DemEventParameter_BUTTON_STUCK,  DEM_STATUS_CONFIRMED }
};
const FiM_ConfigType kTwoEventsConfig = { kTwoEventsRows, 2U, FIM_FUNCTION_COUNT };

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* FiM の単体テスト用フィクスチャ。
 * SetUp(): 時刻を初期化し、Dem_Init(NULL) を呼ぶ。FiM_Init() は各テストが呼ぶ。GetPermission() は、機能の許可状態を取得して戻り値も確認する補助関数。
 * TearDown(): FiM を未初期化の状態へ戻す。 */
class FiM_MainFunction_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Dem_Init() のログはノイズになるため抑制

        NvMTest_ColdBoot();   // 空の EEPROM から NvM を初期化する（Dem_ClearDTC() は不揮発メモリへの書き込みを積むため）
        Dem_Init(NULL);       // 空の EEPROM なので、常に「初回起動」で確定的に再現される

        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        FiM_Test_ResetInitState();
        NvMTest_Shutdown();   // 次のテストへ NvM の初期化状態を残さない
    }

    static boolean GetPermission(FiM_FunctionIdType fid)
    {
        boolean permission = FALSE;
        const Std_ReturnType ret = FiM_GetFunctionPermission(fid, &permission);
        EXPECT_EQ(ret, E_OK);
        return permission;
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(FiM_MainFunction_Test, MainFunction_NG_UninitializedDoesNothing)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FiM_Test_ResetInitState();   // 未初期化にする
    FakeDetHw_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    FiM_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);   // 何もしない（DET も報告しない）
}

TEST_F(FiM_MainFunction_Test, MainFunction_OK_InhibitsFidWhenDtcConfirmedAfterInit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FiM_Init(&FiM_Config);
    ASSERT_EQ(GetPermission(FIM_FID_RUNNING_LED), TRUE);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    // 初期化後に Bus-Off が確定 (DEM_DEBOUNCE_LIMIT_CAN_BUSOFF=1
    // のため1回で確定)
    (void)Dem_SetEventStatus(DemConf_DemEventParameter_CAN_BUSOFF, DEM_EVENT_STATUS_FAILED);
    FiM_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    // RUNNING_LED のみ抑止され、無関係の BUTTON_ACK は影響なし
    EXPECT_EQ(GetPermission(FIM_FID_RUNNING_LED), FALSE);
    EXPECT_EQ(GetPermission(FIM_FID_BUTTON_ACK), TRUE);
}

TEST_F(FiM_MainFunction_Test, MainFunction_OK_PermitsFidAgainWhenDtcIsCleared)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FiM_Init(&FiM_Config);
    (void)Dem_SetEventStatus(DemConf_DemEventParameter_CAN_BUSOFF, DEM_EVENT_STATUS_FAILED);
    FiM_MainFunction();
    ASSERT_EQ(GetPermission(FIM_FID_RUNNING_LED), FALSE);   // いったん抑止される

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ASSERT_EQ(Dem_ClearDTC(0U, DEM_GROUP_ALL_DTCS, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY), E_OK);   // DTC を消去する
    FiM_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(GetPermission(FIM_FID_RUNNING_LED), TRUE);   // 確定していないので、再び許可される
}

// OK: 1 つの FID に 2 つの条件があるとき、1 つ目のイベントの確定で、その FID を抑止する。他の FID には、影響しない。
TEST_F(FiM_MainFunction_Test, MainFunction_OK_InhibitsFidWhenFirstOfTwoEventsIsConfirmed)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FiM_Init(&kTwoEventsConfig);
    (void)Dem_SetEventStatus(DemConf_DemEventParameter_CAN_BUSOFF, DEM_EVENT_STATUS_FAILED);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    FiM_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(GetPermission(FIM_FID_RUNNING_LED), FALSE);
    EXPECT_EQ(GetPermission(FIM_FID_BUTTON_ACK), TRUE);
}

// OK: 1 つの FID に 2 つの条件があるとき、2 つ目のイベントの確定でも、その FID を抑止する（以前は、最後の行だけが、FID の状態を決めていた）。
TEST_F(FiM_MainFunction_Test, MainFunction_OK_InhibitsFidWhenSecondOfTwoEventsIsConfirmed)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FiM_Init(&kTwoEventsConfig);
    (void)Dem_SetEventStatus(DemConf_DemEventParameter_BUTTON_STUCK, DEM_EVENT_STATUS_FAILED);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    FiM_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(GetPermission(FIM_FID_RUNNING_LED), FALSE);
}

// OK: 2 つの条件が両方成り立っていて、片方だけが解除されても、もう片方が成り立つ間は、抑止のまま。両方が解除されると、許可に戻る（OR、[SWS_Fim_00064]）。
TEST_F(FiM_MainFunction_Test, MainFunction_OK_StaysInhibitedWhileOneOfTwoEventsRemainsConfirmed)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    FiM_Init(&kTwoEventsConfig);
    (void)Dem_SetEventStatus(DemConf_DemEventParameter_CAN_BUSOFF, DEM_EVENT_STATUS_FAILED);
    (void)Dem_SetEventStatus(DemConf_DemEventParameter_BUTTON_STUCK, DEM_EVENT_STATUS_FAILED);
    FiM_MainFunction();
    ASSERT_EQ(GetPermission(FIM_FID_RUNNING_LED), FALSE);
    uint32 dtcBusOff = 0U;
    uint32 dtcStuck  = 0U;
    ASSERT_EQ(Dem_GetDTCOfEvent(DemConf_DemEventParameter_CAN_BUSOFF, DEM_DTC_FORMAT_UDS, &dtcBusOff), E_OK);
    ASSERT_EQ(Dem_GetDTCOfEvent(DemConf_DemEventParameter_BUTTON_STUCK, DEM_DTC_FORMAT_UDS, &dtcStuck), E_OK);
    ASSERT_EQ(Dem_ClearDTC(0U, dtcBusOff, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY), E_OK);   // 片方だけを解除する
    FiM_MainFunction();
    const boolean afterFirstCleared = GetPermission(FIM_FID_RUNNING_LED);
    ASSERT_EQ(Dem_ClearDTC(0U, dtcStuck, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY), E_OK);   // もう片方も解除する

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    FiM_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(afterFirstCleared, FALSE);   // もう片方が成り立つ間は、抑止のまま
    EXPECT_EQ(GetPermission(FIM_FID_RUNNING_LED), TRUE);
}

}  // namespace
