/**
 * \file    CanSM_MainFunction_test.cpp
 * \brief   CanSM_MainFunction() の単体テスト
 * \details 2026-10-18、モジュールの単体テストを関数単位のファイルへ分割した（旧 CanSM_test.cpp）。
 *
 *          分割前の共通の説明は、旧 CanSM_test.cpp の冒頭コメント（git の履歴）を参照。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Can.h"
#include "Can_Hw.h"
#include "CanIf.h"
#include "CanSM.h"
#include "CanSM_Cfg.h"
#include "ComM.h"
#include "CanNm.h"
#include "Nm.h"
#include "Fake_Can_Hw.h"
#include "Fake_Millis.h"
#include "Fake_Det_Hw.h"
#include "Wrap_Dem.h"
#include "Fake_Bsw_EcuM.h"
#include "Wrap_BswM.h"
#include "Wrap_CanIf.h"
#include "Wrap_Can.h"
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

const CanIf_ConfigType kBusOffRecoveryCanIfConfig = {
    /* TxPduConfig */ NULL,
    /* TxPduCount */  0U,
    /* RxPduConfig */ NULL,
    /* RxPduCount */  0U
};

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* CanSM の Bus-Off 回復（再試行を含む）の単体テスト用フィクスチャ。
 * SetUp(): 模擬 Can_Hw・Dem・EcuM・BswM と wrap をリセットし、Dem_Init(NULL)・Can_Init()・CanIf_Init(&kBusOffRecoveryCanIfConfig)・CanSM_Init(NULL)・ComM_Init(NULL)（起動時と同じく通信を許可）・CanNm_Init(NULL)・Nm_Init(NULL) の順に呼び、FULL_COM の状態を用意する（NmStack_SleepCoordination_test.cpp の ArrangeFullCom() と同じ流儀）。 */
class CanSM_MainFunction_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeCanHw_Reset();
        WrapDem_Reset();
        FakeEcuM_Reset();
        WrapBswM_Reset();
        FakeMillis_Reset();
        WrapCanIf_Reset();
        WrapCan_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        Dem_Init(NULL);  // Demの内部状態を毎テスト決定的にリセットする（Fake_NvM.cにより常に「初回起動」）

        canConfig.filter.filterId = 0x0220U;
        canConfig.filter.mask     = 0x1FFFU;
        canConfig.csPin           = 10U;
        canConfig.intPin          = 2U;
        canConfig.baudrate        = 500000U;
        canConfig.crystalFreq     = CAN_CRYSTAL_16MHZ;

        Can_Init(&canConfig);
        CanIf_Init(&kBusOffRecoveryCanIfConfig);
        CanSM_Init(NULL);
        ComM_Init(NULL);
        ComM_CommunicationAllowed(COMM_CHANNEL_0, TRUE);  // 実 EcuM_Init() と同じく起動時に許可
        CanNm_Init(NULL);
        Nm_Init(NULL);

        // FULL_COM を Arrange する（NmStack_SleepCoordination_test.cpp の
        // ArrangeFullCom() と同じ流儀。CanNm を Repeat Message State から
        // 抜けさせておかないと、この後の CanSM_ControllerBusOff() が
        // ComM 経由で動かす CanNm 協調ロジックが未検証の中途半端な状態から
        // 始まってしまう）。
        ASSERT_EQ(ComM_RequestComMode(COMM_USER_0, COMM_FULL_COMMUNICATION), E_OK);
        ASSERT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);

        FakeMillis_Value += CANNM_REPEAT_MESSAGE_MS + 100UL;
        CanNm_MainFunction();
        CanNm_StateType nmState;
        CanNm_ModeType  nmMode;
        ASSERT_EQ(CanNm_GetState(CANNM_MAIN_NETWORK_HANDLE, &nmState, &nmMode), E_OK);
        ASSERT_EQ(nmState, CANNM_STATE_NORMAL_OPERATION);

        FakeCanHw_Reset();
        WrapDem_Reset();
        FakeEcuM_Reset();
        WrapBswM_Reset();
        WrapCanIf_Reset();
        WrapCan_Reset();

        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;  // DeInit() のログを抑制
        CanNm_DeInit();
        ComM_DeInit();
        CanSM_DeInit();
        CanIf_DeInit();
    }

    /* Bus-Off を発生させ、L1 周期（CANSM_BUSOFF_RECOVERY_L1_MS）を
     * 超過させた直後の状態まで進める Arrange ヘルパー。 */
    void ArrangeBusOffPastL1Interval()
    {
        CanSM_ControllerBusOff(0U);
        ASSERT_EQ(Can_Test_GetControllerState(), CAN_CS_STOPPED);

        FakeMillis_Value += static_cast<unsigned long>(CANSM_BUSOFF_RECOVERY_L1_MS) + 1UL;

        FakeCanHw_SetModeCount = 0U;
        WrapDem_Reset();
        WrapCanIf_Reset();
        WrapCan_Reset();
    }

    Can_ConfigType canConfig;
};

