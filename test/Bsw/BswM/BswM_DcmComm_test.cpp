/**
 * \file    BswM_DcmComm_test.cpp
 * \brief   診断の通信制御（UDS 0x28）の要求が、Com と Nm の有効・無効へ反映されることの単体テスト
 * \details BswM_Dcm_CommunicationMode_CurrentState() の要求モードが、通常の通信（Com）、NM、両方のどれを対象にするかで、
 *          Com_SetCommunicationEnabled() と Nm_Enable/DisableCommunication() のどちらを呼ぶかが変わる。
 *          同じモードを続けて通知しても、再実行しない。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "BswM.h"
#include "BswM_Cfg.h"
#include "BswM_PBCfg.h"
#include "EcuM.h"
#include "ComM.h"
#include "Dcm_Types.h"
#include "Com_Cfg.h"
#include "Nm_Cfg.h"
#include "Wrap_Os.h"
#include "Wrap_Com.h"
#include "Wrap_Nm.h"
#include "Wrap_BswM.h"
#include "Fake_Det_Hw.h"
}

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

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

/* BswM のルール評価（Dcm の通信制御） の単体テスト用フィクスチャ。
 * SetUp(): Os・Com・Nm・BswM の wrap と DET の記録を初期化し、BswM_Init(&BswM_Config)（本番のルール表）を呼ぶ
 * （Init 中のログは抑制し、Init 自体の記録は消す）。呼び出し回数は、各ルールの Action が呼ぶ関数の wrap で数える。
 * TearDown(): BswM_Deinit() で未初期化へ戻す。 */
class BswM_DcmComm_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;
        WrapOs_Reset();
        WrapCom_Reset();
        WrapNm_Reset();
        WrapBswM_Reset();
        FakeDetHw_Reset();
        BswM_Init(&BswM_Config);
        ResetCounts();
        FakeDetHw_Reset();
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        BswM_Deinit();
    }

    /* Init 中の記録を消す。 */
    static void ResetCounts()
    {
        WrapOs_Reset();
        WrapCom_Reset();
        WrapNm_Reset();
    }

    /* マスクの 1 のビットの数（その分だけ Os_SetTaskActive() が呼ばれる）。 */
    static uint32 Bits(uint32 mask)
    {
        uint32 n = 0U;
        for (; mask != 0U; mask >>= 1U)
        {
            n += (mask & 1U);
        }
        return n;
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(BswM_DcmComm_Test, BswM_Dcm_CommunicationMode_CurrentState_OK_NormalAffectsComOnly)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で BswM_Init() 済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BswM_Dcm_CommunicationMode_CurrentState(NM_MAIN_NETWORK_HANDLE, DCM_DISABLE_RX_TX_NORM);
    const uint32 comAfterFirst = CallCount_Com_SetCommunicationEnabled;
    BswM_Dcm_CommunicationMode_CurrentState(NM_MAIN_NETWORK_HANDLE, DCM_DISABLE_RX_TX_NORM);   // 同じモードの再通知

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(comAfterFirst, 1U);
    EXPECT_EQ(CallCount_Com_SetCommunicationEnabled, 1U);   // 再実行しない
    EXPECT_EQ(CallCount_Nm_DisableCommunication, 0U);       // 通常の通信だけが対象
    EXPECT_EQ(CallCount_Nm_EnableCommunication, 0U);
}

TEST_F(BswM_DcmComm_Test, BswM_Dcm_CommunicationMode_CurrentState_OK_NmDisableAndEnableAffectNmOnly)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で BswM_Init() 済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BswM_Dcm_CommunicationMode_CurrentState(NM_MAIN_NETWORK_HANDLE, DCM_DISABLE_RX_TX_NM);
    const uint32 disabled = CallCount_Nm_DisableCommunication;
    BswM_Dcm_CommunicationMode_CurrentState(NM_MAIN_NETWORK_HANDLE, DCM_ENABLE_RX_TX_NM);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(disabled, 1U);                                // 送信が無効のモード → Nm_DisableCommunication()
    EXPECT_EQ(CallCount_Nm_EnableCommunication, 1U);        // 送信が有効のモード → Nm_EnableCommunication()
    EXPECT_EQ(CallCount_Com_SetCommunicationEnabled, 0U);   // NM だけが対象
}

TEST_F(BswM_DcmComm_Test, BswM_Dcm_CommunicationMode_CurrentState_OK_NormalAndNmAffectBoth)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で BswM_Init() 済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BswM_Dcm_CommunicationMode_CurrentState(NM_MAIN_NETWORK_HANDLE, DCM_DISABLE_RX_TX_NORM_NM);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Com_SetCommunicationEnabled, 1U);   // 通常の通信と NM の両方が対象
    EXPECT_EQ(CallCount_Nm_DisableCommunication, 1U);
}

TEST_F(BswM_DcmComm_Test, BswM_Dcm_CommunicationMode_CurrentState_OK_EnableRxDisableTxEnablesOnlyRx)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で BswM_Init() 済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BswM_Dcm_CommunicationMode_CurrentState(NM_MAIN_NETWORK_HANDLE, DCM_ENABLE_RX_DISABLE_TX_NORM);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Com_SetCommunicationEnabled, 1U);
    EXPECT_EQ(LastRxEnabled_Com_SetCommunicationEnabled, 1U);   // Rx は有効
    EXPECT_EQ(LastTxEnabled_Com_SetCommunicationEnabled, 0U);   // Tx は無効
}

TEST_F(BswM_DcmComm_Test, BswM_Dcm_CommunicationMode_CurrentState_OK_DisableRxEnableTxEnablesOnlyTx)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で BswM_Init() 済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BswM_Dcm_CommunicationMode_CurrentState(NM_MAIN_NETWORK_HANDLE, DCM_DISABLE_RX_ENABLE_TX_NORM);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Com_SetCommunicationEnabled, 1U);
    EXPECT_EQ(LastRxEnabled_Com_SetCommunicationEnabled, 0U);   // Rx は無効
    EXPECT_EQ(LastTxEnabled_Com_SetCommunicationEnabled, 1U);   // Tx は有効
}

TEST_F(BswM_DcmComm_Test, BswM_Dcm_CommunicationMode_CurrentState_OK_NmEnableTxOnlyEnablesNm)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で BswM_Init() 済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BswM_Dcm_CommunicationMode_CurrentState(NM_MAIN_NETWORK_HANDLE, DCM_DISABLE_RX_ENABLE_TX_NM);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Nm_EnableCommunication, 1U);    // 送信が有効なモード → Nm_EnableCommunication()
    EXPECT_EQ(CallCount_Nm_DisableCommunication, 0U);
    EXPECT_EQ(CallCount_Com_SetCommunicationEnabled, 0U);   // NM だけが対象
}

}  // namespace
