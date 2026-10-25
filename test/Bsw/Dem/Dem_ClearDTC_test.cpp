/**
 * \file    Dem_ClearDTC_test.cpp
 * \brief   Dem_ClearDTC() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 Dem_test.cpp）。
 *
 *          分割前の共通の説明は、旧 Dem_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Dem.h"
#include "Fake_Det_Hw.h"
#include "Wrap_Dem.h"
#include "NvM.h"
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

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* Dem の単体テスト用フィクスチャ。
 * SetUp(): wrap の状態を戻し、Dem_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。範囲外のイベント ID を表す kInvalidEventId も用意する。
 * TearDown(): 未初期化の状態へ戻す。 */
class Dem_ClearDTC_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        WrapDem_Reset();  // 他ファイルの故障注入が漏れ伝わらないよう防御的にリセット
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        NvMTest_ColdBoot();   // 空の EEPROM から NvM を初期化する（クリアは不揮発メモリへの書き込みを積むため）
        Dem_Init(NULL);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Dem_Test_ResetInitState();
        NvMTest_Shutdown();   // 次のテストへ NvM の初期化状態を残さない
    }

    static const Dem_EventIdType kInvalidEventId = DEM_EVENT_COUNT;
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Dem_ClearDTC_Test, Dem_ClearDTC_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dem_Test_ResetInitState();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Dem_ClearDTC(0U, DEM_GROUP_ALL_DTCS, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_UNINIT);
}

TEST_F(Dem_ClearDTC_Test, Dem_ClearDTC_NG_WrongConfiguration)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Dem_ClearDTC(0U, DEM_GROUP_ALL_DTCS, DEM_DTC_FORMAT_UDS, static_cast<Dem_DTCOriginType>(0xFFFFU));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, DEM_WRONG_DTCORIGIN);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_WRONG_CONFIGURATION);
}

TEST_F(Dem_ClearDTC_Test, Dem_ClearDTC_NG_UnsupportedDtcFormat)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Dem_ClearDTC(0U, DEM_GROUP_ALL_DTCS, DEM_DTC_FORMAT_OBD, DEM_DTC_ORIGIN_PRIMARY_MEMORY);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, DEM_WRONG_DTC);   // UDS 形式以外は、該当する DTC が無いものとして返す
}

TEST_F(Dem_ClearDTC_Test, Dem_ClearDTC_NG_UnknownDtc)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Dem_ClearDTC(0U, 0x123456UL, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY);   // どのイベントにも割り当てられていない DTC

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, DEM_WRONG_DTC);
}

// NG: 不揮発メモリへの書き込みを積めなかった（NvM のブロックが書き込み保護中）ときは、DEM_CLEAR_MEMORY_ERROR（[SWS_Dem_01057]）。
TEST_F(Dem_ClearDTC_Test, Dem_ClearDTC_NG_NvmWriteFailureIsReportedAsMemoryError)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(NvM_SetBlockProtection(NVM_BLOCK_ID_DEM_STATUS, TRUE), E_OK);   // NvM_WriteBlock() が失敗する

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Dem_ClearDTC(0U, DEM_GROUP_ALL_DTCS, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, DEM_CLEAR_MEMORY_ERROR);
}

// OK: 全 DTC のクリアは、E_OK を返す（不揮発メモリへの書き込みを積めたとき）。
TEST_F(Dem_ClearDTC_Test, Dem_ClearDTC_OK_ClearsAllDtcs)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で NvM と Dem を初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Dem_ClearDTC(0U, DEM_GROUP_ALL_DTCS, DEM_DTC_FORMAT_UDS, DEM_DTC_ORIGIN_PRIMARY_MEMORY);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
}

}  // namespace
