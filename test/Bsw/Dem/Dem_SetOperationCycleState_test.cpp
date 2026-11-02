/**
 * \file    Dem_SetOperationCycleState_test.cpp
 * \brief   Dem_SetOperationCycleState() の単体テスト
 * \details START で操作サイクルを（再）開始し（直前のサイクルの最終結果で、経年回復と PendingDTC の自動クリアを評価し、
 *          TF・TFTOC のクリア、TNCTOC のセット、デバウンスの初期化を行う）、END で終了する（終了している間、
 *          Dem_SetEventStatus() の報告は無視される）。確定が 1 回で済む DemConf_DemEventParameter_BUTTON_STUCK を使う。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Dem.h"
#include "Dem_Cfg.h"
#include "NvM.h"
#include "NvM_Cfg.h"
#include "MemIf.h"
#include "Fake_Det_Hw.h"
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

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* Dem_SetOperationCycleState() の単体テスト用フィクスチャ。
 * SetUp(): wrap と DET を初期化し、MemIf・NvM を初期化してから、Dem_Init(NULL) を呼ぶ（操作サイクルは、開始済みになる）。
 *          Status() は、イベントの UDS ステータスの取得。StartCycle()/EndCycle() は、準備用の、電源サイクルの開始 / 終了。
 * TearDown(): Dem と NvM を未初期化へ戻す。 */
class Dem_SetOperationCycleState_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        WrapDem_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        MemIf_Init();
        NvM_Init(NULL);
        Dem_Init(NULL);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Dem_Test_ResetInitState();
        NvM_Test_ResetInitState();
    }

    static uint8 Status(Dem_EventIdType eventId)
    {
        Dem_UdsStatusByteType status = 0U;
        (void)Dem_GetEventUdsStatus(eventId, &status);
        return status;
    }

    /* 電源サイクルを、開始 / 終了する（準備のための補助。テスト対象の呼び出しは、直接書く） */
    static Std_ReturnType StartCycle()
    {
        return Dem_SetOperationCycleState(DemConf_DemOperationCycle_POWER, DEM_CYCLE_STATE_START);
    }
    static Std_ReturnType EndCycle()
    {
        return Dem_SetOperationCycleState(DemConf_DemOperationCycle_POWER, DEM_CYCLE_STATE_END);
    }

    static const Dem_EventIdType kEvent = DemConf_DemEventParameter_BUTTON_STUCK;
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(Dem_SetOperationCycleState_Test, Dem_SetOperationCycleState_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dem_Test_ResetInitState();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_SetOperationCycleState(DemConf_DemOperationCycle_POWER, DEM_CYCLE_STATE_START);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_UNINIT);
}

// 構成にない操作サイクルは、DEM_E_WRONG_CONFIGURATION で拒否する（[SWS_Dem_01223]）
TEST_F(Dem_SetOperationCycleState_Test, Dem_SetOperationCycleState_NG_UnknownCycle)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_SetOperationCycleState(DEM_OPERATION_CYCLE_COUNT, DEM_CYCLE_STATE_END);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_WRONG_CONFIGURATION);
}

TEST_F(Dem_SetOperationCycleState_Test, Dem_SetOperationCycleState_NG_InvalidCycleState)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret =
        Dem_SetOperationCycleState(DemConf_DemOperationCycle_POWER, static_cast<Dem_OperationCycleStateType>(2));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DEM_E_PARAM_DATA);
}

// END: 終了している間の報告は、無視する。UDS ステータスもデバウンスも、変わらない（[SWS_Dem_00482]）
TEST_F(Dem_SetOperationCycleState_Test, Dem_SetOperationCycleState_OK_EndIgnoresEventReports)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    const uint8 before = Status(kEvent);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_SetOperationCycleState(DemConf_DemOperationCycle_POWER, DEM_CYCLE_STATE_END);
    const Std_ReturnType report = Dem_SetEventStatus(kEvent, DEM_EVENT_STATUS_FAILED);
    sint8 fdc = 99;
    ASSERT_EQ(Dem_GetFaultDetectionCounter(kEvent, &fdc), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(report, E_OK);                  // 受理して、無視する
    EXPECT_EQ(Status(kEvent), before);        // ステータスは、変わらない
    EXPECT_EQ(fdc, 0);                        // デバウンスも、進まない
}

