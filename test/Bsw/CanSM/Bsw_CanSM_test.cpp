/**
 * \file    Bsw_CanSM_test.cpp
 * \brief   CanSM.c（src/Bsw/CanSM/CanSM.c）の単体テスト（GoogleTest / PlatformIO
 *          `[env:native_chain]`）。
 *
 * \details 2026-09、モジュール単位のテストファイルを1モジュール1ファイルへ
 *          集約する方針のもと、`Bsw_CanSM_BusOffRecovery_test.cpp` と旧
 *          `Bsw_CanSM_test.cpp`（NG系のみ）の2ファイルを本ファイルへ統合した。
 *          各セクションの経緯は元ファイルのコメントをそのまま引き継ぐ。
 */
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

namespace
{

// ============================================================================
// CanSM_MainFunction() の Bus-Off 回復リトライ（旧
// Bsw_CanSM_BusOffRecovery_test.cpp）
// ============================================================================

/**
 * \brief   コールチェーン方式 + `-Wl,--wrap` フォールトインジェクション
 *          （GoogleTest / PlatformIO `[env:native_chain]`。2026-09、
 *          試作環境 `[env:native_chain_wrap]` として2ラウンドの実績を
 *          積んだ後、本 env へ統合した）。
 *
 * \details 対象は `CanSM_MainFunction()` の Bus-Off 回復リトライ（README
 *          「CAN コントローラの Bus-Off 検出/回復」節、CanSM.c 内
 *          `CanIf_SetControllerMode(CAN_CS_STARTED)` 失敗時の分岐、
 *          `CanSM.c` 726〜736 行目）。この分岐は「本プロジェクトの簡略化された
 *          CanIf/Can HW 抽象化では到達しないはず」という防御的コードとして
 *          2026-08 のレビュー以降コメントだけが残り、一度もテストで踏まれた
 *          ことがなかった（`test/` を検索しても Bus-Off 回復の
 *          正常系（`Bsw_NmStack_SleepCoordination_test.cpp` の
 *          `ReRequestFullComDuringBusOff_OK_RestoresFullComAfterRecovery` 等）
 *          しか存在しない）。
 *
 *          この分岐を発火させるには「CanIf_SetControllerMode() だけが失敗し、
 *          CanSM/CanIf/Can の残りは正常」という状態が要るが、本プロジェクトの
 *          Can.c は CanState 遷移ガードが健全なため、通常の入力操作だけでは
 *          この状態を作れない（CanIf_ControllerBusOff() 呼び出し元コメント
 *          参照）。かといって CanIf.c ごとフェイクに差し替えると、
 *          この分岐が本当に守っている「CanSM から見た CanIf の失敗」という
 *          コールチェーンの実体（CanIf→Can の実ロジック）を検証できなくなる。
 *
 *          `-Wl,--wrap=CanIf_SetControllerMode`（platformio.ini の
 *          `[env:native_chain]` 参照）を使うと、CanIf.c/Can.c の実体は
 *          そのまま保ちつつ、CanSM.c から見た `CanIf_SetControllerMode()` の
 *          戻り値だけをピンポイントで差し替えられる
 *          （Wrap_CanIf.h 参照）。
 *
 * \note    試作中に発見した副次的なバグ（2026-09-18、別ラウンドで是正済み）:
 *          `CanSM_BusOffTimerMs` は Bus-Off 検出時（`CanSM_ControllerBusOff()`）
 *          だけでなく、各リトライ試行そのものの時刻でも更新するよう修正した
 *          （`CanSM_MainFunction()` 本体のコメント参照）。以前は検出時の1回
 *          しか更新されず、`CanIf_SetControllerMode(CAN_CS_STARTED)` が
 *          失敗して BUS_OFF に据え置かれた場合、以後は `CanSM_MainFunction()`
 *          を呼ぶたびに毎回リトライ条件を満たし続けてしまい（L1/L2 の
 *          バックオフが実質効かない）、`FakeMillis_Value` を追加で進めなくても
 *          即リトライされてしまう不具合だった。下記
 *          `MainFunction_OK_RecoversOnNextAttemptAfterPriorFailure` は
 *          是正後の正しい挙動（2回目の試行にも L1 周期の待機が必要）を
 *          検証するよう更新済み。
 *
 *          このセクションは「CanIf_SetControllerMode() だけをピンポイントで
 *          故障させ、それ以外は実チェーンのまま」という、まさに wrap を
 *          活用したコールチェーン方式の実例であり、モジュール単体の分離
 *          テストへ縮退させる余地はない（2026-09、モジュール単位のテスト
 *          集約を検討した際に再確認・現状維持を確認済み）。
 */

const CanIf_ConfigType kBusOffRecoveryCanIfConfig = {
    /* TxPduConfig */ NULL,
    /* TxPduCount */  0U,
    /* RxPduConfig */ NULL,
    /* RxPduCount */  0U
};

class Bsw_CanSM_BusOffRecovery_Test : public ::testing::Test
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

