/**
 * \file    Bsw_E2EXf_SMCheckFailure_test.cpp
 * \brief   コールチェーン方式 + `-Wl,--wrap` フォールトインジェクションの
 *          第2適用例（GoogleTest / PlatformIO `[env:native_chain]`。2026-09、
 *          試作環境 `[env:native_chain_wrap]` から本 env へ統合した）。
 *
 * \details 対象は `E2EXf_ReportSMVerdict()`（E2EXf.c）内の、`E2E_SMCheck()`
 *          が `E2E_E_OK` 以外を返した場合の分岐（E2EXf.c 47〜59 行目）。
 *          `E2EXf_PBCfg_Init()` が全インスタンスに対し `E2E_SMCheckInit()` を
 *          呼んでから使うため「到達しないはず」の防御的コードとしてコメント
 *          だけが残り、一度もテストで踏まれたことがなかった
 *          （`Wrap_CanIf.h`/`Bsw_CanSM_BusOffRecovery_test.cpp`
 *          と同じ状況、CanSM.c 726〜736 行目の初カバーに続く2件目）。
 *
 *          `-Wl,--wrap=E2E_SMCheck`（platformio.ini の `[env:native_chain]`
 *          参照）を使うと、E2EXf.c/E2E.c/E2E_P05.c の実体はそのまま保ちつつ、
 *          E2EXf.c から見た `E2E_SMCheck()` の戻り値だけをピンポイントで
 *          差し替えられる（Wrap_E2E.h 参照）。
 *
 *          Com/PduR/CanIf 等は経由せず、`E2EXf_InverseTransformP05()` を
 *          直接呼ぶところから始める（`test/test_chain/Bsw_RxE2EChain_test.cpp`
 *          と同じ粒度・同じ本番設定 `E2EXf_EngineInfoRxCfg` を使う）。
 */
#include <gtest/gtest.h>

extern "C" {
#include "E2EXf.h"
#include "E2EXf_PBCfg.h"
#include "E2E_P05.h"
#include "Hal_Det_Hw_fake.h"
#include "Bsw_Dem_fake.h"
#include "Wrap_E2E.h"
}

namespace
{

// 検証対象フレームの組み立て用のローカル E2E 設定（本番の
// E2EXf_EngineInfoCfgP05 と同じ DataID/DataLength/Offset。
// test/test_chain/Bsw_RxE2EChain_test.cpp の kRefEngineInfoCfg と同じ）。
const E2E_P05ConfigType kRefEngineInfoCfg = {
    0x0100U,  /* DataID */
    7U,       /* DataLength */
    1U,       /* MaxDeltaCounter */
    0U        /* Offset */
};

class Bsw_E2EXf_SMCheckFailure_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        FakeDem_Reset();
        WrapE2ESMCheck_Reset();

        // E2EXf_PBCfg_Init() が E2EXf_EngineInfoRxCfg の CheckState/SMState/
        // WaitForFirstData をすべて初期状態へ戻す（E2E_SMCheckInit() 込み）。
        E2EXf_PBCfg_Init();

        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;  // DeInit() のログを抑制
        E2EXf_DeInit();
    }

    // kRefEngineInfoCfg に基づき、独立した Protect 状態でフレームを組み立てる
    // （test/test_chain/Bsw_RxE2EChain_test.cpp の BuildFrame() と同じ）。
    static void BuildFrame(uint8 (&buf)[7], E2E_P05ProtectStateType* state)
    {
        buf[3] = 0x01U;  // EngineSpeed=500rpm 相当
        buf[4] = 0xF4U;
        buf[5] = 0U;
        buf[6] = 0U;
        E2E_P05Protect(&kRefEngineInfoCfg, state, buf, 7U);
    }
};

// ------------------------------------------------------------
// 正常系（回帰確認）: SMCheck が素通りする場合、CRC/Counter が正しい初回
// フレームは受理され、Dem へ PASSED が報告される。
// ------------------------------------------------------------
TEST_F(Bsw_E2EXf_SMCheckFailure_Test, InverseTransformP05_OK_ValidFrameReportsPassedViaDem)
{
    /* 準備 (Arrange): E2E_SMCheck() の状態機械は1回目の呼び出しで
     * NODATA→INITに遷移するだけでカウントを取らず(E2E.c参照)、2回目以降
     * ようやくOkCountを積み上げる。MinOkStateInit=2に達するには3回連続の
     * 正常フレームが要る。 */
    uint8 buf1[7] = { 0U };
    uint8 buf2[7] = { 0U };
    uint8 buf3[7] = { 0U };
    E2E_P05ProtectStateType refState;
    E2E_P05ProtectInit(&refState);
    BuildFrame(buf1, &refState);
    BuildFrame(buf2, &refState);
    BuildFrame(buf3, &refState);

    /* 実行 (Act) */
    E2E_P05StatusType checkStatus = E2E_P05STATUS_ERROR;
    (void)E2EXf_InverseTransformP05(&E2EXf_EngineInfoRxCfg, buf1, 7U, &checkStatus);
    (void)E2EXf_InverseTransformP05(&E2EXf_EngineInfoRxCfg, buf2, 7U, &checkStatus);
    const Std_ReturnType ret =
        E2EXf_InverseTransformP05(&E2EXf_EngineInfoRxCfg, buf3, 7U, &checkStatus);

    /* 評価 (Assert) */
    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(checkStatus, E2E_P05STATUS_OK);
    EXPECT_EQ(WrapE2ESMCheck_CallCount, 3U);
    EXPECT_EQ(FakeDem_SetEventStatusCount, 1U);
    EXPECT_EQ(FakeDem_LastEventId, DEM_EVENT_E2E_ENGINEINFO);
    EXPECT_EQ(FakeDem_LastEventStatus, DEM_EVENT_STATUS_PASSED);
}

