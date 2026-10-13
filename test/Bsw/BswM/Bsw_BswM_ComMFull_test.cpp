/**
 * \file    Bsw_BswM_ComMFull_test.cpp
 * \brief   EcuM が RUN で、ComM が FULL_COM のとき、I-PDU グループを起動することの単体テスト
 * \details ルール 3 と 6 は、EcuM が RUN かつ ComM が FULL_COMMUNICATION の AND 条件で、テレメトリ（TX）とセンサ受信（RX）の
 *          I-PDU グループを起動する（Com_IpduGroupStart()、初期化なし）。片方の条件だけでは成立しない。
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

/* BswM のルール評価（RUN かつ FULL_COM） の単体テスト用フィクスチャ。
 * SetUp(): Os・Com・Nm・BswM の wrap と DET の記録を初期化し、BswM_Init(&BswM_Config)（本番のルール表）を呼ぶ
 * （Init 中のログは抑制し、Init 自体の記録は消す）。呼び出し回数は、各ルールの Action が呼ぶ関数の wrap で数える。
 * TearDown(): BswM_Deinit() で未初期化へ戻す。 */
class Bsw_BswM_ComMFull_Test : public ::testing::Test
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

TEST_F(Bsw_BswM_ComMFull_Test, BswM_ComM_CurrentMode_OK_FullComStartsBothIpduGroupsOnlyWhenRun)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で BswM_Init() 済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BswM_ComM_CurrentMode(COMM_CHANNEL_0, COMM_FULL_COMMUNICATION);   // EcuM はまだ RUN でない
    const uint32 beforeRun = CallCount_Com_IpduGroupStart;
    BswM_EcuM_CurrentState(ECUM_STATE_RUN);          // AND 条件が成立する
    const uint32 afterRun = CallCount_Com_IpduGroupStart;

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(beforeRun, 0U);   // AND 条件の片方だけでは起動しない
    EXPECT_EQ(afterRun, 2U);    // テレメトリ（TX）とセンサ受信（RX）の 2 グループ
}

}  // namespace
