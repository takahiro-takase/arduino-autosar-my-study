/**
 * \file    NmStack_BusOffRecoveryConfirmed_test.cpp
 * \brief   Bus-Off 回復の確定（G_BUS_OFF_PASSIVE、[SWS_CanSM_00496]〜[00498]）の、実際の配線を通したテスト
 *          （GoogleTest / PlatformIO `[env:native_chain]`）。
 *
 * \details CanSM_MainFunction_test.cpp は、CanIf_GetTxConfirmationState() の戻り値を差し替えて確定条件を検証する。
 *          本ファイルは差し替えずに、実体の CanNm → CanIf → Can（模擬 Can_Hw）→ CanIf_TxConfirmation() →
 *          CanIf_GetTxConfirmationState() → CanSM_MainFunction() の経路を通して、次を確認する。
 *            - 再起動の直後は、TX 確認が無いので回復は確定せず、Dem へ PASSED を報告しない。
 *            - 再起動後に NM フレームを送り、その送信確認が CanIf へ届くと、次の CanSM_MainFunction() で回復が確定し、
 *              Dem へ PASSED を 1 回報告する。
 *            - 確定した後は、同じ回復について重ねて報告しない。
 *            - 回復の再起動より前に届いていた送信確認は数えない（再起動のたびに CanIf が状態をリセットする）。
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
#include "Wrap_ComM.h"
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

/** CANNM_CANIF_TX_PDU_ID（CanNm_Cfg.h）だけを有効にした TX PDU 設定。CanNm の NM フレームが
 *  CanIf_Transmit() → Can_Write() → 模擬 Can_Hw まで到達し、その送信確認が CanNm_TxConfirmation() へ戻る。
 *  index 0/1 はダミー。 */
const CanIf_TxPduConfigType kBorTxPduConfig[3] = {
    { 0U, 0U, 0U, 0U, NULL },
    { 0U, 0U, 0U, 0U, NULL },
    { /* UpperLayerTxPduId */ CANNM_CANIF_TX_PDU_ID,
      /* CanId */             0x400U,
      /* Dlc */               CANNM_DLC,
      /* Hth */               0U,
      /* TxConfirmFct */      CanNm_TxConfirmation }
};

const CanIf_ConfigType kBorCanIfConfig = {
    /* TxPduConfig */ kBorTxPduConfig,
    /* TxPduCount */  3U,
    /* RxPduConfig */ NULL,
    /* RxPduCount */  0U
};

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* Bus-Off 回復の確定を、実際の配線で確かめるためのフィクスチャ。
 * SetUp(): 模擬 Can_Hw・Dem・EcuM と wrap をリセットし、Dem_Init(NULL)・Can_Init()・CanIf_Init(&kBorCanIfConfig)・CanSM_Init(NULL)・ComM_Init(NULL)・CanNm_Init(NULL)・Nm_Init(NULL) の順に初期化し、
 * FULL_COM を要求して CanNm を Normal Operation State まで進める。 */
