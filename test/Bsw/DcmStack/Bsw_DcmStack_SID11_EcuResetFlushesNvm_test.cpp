/**
 * \file    Bsw_DcmStack_SID11_EcuResetFlushesNvm_test.cpp
 * \brief   UDS SID 0x11 ECUReset が、リセット直前に NvM の書き込み待ちジョブを
 *          EEPROM へ書き切ること（NvM_WriteAll()）のフルコールチェーンテスト
 *          （GoogleTest / CMake native_chain_tests）。
 *
 * \details 背景: Dem は DTC の確定や消去のたびに NvM_WriteBlock() で書き込みを積み、
 *          実際の EEPROM 書き込みは NvM_MainFunction()/MemIf_MainFunction() が
 *          10ms 周期で 1 バイトずつ非同期に進める。ECUReset は正応答の後に
 *          delay() で待ってから MCU をリセットするが、その間は Os が止まって
 *          書き込みが進まないため、SID 0x14 で DTC をクリアした直後（約 0.6 秒以内）
 *          などにリセットすると保存途中で失われ、クリアしたはずの DTC が
 *          復活していた（[SWS_Dem_00341]、EcuM のシャットダウンでの
 *          NvM_WriteAll() 相当を ECUReset 経路に追加）。
 *
 *          本ファイルは NvM/MemIf/Fee を実体でリンクして検証する
 *          （他の DcmStack テストは NvM を初期化せず、Dem_Init() は常に初回起動扱い）。
 */
#include <gtest/gtest.h>

extern "C" {
#include "Can.h"
#include "Can_Hw.h"
#include "CanIf.h"
#include "CanSM.h"
#include "PduR.h"
#include "CanTp.h"
#include "CanTp_Cfg.h"
#include "Dcm.h"
#include "Dcm_Cfg.h"
#include "Dem.h"
#include "Mcu.h"
#include "NvM.h"
#include "NvM_Cfg.h"
#include "MemIf.h"
#include "Dem_Cfg.h"
#include "Fake_Can_Hw.h"
#include "Fake_Det_Hw.h"
#include "Fake_Mcu_Hw.h"
#include "Fake_Millis.h"
#include "Wrap_Can.h"
#include "Wrap_CanIf.h"
#include "Wrap_PduR.h"
#include "Wrap_CanTp.h"
}

namespace
{

// -----------------------------------------------------------------------
// テスト専用の最小 CanIf/PduR 設定（Bsw_DcmStack_SID19_SF01_ReadDtcCount_test.cpp
// と同一。ファイル冒頭コメント参照）。
// -----------------------------------------------------------------------

const CanIf_RxPduConfigType kTestCanIfRxPdu = {
    /* CanId */                0x7E0U,
    /* Hrh */                  0U,  /* Can_MainFunction_Read() が構築する Mailbox は常に Hoh=0 */
    /* UpperLayerRxPduId */    0U,
    /* Dlc */                  8U,
    /* RxIndicationFct */      PduR_CanIfRxIndication,
    /* ReadRxPduDataEnabled */ 0U
};

const CanIf_TxPduConfigType kTestCanIfTxPdu = {
    /* UpperLayerTxPduId */ 0U,
    /* CanId */             0x7E8U,
    /* Dlc */               8U,
    /* Hth */               0U,
    /* TxConfirmFct */      PduR_CanIfTxConfirmation
};

const CanIf_ConfigType kTestCanIfConfig = {
    /* TxPduConfig */ &kTestCanIfTxPdu,
    /* TxPduCount */  1U,
    /* RxPduConfig */ &kTestCanIfRxPdu,
    /* RxPduCount */  1U
};

const PduR_RxDestType kTestPduRRxDest = {
    /* Module */    PDUR_MODULE_CANTP,
    /* DestPduId */ CANTP_RX_SDU_ID,
    /* RxIndFct */  CanTp_RxIndication
};

const PduR_RxRoutingPathType kTestPduRRxPath = {
    /* SrcPduId */  0U,  /* = kTestCanIfRxPdu.UpperLayerRxPduId */
    /* Dests */     &kTestPduRRxDest,
    /* DestCount */ 1U
};

const PduR_TxRoutingPathType kTestPduRTxPath = {
    /* SrcPduId */             CANTP_PDUR_TX_SDU_ID,
    /* CanIfTxPduId */         0U,  /* = kTestCanIfTxPdu の登録順インデックス */
    /* ConfDestPduId */        0U,
    /* ConfFct */              CanTp_TxConfirmation,
    /* TransmitOverrideFct */  NULL,
    /* TransmitOverrideId */   0U
};

const PduR_PBConfigType kTestPduRConfig = {
    /* RxPaths */     &kTestPduRRxPath,
    /* RxPathCount */ 1U,
    /* TxPaths */     &kTestPduRTxPath,
    /* TxPathCount */ 1U
};

class Bsw_DcmStack_SID11_EcuResetFlushesNvm_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        FakeCanHw_Reset();
        FakeMcuHw_Reset();
        WrapCan_Reset();
        WrapCanIf_Reset();
        WrapPduR_Reset();
        WrapCanTp_Reset();
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制

