/**
 * \file    Bsw_FiM_test.cpp
 * \brief   FiM(機能抑止マネージャ)の単体テスト（GoogleTest / PlatformIO
 *          `[env:native_chain]`。2026-09、専用環境 `[env:native_fim]` として
 *          新設した後、同月中に `native_chain` の Dem 実体リンク化に合わせて
 *          本 env へ統合した）。
 *
 * \details FiM.c は本プロジェクトでこれまでどの native テスト環境にもリンク
 *          されておらず、テストカバレッジが皆無だった。新設時、
 *          `FiM_Init()`が全FIDを無条件で「許可」初期化していた乖離
 *          （[SWS_Fim_00102]/[SWS_Fim_00104]。以前は起動直後から最初の
 *          `FiM_MainFunction()`呼び出しまでの間、既に確定済みのDTCがあって
 *          も誤って許可扱いになっていた）を是正したのに合わせ、専用環境を
 *          新設した。
 *
 *          Dem.c は本物をリンクし（`test/test_chain/NvM_fake.c`で「常に初回
 *          起動」を決定的に固定）、実際の確定 DTC ステータス伝播をそのまま
 *          検証する。
 */
#include <gtest/gtest.h>

extern "C" {
#include "FiM.h"
#include "Dem.h"
#include "Dem_Cfg.h"
#include "Hal_Millis_fake.h"
#include "Hal_Det_Hw_fake.h"
}

namespace
{

class Bsw_FiM_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Dem_Init() のログはノイズになるため抑制

        Dem_Init(NULL);  // NvM_fake.c により常に「初回起動」で確定的に再現

        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
    }

    static boolean GetPermission(FiM_FunctionIdType fid)
    {
        boolean permission = FALSE;
        const Std_ReturnType ret = FiM_GetFunctionPermission(fid, &permission);
        EXPECT_EQ(ret, E_OK);
        return permission;
    }
};

// ------------------------------------------------------------
// 正常系（回帰確認）
// ------------------------------------------------------------

TEST_F(Bsw_FiM_Test, Init_OK_BothFidsPermittedWhenNoConfirmedDtc)
{
    /* 準備 (Arrange): Dem_Init() 直後は全イベント未確定(初回起動) */

    FiM_Init(&FiM_Config);

    /* 評価 (Assert) */
    EXPECT_EQ(GetPermission(FIM_FID_RUNNING_LED), TRUE);
    EXPECT_EQ(GetPermission(FIM_FID_BUTTON_ACK), TRUE);
}

TEST_F(Bsw_FiM_Test, MainFunction_OK_InhibitsFidWhenDtcConfirmedAfterInit)
{
    FiM_Init(&FiM_Config);
    ASSERT_EQ(GetPermission(FIM_FID_RUNNING_LED), TRUE);

    /* 実行 (Act): 初期化後に Bus-Off が確定 (DEM_DEBOUNCE_LIMIT_CAN_BUSOFF=1
     * のため1回で確定) */
    (void)Dem_SetEventStatus(DEM_EVENT_CAN_BUSOFF, DEM_EVENT_STATUS_FAILED);
    FiM_MainFunction();

    /* 評価 (Assert): RUNNING_LED のみ抑止され、無関係の BUTTON_ACK は影響なし */
    EXPECT_EQ(GetPermission(FIM_FID_RUNNING_LED), FALSE);
    EXPECT_EQ(GetPermission(FIM_FID_BUTTON_ACK), TRUE);
}

// ------------------------------------------------------------
// 2026-09 是正の本題: [SWS_Fim_00102]/[SWS_Fim_00104]
// FiM_Init() 完了時点で既に許可状態が確定していること
// ------------------------------------------------------------

TEST_F(Bsw_FiM_Test, Init_NG_InhibitsAlreadyConfirmedDtcWithoutWaitingForMainFunction)
{
    /* 準備 (Arrange): 実機の起動順序 (EcuM.c: Dem_Init() → FiM_Init()) を
     * 模し、FiM_Init() を呼ぶ「前」に Bus-Off が既に確定している状態を作る
     * (前回起動までに NvM から復元された確定済み DTC に相当)。 */
    (void)Dem_SetEventStatus(DEM_EVENT_CAN_BUSOFF, DEM_EVENT_STATUS_FAILED);

    /* 実行 (Act): 是正前は全 FID を無条件で「許可」初期化しており、
     * FiM_MainFunction() が最低1回実行されるまでこの乖離が顕在化しな
     * かった。FiM_MainFunction() は一度も呼ばない。 */
    FiM_Init(&FiM_Config);

    /* 評価 (Assert): FiM_Init() 完了の時点で、既に抑止が反映されている
     * べき（[SWS_Fim_00104]: FiM_GetFunctionPermission は完全初期化前は
     * 使用してはならない＝完全初期化の時点で正しい値が確定している）。 */
    EXPECT_EQ(GetPermission(FIM_FID_RUNNING_LED), FALSE);
    EXPECT_EQ(GetPermission(FIM_FID_BUTTON_ACK), TRUE);
}

TEST_F(Bsw_FiM_Test, Init_OK_SetFunctionAvailableStillOverridesAfterInitEvaluation)
{
    /* 準備 (Arrange): 確定済み DTC は無い状態で初期化 */
    FiM_Init(&FiM_Config);
    ASSERT_EQ(GetPermission(FIM_FID_BUTTON_ACK), TRUE);

    /* 実行 (Act): [SWS_Fim_00106] 外部からの強制利用不可 */
    const Std_ReturnType ret = FiM_SetFunctionAvailable(FIM_FID_BUTTON_ACK, FALSE);

    /* 評価 (Assert) */
    ASSERT_EQ(ret, E_OK);
    EXPECT_EQ(GetPermission(FIM_FID_BUTTON_ACK), FALSE);
}

}  // namespace
