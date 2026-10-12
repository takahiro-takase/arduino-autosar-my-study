/**
 * \file    Bsw_Dcm_test.cpp
 * \brief   Dcm.c（src/Bsw/Dcm/Dcm.c）の単体テスト（NG系）
 * \details Dcm.c 全公開APIの `Det_ReportError()` 呼び出し箇所（NG ケースのみ）を
 *          1ファイルにまとめる。各ケースで、報告される ErrorId が仕様どおり
 *          正しい値になっていることを `Fake_Det_Hw.h`（`FakeDetHw_LastErrorId`）
 *          で検証する。UDS サービスディスパッチ自体の検証（NRC 応答等）は
 *          `Bsw_Dcm_ReadDtcInfo_test.cpp`/`test/Bsw/DcmStack/` の責務であり、
 *          本ファイルは対象外。
 *
 *          Dcm には `Dcm_DeInit()` に相当する API が無く、`Dcm_Initialized` は
 *          native_chain_tests バイナリ全体で共有される static のため、
 *          `DCM_E_UNINIT` の検証には `Dcm_Test_ResetInitState()`
 *          （`DCM_UNIT_TEST` ビルドのみに存在するテスト専用関数、
 *          `Mcu_Test_ResetInitState()` と同じ設計）を使う。
 *
 *          検証対象はいずれも未初期化チェック／NULLチェックの早期 return の
 *          みのため（Dcm.c 参照）、`Suppressed_ComM_DcmDiagnostic` による
 *          ComM 隔離（`Bsw_Dcm_ReadDtcInfo_test.cpp` 冒頭コメント参照）は
 *          不要（ComM へ到達する前に reject される）。
 *
 *          GoogleTest の main() は test_main.cpp に集約しているため、
 *          本ファイルでは定義しない。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "Dcm.h"
#include "Dcm_Cbk.h"
#include "Dcm_Cfg.h"
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

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* Dcm（UDS の個別サービス以外の API）の単体テスト用フィクスチャ。
 * SetUp(): Dcm_Init(NULL) を呼ぶ（Init 中の DET ログは抑制し、Init 自体の記録は消して後続の検証から除く）。
 * TearDown(): 未初期化の状態へ戻す。 */
class Bsw_Dcm_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;  // Init() のログはノイズになるため抑制
        Dcm_Init(NULL);
        FakeDetHw_Reset();             // Init 自体の記録を後続の検証対象から除く
        FakeDetHw_LogSuppressed = 0U;  // ここから各 TEST_F の実行(Act)区間
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        Dcm_Test_ResetInitState();
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

// ------------------------------------------------------------
// Dcm_GetVersionInfo()（他BSWモジュール共通の慣例により未初期化チェック対象外）
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_Test, Dcm_GetVersionInfo_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dcm_GetVersionInfo(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, DCM_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// Dcm_GetVin()（Dcm_Init() 自体が起動時に呼び出す関数のため、実装は
// 未初期化チェックを持たず NULL ポインタチェックのみ行う）
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_Test, Dcm_GetVin_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Dcm_GetVin(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DCM_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// Dcm_GetSecurityLevel()
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_Test, Dcm_GetSecurityLevel_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dcm_Test_ResetInitState();
    Dcm_SecLevelType level;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Dcm_GetSecurityLevel(&level);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DCM_E_UNINIT);
}

TEST_F(Bsw_Dcm_Test, Dcm_GetSecurityLevel_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Dcm_GetSecurityLevel(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DCM_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// Dcm_GetSesCtrlType()
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_Test, Dcm_GetSesCtrlType_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dcm_Test_ResetInitState();
    Dcm_SesCtrlType sesCtrlType;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Dcm_GetSesCtrlType(&sesCtrlType);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DCM_E_UNINIT);
}

TEST_F(Bsw_Dcm_Test, Dcm_GetSesCtrlType_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Dcm_GetSesCtrlType(NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DCM_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// Dcm_GetActiveProtocol()
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_Test, Dcm_GetActiveProtocol_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dcm_Test_ResetInitState();
    Dcm_ProtocolType protocol;
    uint16 connectionId;
    uint16 testerAddr;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Dcm_GetActiveProtocol(&protocol, &connectionId, &testerAddr);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DCM_E_UNINIT);
}

TEST_F(Bsw_Dcm_Test, Dcm_GetActiveProtocol_NG_NullPointer)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint16 connectionId;
    uint16 testerAddr;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Dcm_GetActiveProtocol(NULL, &connectionId, &testerAddr);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DCM_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// Dcm_ResetToDefaultSession()
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_Test, Dcm_ResetToDefaultSession_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dcm_Test_ResetInitState();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Std_ReturnType ret = Dcm_ResetToDefaultSession();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DCM_E_UNINIT);
}

// ------------------------------------------------------------
// Dcm_MainFunction()
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_Test, Dcm_MainFunction_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dcm_Test_ResetInitState();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dcm_MainFunction();

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, DCM_E_UNINIT);
}