        canConfig.filter.filterId = 0x0000U;
        canConfig.filter.mask     = 0x0000U;
        canConfig.csPin           = 10U;
        canConfig.intPin          = 2U;
        canConfig.baudrate        = 500000U;
        canConfig.crystalFreq     = CAN_CRYSTAL_16MHZ;

        Can_Init(&canConfig);
        CanIf_Init(&kTestCanIfConfig);
        CanIf_SetControllerMode(0U, CAN_CS_STARTED);
        CanIf_SetPduMode(0U, CANIF_ONLINE);
        PduR_Init(&kTestPduRConfig);
        CanSM_Init(NULL);
        CanTp_Init(NULL);
        MemIf_Init();
        NvM_Init(NULL);
        Dem_Init(NULL);
        Mcu_Init(&Mcu_Config);
        Dcm_Init(NULL);
        NvM_WriteAll();  // 初回起動の初期化書き込みを済ませ、各テストの Act 区間だけを対象にする

        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;  // DeInit() のログを抑制
        NvM_Test_ResetInitState();
        CanSM_DeInit();
        CanIf_DeInit();
    }

    NvM_RequestResultType ResultOf(NvM_BlockIdType id)
    {
        NvM_RequestResultType r = NVM_REQ_PENDING;
        (void)NvM_GetErrorStatus(id, &r);
        return r;
    }

    Can_ConfigType canConfig;
};

// ------------------------------------------------------------
// OK: DTC を確定させた直後（EEPROM への書き込みがまだ全く進んでいない状態）に
// [0x11, 0x01] hardReset を受けると、リセット（Mcu_PerformReset()）の前に
// STATUS・EXTENDED（冗長 2 面）の書き込みが完了している。
// ------------------------------------------------------------
TEST_F(Bsw_DcmStack_SID11_EcuResetFlushesNvm_Test,
       OK_PendingDtcWritesAreCompletedBeforeMcuReset)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // limit=1 のイベントを FAILED にして DTC を確定させる
    // （Dem が STATUS と EXTENDED の書き込みを NvM に積む）。まだ NvM_MainFunction() は
    // 一度も回していないので、書き込みは PENDING のまま。
    ASSERT_EQ(Dem_SetEventStatus(DEM_EVENT_BUTTON_STUCK, DEM_EVENT_STATUS_FAILED), E_OK);
    ASSERT_EQ(ResultOf(NVM_BLOCK_ID_DEM_STATUS), NVM_REQ_PENDING);
    ASSERT_EQ(ResultOf(NVM_BLOCK_ID_DEM_EXTENDED), NVM_REQ_PENDING);

    /* [0x11, 0x01] を 0x7E0 の受信バッファへセットする（SF: 02 11 01）。 */
    FakeCanHw_RxId  = 0x7E0U;
    FakeCanHw_RxDlc = 8U;
    FakeCanHw_RxData[0] = 2U;
    FakeCanHw_RxData[1] = DCM_SID_ECU_RESET;
    FakeCanHw_RxData[2] = DCM_RESET_HARD;
    for (uint8 i = 3U; i < 8U; i++)
        FakeCanHw_RxData[i] = 0U;
    FakeCanHw_RxPendingCount = 1U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Can_MainFunction_Read();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    // 正応答が出て、リセットが要求され、書き込みが完了していること。
    ASSERT_EQ(FakeCanHw_SendCount, 1U);
    EXPECT_EQ(FakeCanHw_LastSendData[1], 0x51U);
    EXPECT_EQ(FakeMcuHw_PerformResetCount, 1U);
    EXPECT_EQ(ResultOf(NVM_BLOCK_ID_DEM_STATUS), NVM_REQ_OK);
    EXPECT_EQ(ResultOf(NVM_BLOCK_ID_DEM_EXTENDED), NVM_REQ_OK);
}

}  // namespace
