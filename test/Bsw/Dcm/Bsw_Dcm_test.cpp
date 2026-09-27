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
#include <gtest/gtest.h>

extern "C" {
#include "Dcm.h"
#include "Dcm_Cbk.h"
#include "Dcm_Cfg.h"
#include "Fake_Det_Hw.h"
}

namespace
{

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

// ------------------------------------------------------------
// Dcm_GetVersionInfo()（他BSWモジュール共通の慣例により未初期化チェック対象外）
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_Test, Dcm_GetVersionInfo_NG_NullPointer)
{
    Dcm_GetVersionInfo(NULL);

    EXPECT_EQ(FakeDetHw_LastErrorId, DCM_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// Dcm_GetVin()（Dcm_Init() 自体が起動時に呼び出す関数のため、実装は
// 未初期化チェックを持たず NULL ポインタチェックのみ行う）
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_Test, Dcm_GetVin_NG_NullPointer)
{
    Std_ReturnType ret = Dcm_GetVin(NULL);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DCM_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// Dcm_GetSecurityLevel()
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_Test, Dcm_GetSecurityLevel_NG_Uninit)
{
    Dcm_Test_ResetInitState();
    Dcm_SecLevelType level;

    Std_ReturnType ret = Dcm_GetSecurityLevel(&level);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DCM_E_UNINIT);
}

TEST_F(Bsw_Dcm_Test, Dcm_GetSecurityLevel_NG_NullPointer)
{
    Std_ReturnType ret = Dcm_GetSecurityLevel(NULL);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DCM_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// Dcm_GetSesCtrlType()
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_Test, Dcm_GetSesCtrlType_NG_Uninit)
{
    Dcm_Test_ResetInitState();
    Dcm_SesCtrlType sesCtrlType;

    Std_ReturnType ret = Dcm_GetSesCtrlType(&sesCtrlType);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DCM_E_UNINIT);
}

TEST_F(Bsw_Dcm_Test, Dcm_GetSesCtrlType_NG_NullPointer)
{
    Std_ReturnType ret = Dcm_GetSesCtrlType(NULL);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DCM_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// Dcm_GetActiveProtocol()
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_Test, Dcm_GetActiveProtocol_NG_Uninit)
{
    Dcm_Test_ResetInitState();
    Dcm_ProtocolType protocol;
    uint16 connectionId;
    uint16 testerAddr;

    Std_ReturnType ret = Dcm_GetActiveProtocol(&protocol, &connectionId, &testerAddr);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DCM_E_UNINIT);
}

TEST_F(Bsw_Dcm_Test, Dcm_GetActiveProtocol_NG_NullPointer)
{
    uint16 connectionId;
    uint16 testerAddr;

    Std_ReturnType ret = Dcm_GetActiveProtocol(NULL, &connectionId, &testerAddr);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DCM_E_PARAM_POINTER);
}

// ------------------------------------------------------------
// Dcm_ResetToDefaultSession()
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_Test, Dcm_ResetToDefaultSession_NG_Uninit)
{
    Dcm_Test_ResetInitState();

    Std_ReturnType ret = Dcm_ResetToDefaultSession();

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(FakeDetHw_LastErrorId, DCM_E_UNINIT);
}

// ------------------------------------------------------------
// Dcm_MainFunction()
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_Test, Dcm_MainFunction_NG_Uninit)
{
    Dcm_Test_ResetInitState();

    Dcm_MainFunction();

    EXPECT_EQ(FakeDetHw_LastErrorId, DCM_E_UNINIT);
}

// ------------------------------------------------------------
// Dcm_ComIndication()
// ------------------------------------------------------------

TEST_F(Bsw_Dcm_Test, Dcm_ComIndication_NG_Uninit)
{
    Dcm_Test_ResetInitState();
    uint8 sdu[1] = { 0x22U };
    PduInfoType pdu = { sdu, 1U };

    Dcm_ComIndication(0U, &pdu);

    EXPECT_EQ(FakeDetHw_LastErrorId, DCM_E_UNINIT);
}

TEST_F(Bsw_Dcm_Test, Dcm_ComIndication_NG_NullPointer)
{
    Dcm_ComIndication(0U, NULL);

    EXPECT_EQ(FakeDetHw_LastErrorId, DCM_E_PARAM_POINTER);
}

}  // namespace