class NmStack_BusOffRecoveryConfirmed_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeCanHw_Reset();
        WrapCanIf_Reset();
        WrapCan_Reset();
        WrapComM_Reset();
        WrapDem_Reset();
        FakeEcuM_Reset();
        WrapBswM_Reset();
        FakeMillis_Reset();
        FakeDetHw_LogSuppressed = 1U;
        Dem_Init(NULL);

        canConfig.filter.filterId = 0x0220U;
        canConfig.filter.mask     = 0x1FFFU;
        canConfig.csPin           = 10U;
        canConfig.intPin          = 2U;
        canConfig.baudrate        = 500000U;
        canConfig.crystalFreq     = CAN_CRYSTAL_16MHZ;

        Can_Init(&canConfig);
        CanIf_Init(&kBorCanIfConfig);
        CanSM_Init(NULL);
        ComM_Init(NULL);
        ComM_CommunicationAllowed(COMM_CHANNEL_0, TRUE);
        Can_MainFunction_Mode();
        CanNm_Init(NULL);
        Nm_Init(NULL);

        ASSERT_EQ(ComM_RequestComMode(COMM_USER_0, COMM_FULL_COMMUNICATION), E_OK);
        Can_MainFunction_Mode();
        ASSERT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);
        FakeMillis_Value += CANNM_REPEAT_MESSAGE_MS + 100UL;
        CanNm_MainFunction();
        Can_MainFunction_Mode();

        FakeCanHw_Reset();
        WrapDem_Reset();
        FakeEcuM_Reset();
        WrapBswM_Reset();
        WrapCanIf_Reset();
        WrapCan_Reset();

        FakeDetHw_LogSuppressed = 0U;
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        CanNm_DeInit();
        ComM_DeInit();
        CanSM_DeInit();
        CanIf_DeInit();
    }

    /** Bus-Off を起こし、L1 の間隔を空けて CanSM_MainFunction() で再起動させる（FULL_COM へ戻る）。 */
    void BusOffAndRestart()
    {
        CanSM_ControllerBusOff(0U);
        ASSERT_EQ(Can_Test_GetControllerState(), CAN_CS_STOPPED);
        FakeMillis_Value += static_cast<unsigned long>(CANSM_BUSOFF_RECOVERY_L1_MS) + 1UL;
        CanSM_MainFunction();
        ASSERT_EQ(Can_Test_GetControllerState(), CAN_CS_STARTED);
    }

    /** CanNm に NM フレームを 1 つ送らせ、その送信確認を CanIf まで届ける。 */
    void SendNmFrameAndConfirm()
    {
        const uint32 sentBefore = FakeCanHw_SendCount;
        FakeMillis_Value += CANNM_CYCLE_MS;
        CanNm_MainFunction();
        Can_MainFunction_Mode();
        ASSERT_GT(FakeCanHw_SendCount, sentBefore);   // 模擬 Can_Hw まで届いた
        Can_MainFunction_Write();                     // 保留中の TX 確認を、CanIf_TxConfirmation() へ渡す
    }

    Can_ConfigType canConfig;
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

// ------------------------------------------------------------
// 再起動しただけ（TX 確認がまだ無い）では、回復は確定しない。
// ------------------------------------------------------------
TEST_F(NmStack_BusOffRecoveryConfirmed_Test, NG_RecoveryNotConfirmedBeforeAnyTxConfirmation)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    BusOffAndRestart();
    WrapDem_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    FakeMillis_Value += 1000UL;
    CanSM_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CanIf_GetTxConfirmationState(0U), CANIF_NO_NOTIFICATION);
    EXPECT_EQ(CallCount_Dem_SetEventStatus, 0U);
}

// ------------------------------------------------------------
// 再起動後に NM フレームを送り、その送信確認が届くと、回復が確定する（Dem へ PASSED を 1 回）。
// 確定した後は、同じ回復について重ねて報告しない。
// ------------------------------------------------------------
TEST_F(NmStack_BusOffRecoveryConfirmed_Test, OK_TxConfirmationAfterRestartConfirmsRecovery)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    BusOffAndRestart();
    WrapDem_Reset();
    SendNmFrameAndConfirm();
    ASSERT_EQ(CanIf_GetTxConfirmationState(0U), CANIF_TX_RX_NOTIFICATION);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanSM_MainFunction();
    CanSM_MainFunction();   // 2 回目は、重ねて報告しない

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CallCount_Dem_SetEventStatus, 1U);
    EXPECT_EQ(LastEventStatus_Dem_SetEventStatus, DEM_EVENT_STATUS_PASSED);
}

// ------------------------------------------------------------
// 回復の再起動より前に届いた送信確認は、数えない。
// 再起動（CanIf_SetControllerMode(STARTED)）のたびに、CanIf が TX 確認の状態をリセットするため
// （[SWS_CANIF_00734]）、1 回目の回復の確定に使った確認が、2 回目の回復を確定させることはない。
// ------------------------------------------------------------
TEST_F(NmStack_BusOffRecoveryConfirmed_Test, OK_ConfirmationBeforeRestartIsNotCounted)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    BusOffAndRestart();
    SendNmFrameAndConfirm();
    CanSM_MainFunction();                 // 1 回目の回復が確定する
    WrapDem_Reset();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BusOffAndRestart();                   // 2 回目の Bus-Off と再起動。TX 確認は、まだ無い
    CanSM_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(CanIf_GetTxConfirmationState(0U), CANIF_NO_NOTIFICATION);   // 再起動でリセットされている
    EXPECT_EQ(CallCount_Dem_SetEventStatus, 1U);                           // 2 回目の Bus-Off の PREFAILED だけ
    EXPECT_EQ(LastEventStatus_Dem_SetEventStatus, DEM_EVENT_STATUS_PREFAILED);   // 2 回目の回復は、まだ確定しない（PASSED なし）
}

}  // namespace