// END のあと START すると、報告を再び受け付ける（[SWS_Dem_00481]）
TEST_F(Dem_SetOperationCycleState_Test, Dem_SetOperationCycleState_OK_StartAcceptsReportsAgain)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(EndCycle(), E_OK);
    ASSERT_EQ(Dem_SetEventStatus(kEvent, DEM_EVENT_STATUS_FAILED), E_OK);   // 無視される

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ASSERT_EQ(StartCycle(), E_OK);
    ASSERT_EQ(Dem_SetEventStatus(kEvent, DEM_EVENT_STATUS_FAILED), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_NE(Status(kEvent) & DEM_STATUS_TEST_FAILED, 0U);
}

// 既に終了済みの END は、何もしない（[SWS_Dem_00484]）。状態は、終了のまま
TEST_F(Dem_SetOperationCycleState_Test, Dem_SetOperationCycleState_OK_EndWhenAlreadyEndedDoesNothing)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(EndCycle(), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_SetOperationCycleState(DemConf_DemOperationCycle_POWER, DEM_CYCLE_STATE_END);
    Dem_OperationCycleStateType state = DEM_CYCLE_STATE_START;
    ASSERT_EQ(Dem_GetOperationCycleState(DemConf_DemOperationCycle_POWER, &state), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(state, DEM_CYCLE_STATE_END);
}

// START（開始済みからの再開始も同じ、[SWS_Dem_00483]）: TF をクリアし、TNCTOC を立て、デバウンスを 0 へ戻す。
// CONFIRMED・PENDING は、保つ（TFTOC のクリアは、availability mask で見えないので、PendingDTC・経年回復のテストで確かめる）
TEST_F(Dem_SetOperationCycleState_Test, Dem_SetOperationCycleState_OK_RestartResetsCycleBitsAndDebounce)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SetEventStatus(kEvent, DEM_EVENT_STATUS_FAILED), E_OK);   // 確定（TF・TFTOC・PDTC・CDTC）
    const uint8 failed = Status(kEvent);
    ASSERT_NE(failed & DEM_STATUS_TEST_FAILED, 0U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_SetOperationCycleState(DemConf_DemOperationCycle_POWER, DEM_CYCLE_STATE_START);
    const uint8 after = Status(kEvent);
    Dem_MonitorStatusType monitor = 0U;
    ASSERT_EQ(Dem_GetMonitorStatus(kEvent, &monitor), E_OK);
    sint8 fdc = 99;
    ASSERT_EQ(Dem_GetFaultDetectionCounter(kEvent, &fdc), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(after & DEM_STATUS_TEST_FAILED, 0U);
    EXPECT_NE(monitor & DEM_MONITOR_STATUS_TNCTOC, 0U);   // 今サイクルは、まだテストされていない
    EXPECT_NE(after & DEM_STATUS_CONFIRMED, 0U);   // 確定は、保たれる
    EXPECT_NE(after & DEM_STATUS_PENDING, 0U);     // このサイクルは、失敗したので、PendingDTC も保たれる
    EXPECT_EQ(fdc, 0);
}

// START: 直前のサイクルが、クリーン（失敗なし、テスト済み）なら、PendingDTC を自動でクリアする（[SWS_Dem_00390]）。
// 開始のたびに評価するので、Dem_Init() のあとに、再開始でも同じに動く
TEST_F(Dem_SetOperationCycleState_Test, Dem_SetOperationCycleState_OK_StartClearsPendingAfterCleanCycle)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SetEventStatus(kEvent, DEM_EVENT_STATUS_FAILED), E_OK);                           // サイクル 1: 失敗
    ASSERT_EQ(StartCycle(), E_OK);   // サイクル 2 へ
    ASSERT_NE(Status(kEvent) & DEM_STATUS_PENDING, 0U);
    ASSERT_EQ(Dem_SetEventStatus(kEvent, DEM_EVENT_STATUS_PASSED), E_OK);                           // サイクル 2: テスト済み・失敗なし

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ASSERT_EQ(StartCycle(), E_OK);   // サイクル 3 へ

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Status(kEvent) & DEM_STATUS_PENDING, 0U);
}