/**
 * \details 上記セクションは Bus-Off 回復のリトライ/バックオフ状態機械のみを
 *          検証しており、`Det_ReportError()` の報告内容はどこからも
 *          検証されていなかった。本セクションは CanSM.c の全
 *          `Det_ReportError()` 呼び出し箇所（NG ケースのみ）をまとめ、
 *          報告される ErrorId が仕様どおり正しい値になっていることを
 *          `Fake_Det_Hw.h`（`FakeDetHw_LastErrorId`）で検証する。
 *
 *          CanSM には `CanSM_DeInit()` が存在するため、Mcu/PduR/CanTp と
 *          異なり「未初期化状態」の検証にテスト専用のリセット関数は不要
 *          （各テストが明示的に `CanSM_DeInit()` を呼んでから検証する）。
 *
 *          検証対象はいずれも Can/CanIf への実処理カスケードへ到達する前に
 *          reject される分岐のみのため（CanSM.c 参照）、Can_Init()/CanIf_Init()
 *          は不要（他モジュールのテストの残留 static 状態と競合しない）。
 */

/* CanSM（Bus-Off 回復以外の API）の単体テスト用フィクスチャ。
 * SetUp(): CanSM_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): CanSM_DeInit() で未初期化へ戻す。 */
class CanSM_MainFunction_Base_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        CanSM_Init(NULL);
        FakeDetHw_Reset();              // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        CanSM_DeInit();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(CanSM_MainFunction_Base_Test, CanSM_MainFunction_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    CanSM_DeInit();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanSM_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, CANSM_E_UNINIT);
}

// ------------------------------------------------------------
// 異常系: CanIf_SetControllerMode(STARTED) が失敗する回、コントローラは
// BUS_OFF のまま据え置かれ、次周期の再試行に委ねられる（CanSM.c 726〜736
// 行目、これまで未到達だった防御分岐）。
// ------------------------------------------------------------

TEST_F(CanSM_MainFunction_Test, MainFunction_NG_RecoveryAttemptFails_StaysInBusOffForNextRetry)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ArrangeBusOffPastL1Interval();
    FailFromCallCount_CanIf_SetControllerMode = 1U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanSM_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    // 回復を試みたが失敗 → BUS_OFF のまま
    EXPECT_EQ(CallCount_CanIf_SetControllerMode, 1U);
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STOPPED);
    ComM_ModeType mode = COMM_FULL_COMMUNICATION;
    ASSERT_EQ(ComM_GetCurrentComMode(COMM_USER_0, &mode), E_OK);
    EXPECT_EQ(mode, static_cast<ComM_ModeType>(COMM_SILENT_COMMUNICATION));
    EXPECT_EQ(CallCount_Dem_SetEventStatus, 0U);  // まだ PASSED は報告しない
}

TEST_F(CanSM_MainFunction_Test, MainFunction_OK_DoesNotRetryImmediatelyAfterFailedAttempt)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // 1 回目は失敗させ、BUS_OFF のまま据え置かれた状態にする
    ArrangeBusOffPastL1Interval();
    FailFromCallCount_CanIf_SetControllerMode = 1U;
    CanSM_MainFunction();
    ASSERT_EQ(Can_Test_GetControllerState(), CAN_CS_STOPPED);
    ASSERT_EQ(CallCount_CanIf_SetControllerMode, 1U);
    FailFromCallCount_CanIf_SetControllerMode = WRAP_CANIF_FAIL_FROM_CALL_COUNT_DISABLED;  // 以降はパススルー（実体成功）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    // FakeMillis_Value を進めずに（＝L1 周期未経過のまま）
    // 呼ぶ。是正前はここで即座に2回目の試行が発生していた
    // （ファイル冒頭コメント参照）。
    CanSM_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    // 再試行は発生せず（呼び出し回数据え置き）、BUS_OFF のまま
    EXPECT_EQ(CallCount_CanIf_SetControllerMode, 1U);
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STOPPED);
}