// ------------------------------------------------------------
// Dcm_StartOfReception()
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_Test, Dcm_StartOfReception_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dcm_Test_ResetInitState();
    PduLengthType bufSize = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BufReq_ReturnType ret = Dcm_StartOfReception(0U, NULL, 1U, &bufSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, BUFREQ_E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DCM_E_UNINIT);
}

TEST_F(Bsw_Dcm_Test, Dcm_StartOfReception_NG_NullBufferSizePtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BufReq_ReturnType ret = Dcm_StartOfReception(0U, NULL, 1U, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, BUFREQ_E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DCM_E_PARAM_POINTER);
}

TEST_F(Bsw_Dcm_Test, Dcm_StartOfReception_NG_Overflow)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PduLengthType bufSize = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BufReq_ReturnType ret = Dcm_StartOfReception(0U, NULL, (PduLengthType)(DCM_RX_BUF_SIZE + 1U), &bufSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, BUFREQ_E_OVFL);
}

TEST_F(Bsw_Dcm_Test, Dcm_StartOfReception_OK_ZeroLengthQueriesBufferSize)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PduLengthType bufSize = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BufReq_ReturnType ret = Dcm_StartOfReception(0U, NULL, 0U, &bufSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, BUFREQ_OK);
    EXPECT_EQ(bufSize, (PduLengthType)DCM_RX_BUF_SIZE);
}

// ------------------------------------------------------------
// Dcm_CopyRxData()
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_Test, Dcm_CopyRxData_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dcm_Test_ResetInitState();
    uint8 sdu[1] = { 0x22U };
    PduInfoType info = { sdu, 1U };
    PduLengthType bufSize = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BufReq_ReturnType ret = Dcm_CopyRxData(0U, &info, &bufSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, BUFREQ_E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DCM_E_UNINIT);
}

TEST_F(Bsw_Dcm_Test, Dcm_CopyRxData_NG_NullInfoPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PduLengthType bufSize = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BufReq_ReturnType ret = Dcm_CopyRxData(0U, NULL, &bufSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, BUFREQ_E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DCM_E_PARAM_POINTER);
}

TEST_F(Bsw_Dcm_Test, Dcm_CopyRxData_NG_NotReceiving)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // Dcm_StartOfReception() を呼ばず、受信中ではない状態にしておく。
    uint8 sdu[1] = { 0x22U };
    PduInfoType info = { sdu, 1U };
    PduLengthType bufSize = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BufReq_ReturnType ret = Dcm_CopyRxData(0U, &info, &bufSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, BUFREQ_E_NOT_OK);
}

TEST_F(Bsw_Dcm_Test, Dcm_CopyRxData_NG_ExceedsAnnouncedLength)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PduLengthType bufSize = 0U;
    ASSERT_EQ(Dcm_StartOfReception(0U, NULL, 2U, &bufSize), BUFREQ_OK);
    uint8 sdu[3] = { 0x22U, 0x01U, 0x01U };
    PduInfoType info = { sdu, 3U };  // 通知した長さ（2）より多い

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BufReq_ReturnType ret = Dcm_CopyRxData(0U, &info, &bufSize);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, BUFREQ_E_NOT_OK);
}

// ------------------------------------------------------------
// Dcm_TpRxIndication()
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_Test, Dcm_TpRxIndication_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dcm_Test_ResetInitState();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dcm_TpRxIndication(0U, E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, DCM_E_UNINIT);
}

// ------------------------------------------------------------
// Dcm_CopyTxData()
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_Test, Dcm_CopyTxData_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dcm_Test_ResetInitState();
    uint8 buf[1] = { 0U };
    PduInfoType info = { buf, 1U };
    PduLengthType avail = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BufReq_ReturnType ret = Dcm_CopyTxData(0U, &info, NULL, &avail);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, BUFREQ_E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DCM_E_UNINIT);
}

TEST_F(Bsw_Dcm_Test, Dcm_CopyTxData_NG_NullAvailableDataPtr)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    uint8 buf[1] = { 0U };
    PduInfoType info = { buf, 1U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BufReq_ReturnType ret = Dcm_CopyTxData(0U, &info, NULL, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, BUFREQ_E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DCM_E_PARAM_POINTER);
}

TEST_F(Bsw_Dcm_Test, Dcm_CopyTxData_NG_NotTransmitting)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // 送信が進行中でない状態（応答を準備していない）にしておく。
    uint8 buf[1] = { 0U };
    PduInfoType info = { buf, 1U };
    PduLengthType avail = 0U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    BufReq_ReturnType ret = Dcm_CopyTxData(0U, &info, NULL, &avail);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ret, BUFREQ_E_NOT_OK);
}

// ------------------------------------------------------------
// Dcm_TpTxConfirmation()
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_Test, Dcm_TpTxConfirmation_NG_Uninit)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Dcm_Test_ResetInitState();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Dcm_TpTxConfirmation(0U, E_OK);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, DCM_E_UNINIT);
}

}  // namespace