// END: そのサイクルが、クリーン（失敗なし、テスト済み）なら、次の START を待たずに、PendingDTC を自動でクリアする
// （[SWS_Dem_00390]: 「END|RESTART」でクリア）
TEST_F(Dem_SetOperationCycleState_Test, Dem_SetOperationCycleState_OK_EndClearsPendingAfterCleanCycle)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SetEventStatus(kEvent, DEM_EVENT_STATUS_FAILED), E_OK);   // サイクル 1: 失敗
    ASSERT_EQ(StartCycle(), E_OK);                                          // サイクル 2 へ
    ASSERT_NE(Status(kEvent) & DEM_STATUS_PENDING, 0U);
    ASSERT_EQ(Dem_SetEventStatus(kEvent, DEM_EVENT_STATUS_PASSED), E_OK);   // サイクル 2: テスト済み・失敗なし

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const Std_ReturnType ret = Dem_SetOperationCycleState(DemConf_DemOperationCycle_POWER, DEM_CYCLE_STATE_END);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(Status(kEvent) & DEM_STATUS_PENDING, 0U);   // START していないのに、クリアされている
}

// END: そのサイクルで失敗したイベントの PendingDTC は、クリアしない
TEST_F(Dem_SetOperationCycleState_Test, Dem_SetOperationCycleState_OK_EndKeepsPendingWhenCycleFailed)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SetEventStatus(kEvent, DEM_EVENT_STATUS_FAILED), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ASSERT_EQ(EndCycle(), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_NE(Status(kEvent) & DEM_STATUS_PENDING, 0U);
}

// END: そのサイクルで、一度もテストされなかったイベントの PendingDTC は、クリアしない
TEST_F(Dem_SetOperationCycleState_Test, Dem_SetOperationCycleState_OK_EndKeepsPendingWhenNotTested)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SetEventStatus(kEvent, DEM_EVENT_STATUS_FAILED), E_OK);   // サイクル 1: 失敗
    ASSERT_EQ(StartCycle(), E_OK);                                          // サイクル 2 へ（まだ、テストしていない）
    ASSERT_NE(Status(kEvent) & DEM_STATUS_PENDING, 0U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ASSERT_EQ(EndCycle(), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_NE(Status(kEvent) & DEM_STATUS_PENDING, 0U);
}

// END: PendingDTC をクリアしたとき、変更したステータスを NvM へ書き込む
TEST_F(Dem_SetOperationCycleState_Test, Dem_SetOperationCycleState_OK_EndPersistsClearedPendingToNvM)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SetEventStatus(kEvent, DEM_EVENT_STATUS_FAILED), E_OK);
    ASSERT_EQ(StartCycle(), E_OK);
    ASSERT_EQ(Dem_SetEventStatus(kEvent, DEM_EVENT_STATUS_PASSED), E_OK);
    uint8 sentinel[DEM_EVENT_COUNT];
    for (uint8 i = 0U; i < DEM_EVENT_COUNT; i++)
    {
        sentinel[i] = 0xA5U;
    }
    ASSERT_EQ(NvM_WriteBlock(NVM_BLOCK_ID_DEM_STATUS, sentinel), E_OK);   // 書き込まれたかを見分ける目印

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ASSERT_EQ(EndCycle(), E_OK);
    uint8 saved[DEM_EVENT_COUNT] = { 0U };
    (void)NvM_ReadBlock(NVM_BLOCK_ID_DEM_STATUS, saved);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(saved[kEvent] & DEM_STATUS_PENDING, 0U);
}

