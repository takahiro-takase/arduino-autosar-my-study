/**
 * \file    BswM_ComMOrRule_test.cpp
 * \brief   OR 条件のルールが、成立が変化したときにだけ Action を実行することの単体テスト
 * \details ルール 5 は、ComM が SILENT_COM または NO_COM（OR 条件）のとき、テレメトリの I-PDU グループを停止する。SILENT_COM で成立して
 *          1 回実行した後、NO_COM になっても、ルール 5 の結果は成立のまま変わらないので、再実行しない。
 *          NO_COM で成立するルール 7（センサ受信のグループを停止）だけが、新たに実行される。
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

/* BswM のルール評価（OR 条件と、成立の変化のときだけ実行） の単体テスト用フィクスチャ。
 * SetUp(): Os・Com・Nm・BswM の wrap と DET の記録を初期化し、BswM_Init(&BswM_Config)（本番のルール表）を呼ぶ
 * （Init 中のログは抑制し、Init 自体の記録は消す）。呼び出し回数は、各ルールの Action が呼ぶ関数の wrap で数える。
 * TearDown(): BswM_Deinit() で未初期化へ戻す。 */
class BswM_ComMOrRule_Test : public ::testing::Test
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

TEST_F(BswM_ComMOrRule_Test, BswM_ComM_CurrentMode_OK_OrRuleFiresOnceWhileResultStaysTrue)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で BswM_Init() 済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BswM_ComM_CurrentMode(COMM_CHANNEL_0, COMM_SILENT_COMMUNICATION);
    const uint32 afterSilent = CallCount_Com_IpduGroupStop;
    BswM_ComM_CurrentMode(COMM_CHANNEL_0, COMM_NO_COMMUNICATION);
    const uint32 afterNoCom = CallCount_Com_IpduGroupStop;

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(afterSilent, 1U);   // ルール 5（OR）が成立し、テレメトリを停止する
    EXPECT_EQ(afterNoCom, 2U);    // ルール 5 は再実行しない。ルール 7 が、センサ受信を停止する（+1 だけ）
}

}  // namespace
