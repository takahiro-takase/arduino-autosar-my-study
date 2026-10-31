/**
 * \file    PduR_EnableRouting_test.cpp
 * \brief   PduR_EnableRouting() の単体テスト
 * \details ルーティングパスグループ（[SWS_PduR_00615]/[SWS_PduR_00617]、[SWS_PDUR_00805]）を検証する。
 *          各テストは、kTestConfig（上の定義）の 3 つのグループと、3 本ずつの TX/RX パスを使う。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "PduR.h"
#include "Fake_Det_Hw.h"
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

// 呼ばれた回数を数えるだけの、テスト用の関数。
uint32 gOverrideCalls;
uint32 gRxCalls;
uint32 gConfCalls;
Std_ReturnType OverrideTransmit(PduIdType, const PduInfoType*) { gOverrideCalls++; return E_OK; }
void           CountRx(PduIdType, const PduInfoType*)          { gRxCalls++; }
void           CountConf(PduIdType, Std_ReturnType)            { gConfCalls++; }

// TX パス 3 本（SrcPduId 10 / 11 / 12）。送信は TransmitOverrideFct へ渡るので、CanIf を使わずに、渡ったかを数えられる。
const PduR_TxRoutingPathType kTxPaths[] = {
    { 10U, 0U, 20U, CountConf, OverrideTransmit, 7U },
    { 11U, 1U, 21U, CountConf, OverrideTransmit, 8U },
    { 12U, 2U, 22U, CountConf, OverrideTransmit, 9U },
};

// RX パス 3 本（SrcPduId 40 / 41 / 42）。配信先は、呼ばれた回数を数える関数。
const PduR_RxDestType kRxDest = { PDUR_MODULE_COM, 30U, CountRx };
const PduR_RxRoutingPathType kRxPaths[] = {
    { 40U, &kRxDest, 1U },
    { 41U, &kRxDest, 1U },
    { 42U, &kRxDest, 1U },
};

// ルーティングパスグループ 3 つ（すべて、起動時は有効）。
//   グループ 0: TX 10、RX 40
//   グループ 1: TX 11、RX 41
//   グループ 2: TX 10（TX 10 は、グループ 0 とグループ 2 の両方に所属する）
// TX 12 と RX 42 は、どのグループにも所属しない（常に有効）。
const PduIdType kG0Tx[] = { 10U };
const PduIdType kG0Rx[] = { 40U };
const PduIdType kG1Tx[] = { 11U };
const PduIdType kG1Rx[] = { 41U };
const PduIdType kG2Tx[] = { 10U };

const PduR_RoutingPathGroupType kGroups[] = {
    { TRUE, kG0Rx, 1U, kG0Tx, 1U },
    { TRUE, kG1Rx, 1U, kG1Tx, 1U },
    { TRUE, NULL,  0U, kG2Tx, 1U },
};

const PduR_PBConfigType kTestConfig = {
    kRxPaths, 3U,
    kTxPaths, 3U,
    /* PBConfigId */            5U,
    /* RoutingPathGroups */     kGroups,
    /* RoutingPathGroupCount */ 3U
};

// SrcPduId の TX パスへ送信を要求する。戻り値を返す。
Std_ReturnType Transmit(PduIdType srcPduId)
{
    uint8 sdu[8] = { 0 };
    PduInfoType info = { sdu, 1U };
    return PduR_ComTransmit(srcPduId, &info);
}

// SrcPduId の RX パスへ受信を通知する。
void Receive(PduIdType srcPduId)
{
    uint8 sdu[8] = { 0 };
    PduInfoType info = { sdu, 1U };
    PduR_ComRxIndication(srcPduId, &info);
}

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* SetUp(): PduR_Init(&kTestConfig) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * PduR に DeInit() は無いため TearDown は書かない。未初期化状態が必要なテストは各自 PduR_Test_ResetInitState() を呼ぶ。 */
class PduR_EnableRouting_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        gOverrideCalls = 0U;
        gRxCalls       = 0U;
        gConfCalls     = 0U;
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        PduR_Init(&kTestConfig);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(PduR_EnableRouting_Test, PduR_EnableRouting_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PduR_Test_ResetInitState();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    PduR_EnableRouting(0U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, PDUR_E_UNINIT);
}

TEST_F(PduR_EnableRouting_Test, PduR_EnableRouting_NG_UnknownGroupId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PduR_DisableRouting(0U, FALSE);   // グループ 0 を無効にしておく

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    PduR_EnableRouting(3U);           // 存在しないグループ（グループは 0〜2）

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, PDUR_E_ROUTING_PATH_GROUP_ID_INVALID);   // [SWS_PduR_00648]
    Receive(40U);
    EXPECT_EQ(gRxCalls, 0U);          // 何もしない（無効にしたグループ 0 は、無効のまま）[SWS_PduR_00647]
}

TEST_F(PduR_EnableRouting_Test, PduR_EnableRouting_OK_EnablesDisabledGroupImmediately)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PduR_DisableRouting(1U, FALSE);
    ASSERT_EQ(Transmit(11U), E_NOT_OK);   // グループ 1 の TX は、無効の間、受け付けない

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    PduR_EnableRouting(1U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(Transmit(11U), E_OK);       // すぐに有効（[SWS_PduR_00715]）
    EXPECT_EQ(gOverrideCalls, 1U);
    Receive(41U);
    EXPECT_EQ(gRxCalls, 1U);              // RX も、すぐに有効
}

TEST_F(PduR_EnableRouting_Test, PduR_EnableRouting_OK_OtherGroupsStayDisabled)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PduR_DisableRouting(0U, FALSE);
    PduR_DisableRouting(1U, FALSE);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    PduR_EnableRouting(1U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    Receive(40U);
    EXPECT_EQ(gRxCalls, 0U);              // グループ 0 は、無効のまま
    Receive(41U);
    EXPECT_EQ(gRxCalls, 1U);              // グループ 1 だけが、有効に戻る
}

}  // namespace