        // FULL_COM を Arrange する（Bsw_NmStack_SleepCoordination_test.cpp の
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

// ------------------------------------------------------------
// 異常系: CanIf_SetControllerMode(STARTED) が失敗する回、コントローラは
// BUS_OFF のまま据え置かれ、次周期の再試行に委ねられる（CanSM.c 726〜736
// 行目、これまで未到達だった防御分岐）。
// ------------------------------------------------------------
TEST_F(Bsw_CanSM_BusOffRecovery_Test, MainFunction_NG_RecoveryAttemptFails_StaysInBusOffForNextRetry)
{
    /* 準備 (Arrange) */
    ArrangeBusOffPastL1Interval();
    FailFromCallCount_CanIf_SetControllerMode = 1U;

    /* 実行 (Act) */
    CanSM_MainFunction();

    /* 評価 (Assert): 回復を試みたが失敗 → BUS_OFF のまま */
    EXPECT_EQ(CallCount_CanIf_SetControllerMode, 1U);
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STOPPED);
    ComM_ModeType mode = COMM_FULL_COMMUNICATION;
    ASSERT_EQ(ComM_GetCurrentComMode(COMM_USER_0, &mode), E_OK);
    EXPECT_EQ(mode, static_cast<ComM_ModeType>(COMM_SILENT_COMMUNICATION));
    EXPECT_EQ(CallCount_Dem_SetEventStatus, 0U);  // まだ PASSED は報告しない
}

// ------------------------------------------------------------
// 2026-09 是正の本題: 失敗した試行の直後（L1 周期を空けずに）呼んでも
// 再試行しない（バックオフ周期を正しく守る）。
// ------------------------------------------------------------
TEST_F(Bsw_CanSM_BusOffRecovery_Test, MainFunction_OK_DoesNotRetryImmediatelyAfterFailedAttempt)
{
    /* 準備 (Arrange): 1 回目は失敗させ、BUS_OFF のまま据え置かれた状態にする */
    ArrangeBusOffPastL1Interval();
    FailFromCallCount_CanIf_SetControllerMode = 1U;
    CanSM_MainFunction();
    ASSERT_EQ(Can_Test_GetControllerState(), CAN_CS_STOPPED);
    ASSERT_EQ(CallCount_CanIf_SetControllerMode, 1U);
    FailFromCallCount_CanIf_SetControllerMode = WRAP_CANIF_FAIL_FROM_CALL_COUNT_DISABLED;  // 以降はパススルー（実体成功）

    /* 実行 (Act): FakeMillis_Value を進めずに（＝L1 周期未経過のまま）
     * 呼ぶ。是正前はここで即座に2回目の試行が発生していた
     * （ファイル冒頭コメント参照）。 */
    CanSM_MainFunction();

    /* 評価 (Assert): 再試行は発生せず（呼び出し回数据え置き）、BUS_OFF のまま */
    EXPECT_EQ(CallCount_CanIf_SetControllerMode, 1U);
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STOPPED);
}

// ------------------------------------------------------------
// 正常系: L1 周期を空けて次の試行で CanIf_SetControllerMode(STARTED) が
// 成功すれば FULL_COM へ正しく回復する（上のテストと地続きの回復シーケンス）。
// ------------------------------------------------------------
TEST_F(Bsw_CanSM_BusOffRecovery_Test, MainFunction_OK_RecoversOnNextAttemptAfterPriorFailure)
{
    /* 準備 (Arrange): 1 回目は失敗させ、BUS_OFF のまま据え置かれた状態にする */
    ArrangeBusOffPastL1Interval();
    FailFromCallCount_CanIf_SetControllerMode = 1U;
    CanSM_MainFunction();
    ASSERT_EQ(Can_Test_GetControllerState(), CAN_CS_STOPPED);
    FailFromCallCount_CanIf_SetControllerMode = WRAP_CANIF_FAIL_FROM_CALL_COUNT_DISABLED;  // 以降はパススルー（実体成功）

    /* 実行 (Act): 是正後は失敗した試行の時刻が基準点として更新されるため、
     * 次の試行にも改めて L1 周期分の経過が必要。 */
    FakeMillis_Value += static_cast<unsigned long>(CANSM_BUSOFF_RECOVERY_L1_MS) + 1UL;
    CanSM_MainFunction();

    /* 評価 (Assert): FULL_COM へ回復し、Dem へ PASSED を報告する */
    EXPECT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);
    ComM_ModeType mode = COMM_NO_COMMUNICATION;
    ASSERT_EQ(ComM_GetCurrentComMode(COMM_USER_0, &mode), E_OK);
    EXPECT_EQ(mode, static_cast<ComM_ModeType>(COMM_FULL_COMMUNICATION));
    EXPECT_EQ(CallCount_Dem_SetEventStatus, 1U);
    EXPECT_EQ(LastEventStatus_Dem_SetEventStatus, DEM_EVENT_STATUS_PASSED);
}