// END のあとに電源を切って、起動し直しても（Dem_Init）、同じサイクルを、もう一度評価しない。経年回復カウンタが、二重に進まない
// （閾値に達する前に、確定が解除されない）
TEST_F(Dem_SetOperationCycleState_Test, Dem_SetOperationCycleState_OK_EndThenRebootDoesNotDoubleCountAging)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SetEventStatus(kEvent, DEM_EVENT_STATUS_FAILED), E_OK);   // CONFIRMED
    ASSERT_EQ(EndCycle(), E_OK);     // 失敗したサイクルの終わり（再故障なので、カウンタは 0 のまま）
    Dem_Test_ResetInitState();
    Dem_Init(NULL);                  // 電源を入れ直す（NvM は、そのまま）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    for (uint8 cycle = 1U; cycle < DEM_AGING_THRESHOLD_BUTTON_STUCK; cycle++)
    {
        ASSERT_EQ(Dem_SetEventStatus(kEvent, DEM_EVENT_STATUS_PASSED), E_OK);   // クリーンなサイクル
        ASSERT_EQ(EndCycle(), E_OK);
        Dem_Test_ResetInitState();
        Dem_Init(NULL);
        ASSERT_NE(Status(kEvent) & DEM_STATUS_CONFIRMED, 0U);                   // 閾値の前は、まだ確定のまま
    }
    ASSERT_EQ(Dem_SetEventStatus(kEvent, DEM_EVENT_STATUS_PASSED), E_OK);       // 閾値に達する、最後のクリーンなサイクル
    ASSERT_EQ(EndCycle(), E_OK);
    Dem_Test_ResetInitState();
    Dem_Init(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Status(kEvent) & DEM_STATUS_CONFIRMED, 0U);
}

// END: 評価済みの印（内部用の bit7）は、availability mask で、外に見えない。次のサイクルの開始で、落ちる
TEST_F(Dem_SetOperationCycleState_Test, Dem_SetOperationCycleState_OK_EvaluatedMarkIsHiddenAndClearedByStart)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SetEventStatus(kEvent, DEM_EVENT_STATUS_FAILED), E_OK);
    ASSERT_EQ(EndCycle(), E_OK);
    uint8 saved[DEM_EVENT_COUNT] = { 0U };
    (void)NvM_ReadBlock(NVM_BLOCK_ID_DEM_STATUS, saved);
    ASSERT_NE(saved[kEvent] & DEM_STATUS_CYCLE_EVALUATED, 0U);   // END で、NvM に保存された

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    const uint8 hidden = Status(kEvent);
    ASSERT_EQ(StartCycle(), E_OK);
    (void)NvM_ReadBlock(NVM_BLOCK_ID_DEM_STATUS, saved);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(hidden & DEM_STATUS_CYCLE_EVALUATED, 0U);
    EXPECT_EQ(saved[kEvent] & DEM_STATUS_CYCLE_EVALUATED, 0U);
}

// END: そのサイクルが、クリーンなら、経年回復カウンタを 1 つ進める。閾値に達した END で、CONFIRMED を解除する
// （START を待たない）。END と START を組にして繰り返しても、カウンタは二重に進まない（閾値に達する前に、解除されない）
TEST_F(Dem_SetOperationCycleState_Test, Dem_SetOperationCycleState_OK_EndAgesConfirmedEventWithoutDoubleCounting)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SetEventStatus(kEvent, DEM_EVENT_STATUS_FAILED), E_OK);   // CONFIRMED
    ASSERT_NE(Status(kEvent) & DEM_STATUS_CONFIRMED, 0U);
    ASSERT_EQ(EndCycle(), E_OK);     // 失敗したサイクルの終わり（再故障なので、カウンタは 0 のまま）
    ASSERT_EQ(StartCycle(), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    for (uint8 cycle = 1U; cycle < DEM_AGING_THRESHOLD_BUTTON_STUCK; cycle++)
    {
        ASSERT_EQ(Dem_SetEventStatus(kEvent, DEM_EVENT_STATUS_PASSED), E_OK);   // クリーンなサイクル
        ASSERT_EQ(EndCycle(), E_OK);
        ASSERT_NE(Status(kEvent) & DEM_STATUS_CONFIRMED, 0U);                   // 閾値の前は、まだ確定のまま
        ASSERT_EQ(StartCycle(), E_OK);
        ASSERT_NE(Status(kEvent) & DEM_STATUS_CONFIRMED, 0U);                   // START で、二重に進まない
    }
    ASSERT_EQ(Dem_SetEventStatus(kEvent, DEM_EVENT_STATUS_PASSED), E_OK);       // 閾値に達する、最後のクリーンなサイクル
    ASSERT_EQ(EndCycle(), E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Status(kEvent) & DEM_STATUS_CONFIRMED, 0U);   // START していないのに、回復している
}