// ------------------------------------------------------------
// 正常系: L1 周期を空けて次の試行で CanIf_SetControllerMode(STARTED) が
// 成功すれば FULL_COM へ正しく回復する（上のテストと地続きの回復シーケンス）。
// ------------------------------------------------------------

TEST_F(CanSM_MainFunction_Test, MainFunction_OK_RecoversOnNextAttemptAfterPriorFailure)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // 1 回目は失敗させ、BUS_OFF のまま据え置かれた状態にする
    ArrangeBusOffPastL1Interval();
    FailFromCallCount_CanIf_SetControllerMode = 1U;
    CanSM_MainFunction();
    ASSERT_EQ(Can_Test_GetControllerState(), CAN_CS_STOPPED);
    FailFromCallCount_CanIf_SetControllerMode = WRAP_CANIF_FAIL_FROM_CALL_COUNT_DISABLED;  // 以降はパススルー（実体成功）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    // 是正後は失敗した試行の時刻が基準点として更新されるため、
    // 次の試行にも改めて L1 周期分の経過が必要。
    FakeMillis_Value += static_cast<unsigned long>(CANSM_BUSOFF_RECOVERY_L1_MS) + 1UL;
    CanSM_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    // FULL_COM へ回復し、Dem へ PASSED を報告する
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);
    ComM_ModeType mode = COMM_NO_COMMUNICATION;
    ASSERT_EQ(ComM_GetCurrentComMode(COMM_USER_0, &mode), E_OK);
    EXPECT_EQ(mode, static_cast<ComM_ModeType>(COMM_FULL_COMMUNICATION));
    EXPECT_EQ(CallCount_Dem_SetEventStatus, 1U);
    EXPECT_EQ(LastEventStatus_Dem_SetEventStatus, DEM_EVENT_STATUS_PASSED);
}

TEST_F(CanSM_MainFunction_Test, MainFunction_OK_DoesNothingWhileNotInBusOff)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    const uint32 setModeBefore = CallCount_CanIf_SetControllerMode;   // 通常運転中（BUS_OFF ではない）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    FakeMillis_Value += static_cast<unsigned long>(CANSM_BUSOFF_RECOVERY_L2_MS) + 1UL;
    CanSM_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_CanIf_SetControllerMode, setModeBefore);   // 回復の処理は始めない
}

TEST_F(CanSM_MainFunction_Test, MainFunction_OK_DegradesToL2AndReportsDemAfterRepeatedFailedRecoveries)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ArrangeBusOffPastL1Interval();
    FailFromCallCount_CanIf_SetControllerMode = 1U;   // 回復の試行が、いつも失敗する

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanSM_MainFunction();   // 1 回目（L1）
    FakeMillis_Value += static_cast<unsigned long>(CANSM_BUSOFF_RECOVERY_L1_MS) + 1UL;
    CanSM_MainFunction();   // 2 回目（L1）
    FakeMillis_Value += static_cast<unsigned long>(CANSM_BUSOFF_RECOVERY_L1_MS) + 1UL;
    CanSM_MainFunction();   // 3 回目（L1）
    const uint32 demBeforeL2 = CallCount_Dem_SetEventStatus;
    FakeMillis_Value += static_cast<unsigned long>(CANSM_BUSOFF_RECOVERY_L1_MS) + 1UL;
    CanSM_MainFunction();   // L1 の間隔では、まだ 4 回目の試行をしない（L2 の間隔が必要）
    const uint32 demAtL1Interval = CallCount_Dem_SetEventStatus;
    FakeMillis_Value += static_cast<unsigned long>(CANSM_BUSOFF_RECOVERY_L2_MS) + 1UL;
    CanSM_MainFunction();   // 4 回目（L2 の間隔が経過）

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(demBeforeL2, 0U);
    EXPECT_EQ(demAtL1Interval, 0U);
    EXPECT_EQ(CallCount_Dem_SetEventStatus, 1U);   // L1 を使い切って L2 へ格下げした回に、1 回だけ報告する
    EXPECT_EQ(LastEventStatus_Dem_SetEventStatus, DEM_EVENT_STATUS_FAILED);
}

}  // namespace