// ============================================================================
// その他公開APIの NG系（旧 Bsw_CanSM_test.cpp）
// ============================================================================

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

class Bsw_CanSM_Test : public ::testing::Test
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

// ------------------------------------------------------------
// CanSM_DeInit()
// ------------------------------------------------------------

TEST_F(Bsw_CanSM_Test, CanSM_DeInit_NG_Uninit)
{
    CanSM_DeInit();  // 1回目: SetUp() の Init を正常に解除する
    FakeDetHw_Reset();

    CanSM_DeInit();  // 2回目: 既に未初期化のため NG

    EXPECT_EQ(FakeDetHw_LastErrorId, CANSM_E_UNINIT);
}

// ------------------------------------------------------------
// CanSM_RequestComMode()
// ------------------------------------------------------------

TEST_F(Bsw_CanSM_Test, CanSM_RequestComMode_NG_Uninit)
{
    CanSM_DeInit();

    Std_ReturnType ret = CanSM_RequestComMode(0U, COMM_FULL_COMMUNICATION);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANSM_E_UNINIT);
}

TEST_F(Bsw_CanSM_Test, CanSM_RequestComMode_NG_InvalidNetworkHandle)
{
    Std_ReturnType ret = CanSM_RequestComMode(CANSM_CHANNEL_COUNT, COMM_FULL_COMMUNICATION);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANSM_E_INVALID_NETWORK_HANDLE);
}

// ------------------------------------------------------------
// CanSM_GetCurrentComMode()
// ------------------------------------------------------------

TEST_F(Bsw_CanSM_Test, CanSM_GetCurrentComMode_NG_Uninit)
{
    CanSM_DeInit();
    ComM_ModeType mode;

    Std_ReturnType ret = CanSM_GetCurrentComMode(0U, &mode);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANSM_E_UNINIT);
}

TEST_F(Bsw_CanSM_Test, CanSM_GetCurrentComMode_NG_InvalidNetworkHandle)
{
    ComM_ModeType mode;

    Std_ReturnType ret = CanSM_GetCurrentComMode(CANSM_CHANNEL_COUNT, &mode);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANSM_E_INVALID_NETWORK_HANDLE);
}

TEST_F(Bsw_CanSM_Test, CanSM_GetCurrentComMode_NG_NullPointer)
{
    Std_ReturnType ret = CanSM_GetCurrentComMode(0U, NULL);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANSM_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// CanSM_GetVersionInfo()
// ------------------------------------------------------------

TEST_F(Bsw_CanSM_Test, CanSM_GetVersionInfo_NG_NullPointer)
{
    CanSM_GetVersionInfo(NULL);

    EXPECT_EQ(FakeDetHw_LastErrorId, CANSM_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// CanSM_ControllerBusOff()
// ------------------------------------------------------------

TEST_F(Bsw_CanSM_Test, CanSM_ControllerBusOff_NG_Uninit)
{
    CanSM_DeInit();

    CanSM_ControllerBusOff(0U);

    EXPECT_EQ(FakeDetHw_LastErrorId, CANSM_E_UNINIT);
}

TEST_F(Bsw_CanSM_Test, CanSM_ControllerBusOff_NG_InvalidController)
{
    CanSM_ControllerBusOff(1U);

    EXPECT_EQ(FakeDetHw_LastErrorId, CANSM_E_PARAM_CONTROLLER);
}

// ------------------------------------------------------------
// CanSM_ControllerModeIndication()
// ------------------------------------------------------------

TEST_F(Bsw_CanSM_Test, CanSM_ControllerModeIndication_NG_Uninit)
{
    CanSM_DeInit();

    CanSM_ControllerModeIndication(0U, CAN_CS_STARTED);

    EXPECT_EQ(FakeDetHw_LastErrorId, CANSM_E_UNINIT);
}

TEST_F(Bsw_CanSM_Test, CanSM_ControllerModeIndication_NG_InvalidController)
{
    CanSM_ControllerModeIndication(1U, CAN_CS_STARTED);

    EXPECT_EQ(FakeDetHw_LastErrorId, CANSM_E_PARAM_CONTROLLER);
}

// ------------------------------------------------------------
// CanSM_RxIndication()
// ------------------------------------------------------------

TEST_F(Bsw_CanSM_Test, CanSM_RxIndication_NG_Uninit)
{
    CanSM_DeInit();

    CanSM_RxIndication(0U);

    EXPECT_EQ(FakeDetHw_LastErrorId, CANSM_E_UNINIT);
}

// ------------------------------------------------------------
// CanSM_MainFunction()
// ------------------------------------------------------------

TEST_F(Bsw_CanSM_Test, CanSM_MainFunction_NG_Uninit)
{
    CanSM_DeInit();

    CanSM_MainFunction();

    EXPECT_EQ(FakeDetHw_LastErrorId, CANSM_E_UNINIT);
}

}  // namespace