// END: 経年回復を進めたとき、変更したカウンタを NvM へ書き込む
TEST_F(Dem_SetOperationCycleState_Test, Dem_SetOperationCycleState_OK_EndPersistsAgingCounterToNvM)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SetEventStatus(kEvent, DEM_EVENT_STATUS_FAILED), E_OK);
    ASSERT_EQ(EndCycle(), E_OK);
    ASSERT_EQ(StartCycle(), E_OK);
    ASSERT_EQ(Dem_SetEventStatus(kEvent, DEM_EVENT_STATUS_PASSED), E_OK);   // クリーンなサイクル
    uint8 sentinel[DEM_EVENT_COUNT];
    for (uint8 i = 0U; i < DEM_EVENT_COUNT; i++)
    {
        sentinel[i] = 0xA5U;
    }
    ASSERT_EQ(NvM_WriteBlock(NVM_BLOCK_ID_DEM_AGING, sentinel), E_OK);   // 書き込まれたかを見分ける目印

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ASSERT_EQ(EndCycle(), E_OK);   // カウンタが 0 → 1
    uint8 saved[DEM_EVENT_COUNT] = { 0U };
    (void)NvM_ReadBlock(NVM_BLOCK_ID_DEM_AGING, saved);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(saved[kEvent], 1U);
}

// START: 直前のサイクルが、クリーンなら、経年回復カウンタを 1 つ進める。閾値に達したら、CONFIRMED を解除する
TEST_F(Dem_SetOperationCycleState_Test, Dem_SetOperationCycleState_OK_StartAgesConfirmedEventAfterCleanCycles)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SetEventStatus(kEvent, DEM_EVENT_STATUS_FAILED), E_OK);   // CONFIRMED
    ASSERT_NE(Status(kEvent) & DEM_STATUS_CONFIRMED, 0U);
    ASSERT_EQ(StartCycle(), E_OK);   // 失敗したサイクルの終わり

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    for (uint8 cycle = 0U; cycle < DEM_AGING_THRESHOLD_BUTTON_STUCK; cycle++)
    {
        ASSERT_EQ(Dem_SetEventStatus(kEvent, DEM_EVENT_STATUS_PASSED), E_OK);   // クリーンなサイクル
        ASSERT_NE(Status(kEvent) & DEM_STATUS_CONFIRMED, 0U);                   // 閾値の前は、まだ確定のまま
        ASSERT_EQ(StartCycle(), E_OK);
    }

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Status(kEvent) & DEM_STATUS_CONFIRMED, 0U);   // 閾値に達して、回復した
}

// START: 変更したステータスと経年回復カウンタを、NvM へ書き込む
TEST_F(Dem_SetOperationCycleState_Test, Dem_SetOperationCycleState_OK_StartPersistsStatusToNvM)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(Dem_SetEventStatus(kEvent, DEM_EVENT_STATUS_FAILED), E_OK);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ASSERT_EQ(StartCycle(), E_OK);
    uint8 saved[DEM_EVENT_COUNT] = { 0U };
    (void)NvM_ReadBlock(NVM_BLOCK_ID_DEM_STATUS, saved);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(saved[kEvent] & DEM_STATUS_TEST_FAILED, 0U);
    EXPECT_NE(saved[kEvent] & DEM_STATUS_NOT_COMPLETED_THIS_CYCLE, 0U);
}

// START: 何も変わらない再開始では、NvM へ書き込まない（EEPROM を消耗させない）
TEST_F(Dem_SetOperationCycleState_Test, Dem_SetOperationCycleState_OK_RestartWithoutChangeSkipsNvMWrite)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ASSERT_EQ(StartCycle(), E_OK);   // 変化なし（起動直後と同じ）
    uint8 sentinel[DEM_EVENT_COUNT];
    for (uint8 i = 0U; i < DEM_EVENT_COUNT; i++)
    {
        sentinel[i] = 0xA5U;
    }
    ASSERT_EQ(NvM_WriteBlock(NVM_BLOCK_ID_DEM_STATUS, sentinel), E_OK);   // 書き込まれたかを見分ける目印

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ASSERT_EQ(StartCycle(), E_OK);
    uint8 saved[DEM_EVENT_COUNT] = { 0U };
    (void)NvM_ReadBlock(NVM_BLOCK_ID_DEM_STATUS, saved);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(saved[kEvent], 0xA5U);   // 目印のまま（Dem は書き込んでいない）
}

}  // namespace
