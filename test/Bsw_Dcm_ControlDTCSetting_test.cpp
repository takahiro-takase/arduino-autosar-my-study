/**
 * \file    Bsw_Dcm_ControlDTCSetting_test.cpp
 * \brief   UDS SID 0x85 ControlDTCSetting のうち、`Wrap_CanTp.h` で CanTp の
 *          ビジー状態を強制注入する境界フォールトインジェクションのテスト
 *          （GoogleTest / PlatformIO `[env:native_chain]`）。
 *
 * \details 大半のシナリオ（UDS要求データと外部データ（Dem状態・セッション
 *          状態）の組み合わせで確認できる内容）は
 *          Bsw_DcmStack_SID85_ControlDTCSettingChain_test.cpp（物理層 Can_Hw
 *          までの検証）へ移植済み（2026-09）。本ファイルに残る2件
 *          （S3Timer_OK_*）は、CanTp が実際にビジーかどうかではなく
 *          `Wrap_CanTp.h`（`FailFromCallCount_CanTp_IsTxBusy`）で人工的に
 *          ビジー状態を作り出して Dcm の S3 タイマー一時停止ロジック
 *          （[SWS_Dcm_00141]）を検証するものであり、物理チェーンとは無関係
 *          なため、引き続き `Dcm_ComIndication()` に生の UDS バイト列を
 *          直接渡すブラックボックステストのまま残す。
 */
#include <gtest/gtest.h>

extern "C" {
#include "Dcm.h"
#include "Dcm_Cfg.h"
#include "Dem.h"
#include "Wrap_CanTp.h"
#include "Fake_Millis.h"
#include "Fake_Det_Hw.h"
#include "Wrap_ComM.h"
}

namespace
{

class Bsw_Dcm_ControlDTCSetting_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        WrapCanTp_Reset();
        Suppressed_ComM_DcmDiagnostic = 1U;  // 本テストは通信管理(ComM/CanSM/Nm)が対象外
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制

        CanTp_Init(NULL);
        Dem_Init(NULL);
        Dcm_Init(NULL);

        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        WrapComM_Reset();  // 他のテストファイルへ影響を残さない
    }

    /** UDS ペイロードを Dcm_ComIndication() へ直接渡す。 */
    void Send(const uint8* payload, uint8 len)
    {
        PduInfoType pdu = { const_cast<uint8*>(payload), len };
        Dcm_ComIndication(0U, &pdu);
    }

    /** [0x10, 0x03] extendedDiagnosticSession へ遷移する（0x85 の前提）。 */
    void EnterExtendedSession()
    {
        uint8 req[2] = { DCM_SID_SESSION_CTRL, DCM_SESSION_EXTENDED };
        Send(req, sizeof(req));
        ASSERT_EQ(LastData_CanTp_Transmit[0], 0x50U);  // 正応答確認（前提が崩れていないこと）
        WrapCanTp_Reset();
    }

    /** [0x85, subFunc] を送る。 */
    void SendControlDTCSetting(uint8 subFunc)
    {
        uint8 req[2] = { DCM_SID_CONTROL_DTC_SETTING, subFunc };
        Send(req, sizeof(req));
    }
};

TEST_F(Bsw_Dcm_ControlDTCSetting_Test, S3Timer_OK_DoesNotTimeOutWhileCanTpTxBusy)
{
    /* 準備 (Arrange): extendedSession へ遷移。 */
    EnterExtendedSession();

    /* 実行 (Act): CanTp TX がビジー状態(前回応答、特にマルチフレームの
     * 送信未完了を模擬)のまま S3 タイムアウト相当の時間が経過しても、
     * [SWS_Dcm_00141] によりタイマは進まないはず。 */
    FailFromCallCount_CanTp_IsTxBusy = 1U;
    FakeMillis_Value += DCM_S3_TIMEOUT_MS + 1UL;
    Dcm_MainFunction();
    FailFromCallCount_CanTp_IsTxBusy = WRAP_CANTP_FAIL_FROM_CALL_COUNT_DISABLED;

    /* 評価 (Assert): defaultSession へ落ちていないこと
     * (extendedSession 限定の 0x85 が引き続き正応答を返すことで確認)。 */
    WrapCanTp_Reset();
    SendControlDTCSetting(DCM_DTCSETTING_ON);
    ASSERT_EQ(LastData_CanTp_Transmit[0], (uint8)(DCM_SID_CONTROL_DTC_SETTING + 0x40U));
}

TEST_F(Bsw_Dcm_ControlDTCSetting_Test, S3Timer_OK_TimesOutNormallyOnceCanTpTxIdleAgain)
{
    /* 準備 (Arrange): extendedSession へ遷移し、ビジー中はタイムアウトしない
     * ことを確認する（前のテストと同じ前提）。 */
    EnterExtendedSession();
    FailFromCallCount_CanTp_IsTxBusy = 1U;
    FakeMillis_Value += DCM_S3_TIMEOUT_MS + 1UL;
    Dcm_MainFunction();

    /* 実行 (Act): CanTp TX がアイドルへ戻った後、改めて S3 タイムアウト分の
     * 時間を経過させる。 */
    FailFromCallCount_CanTp_IsTxBusy = WRAP_CANTP_FAIL_FROM_CALL_COUNT_DISABLED;
    Dcm_MainFunction();
    FakeMillis_Value += DCM_S3_TIMEOUT_MS + 1UL;
    Dcm_MainFunction();

    /* 評価 (Assert): 通常通り defaultSession へ落ちていること
     * (0x85 が NRC 0x7F で拒否される)。 */
    WrapCanTp_Reset();
    SendControlDTCSetting(DCM_DTCSETTING_ON);
    ASSERT_EQ(LastData_CanTp_Transmit[0], DCM_SID_NEGATIVE_RESP);
}

}  // namespace