// ------------------------------------------------------------
// 2026-09 新規カバー: E2E_SMCheck() が E2E_E_OK 以外を返した場合、
// [SWS_E2EXf_00027]により戻り値は E_SAFETY_SOFT_RUNTIMEERROR になる。
// CheckStatus 自体（生データの合否判定）は SMCheck 呼び出し前に確定済みの
// ため書き換わらず、Dem への PASSED/FAILED 報告だけが保留される
// （フェイルセーフ側）。
// ------------------------------------------------------------
TEST_F(Bsw_E2EXf_SMCheckFailure_Test, InverseTransformP05_NG_SMCheckFailureReturnsSafetySoftRuntimeErrorAndSkipsDemReport)
{
    /* 準備 (Arrange): CRC/Counter は正しいフレームを用意した上で、
     * E2E_SMCheck() だけを強制失敗させる。 */
    uint8 buf[7] = { 0U };
    E2E_P05ProtectStateType refState;
    E2E_P05ProtectInit(&refState);
    BuildFrame(buf, &refState);

    WrapE2ESMCheck_ForceFail    = 1U;
    WrapE2ESMCheck_ForcedReturn = E2E_E_WRONGSTATE;

    /* 実行 (Act) */
    E2E_P05StatusType checkStatus = E2E_P05STATUS_ERROR;
    const Std_ReturnType ret =
        E2EXf_InverseTransformP05(&E2EXf_EngineInfoRxCfg, buf, 7U, &checkStatus);

    /* 評価 (Assert): [SWS_E2EXf_00027] 準拠で戻り値は
     * E_SAFETY_SOFT_RUNTIMEERROR（2026-09 追加、以前は誤って通常の
     * E_OK/E_NOT_OK を返していた）。CheckStatus は E2E_SMCheck() の前に
     * 確定済みのため正常系と変わらない（E2EXf.c のコメント「フレームが
     * 使えるかの判定は Dem 報告方針とは別物」参照）。Dem への報告はゼロ件の
     * まま（E2EXf_ReportSMVerdict() が SMCheck 失敗を検知して return する
     * ため）。 */
    EXPECT_EQ(ret, E_SAFETY_SOFT_RUNTIMEERROR);
    EXPECT_EQ(checkStatus, E2E_P05STATUS_OK);
    EXPECT_EQ(WrapE2ESMCheck_CallCount, 1U);
    EXPECT_EQ(FakeDem_SetEventStatusCount, 0U);
}

// ------------------------------------------------------------
// 上のテストの直後、SMCheck が正常に戻れば通常通り報告を再開する
// （一時的な防御分岐であり、以降の呼び出しへ悪影響を残さないことの確認）。
// ------------------------------------------------------------
TEST_F(Bsw_E2EXf_SMCheckFailure_Test, InverseTransformP05_OK_ResumesReportingAfterSMCheckRecovers)
{
    /* 準備 (Arrange): 1回目は SMCheck を強制失敗させる。この間 E2E_SMCheck()
     * の実体は一切呼ばれない（wrap がパススルーせず即座に戻り値を返す
     * ため）ので、実ステートマシンは NODATA のまま進んでいない。以降の
     * 3フレームは通常通り実体を通し、上のテストと同じ理由で3回連続の
     * 正常フレームが必要（NODATA→INITへの遷移だけの1回目、OkCount 1回目・
     * 2回目でようやくVALIDへ）。 */
    uint8 buf1[7] = { 0U };
    uint8 buf2[7] = { 0U };
    uint8 buf3[7] = { 0U };
    uint8 buf4[7] = { 0U };
    E2E_P05ProtectStateType refState;
    E2E_P05ProtectInit(&refState);
    BuildFrame(buf1, &refState);
    BuildFrame(buf2, &refState);
    BuildFrame(buf3, &refState);
    BuildFrame(buf4, &refState);

    WrapE2ESMCheck_ForceFail = 1U;
    E2E_P05StatusType checkStatus = E2E_P05STATUS_ERROR;
    (void)E2EXf_InverseTransformP05(&E2EXf_EngineInfoRxCfg, buf1, 7U, &checkStatus);
    ASSERT_EQ(FakeDem_SetEventStatusCount, 0U);
    WrapE2ESMCheck_ForceFail = 0U;  // 以降はパススルー（実体成功）

    /* 実行 (Act) */
    (void)E2EXf_InverseTransformP05(&E2EXf_EngineInfoRxCfg, buf2, 7U, &checkStatus);
    (void)E2EXf_InverseTransformP05(&E2EXf_EngineInfoRxCfg, buf3, 7U, &checkStatus);
    (void)E2EXf_InverseTransformP05(&E2EXf_EngineInfoRxCfg, buf4, 7U, &checkStatus);

    /* 評価 (Assert): 強制失敗が悪影響を残さず、実フレーム3回分で通常通り
     * Dem へ PASSED を報告する */
    EXPECT_EQ(WrapE2ESMCheck_CallCount, 4U);
    EXPECT_EQ(FakeDem_SetEventStatusCount, 1U);
    EXPECT_EQ(FakeDem_LastEventStatus, DEM_EVENT_STATUS_PASSED);
}

}  // namespace
