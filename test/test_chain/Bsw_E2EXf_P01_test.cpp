/**
 * \file    Bsw_E2EXf_P01_test.cpp
 * \brief   E2EXf_InverseTransform()（E2E Profile 01 版）の単体テスト
 *          （GoogleTest / PlatformIO `[env:native_chain]`）。
 *
 * \details `E2EXf_RxConfigType`（Profile 01）は、EngineInfo/AbsInfo が
 *          いずれも Profile 05 へ移行済みのため本プロジェクトに実際の
 *          呼び出し元が無い参考実装である（E2EXf_PBCfg.c 冒頭コメント参照）。
 *          そのため本ファイルは、Com/Rte を経由せず `E2EXf_InverseTransform()`
 *          をローカルに組み立てた設定・状態で直接呼び、Dem への合否報告
 *          （`Bsw_Dem_fake.h` で記録）を検証する。
 *
 *          本テストの主目的は、2026-09 に `E2EXf_InverseTransform()` の
 *          合否判定を `E2E_P01MapStatusToSM()`（[SWS_E2E_00476]、
 *          profileBehavior=FALSE）経由に変更した際の回帰検知である。
 *          変更前は E2E_P01STATUS_SYNC（WRONGSEQUENCE 検知後の再ロック中）も
 *          合格（PASSED）扱いだったが、変更後は不合格（FAILED）になる
 *          （再ロック機構本来の目的「回復確認まで安易に正常扱いしない」との
 *          整合を優先した設計判断。E2EXf.h の宣言側コメント参照）。
 */
#include <gtest/gtest.h>

extern "C" {
#include "E2EXf.h"
#include "Bsw_Dem_fake.h"
#include "Hal_Det_Hw_fake.h"
}

namespace
{

/** AbsInfo(CAN 0x110)相当の設定。CRCOffset=0/CounterOffset=1、DLC=5。 */
const E2E_P01ConfigType kConfig = {
    0x0110U,  /* DataID */
    5U,       /* DataLength */
    1U,       /* MaxDeltaCounter */
    1U,       /* CounterOffset */
    0U,       /* CRCOffset */
    2U        /* SyncCounterInit */
};

class Bsw_E2EXf_P01_Test : public ::testing::Test
{
protected:
    E2E_P01ProtectStateType protectState;
    E2E_P01CheckStateType   checkState;
    E2EXf_RxConfigType      rxConfig;

    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;
        FakeDem_Reset();
        E2E_P01ProtectInit(&protectState);
        E2E_P01CheckInit(&checkState);
        E2EXf_Init(NULL);

        rxConfig.E2EConfig  = &kConfig;
        rxConfig.CheckState = &checkState;
        rxConfig.DemEventId = DEM_EVENT_E2E_ABSINFO;  // 適当な既存イベントを流用

        FakeDetHw_LogSuppressed = 0U;
    }

    void TearDown() override
    {
        FakeDetHw_LogSuppressed = 1U;
        E2EXf_DeInit();
    }

    /** 現在の protectState で1フレーム分の CRC/Counter を付与する。 */
    void BuildFrame(uint8 (&buf)[5])
    {
        buf[2] = 0x01U;
        buf[3] = 0x02U;
        buf[4] = 0x03U;
        E2E_P01Protect(&kConfig, &protectState, buf);
    }
};

TEST_F(Bsw_E2EXf_P01_Test, InverseTransform_OK_FirstFrameInitialIsAcceptedAsPassed)
{
    uint8 buf[5] = { 0U };
    BuildFrame(buf);
    E2E_P01StatusType checkStatus;

    Std_ReturnType ret = E2EXf_InverseTransform(&rxConfig, buf, sizeof(buf), &checkStatus);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(checkStatus, E2E_P01STATUS_INITIAL);
    EXPECT_EQ(FakeDem_LastEventStatus, DEM_EVENT_STATUS_PASSED);
}

TEST_F(Bsw_E2EXf_P01_Test, InverseTransform_OK_SecondConsecutiveFrameIsAcceptedAsPassed)
{
    uint8 buf1[5] = { 0U };
    uint8 buf2[5] = { 0U };
    BuildFrame(buf1);
    BuildFrame(buf2);
    E2E_P01StatusType checkStatus;

    E2EXf_InverseTransform(&rxConfig, buf1, sizeof(buf1), &checkStatus);  // INITIAL
    Std_ReturnType ret = E2EXf_InverseTransform(&rxConfig, buf2, sizeof(buf2), &checkStatus);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(checkStatus, E2E_P01STATUS_OK);
    EXPECT_EQ(FakeDem_LastEventStatus, DEM_EVENT_STATUS_PASSED);
}

TEST_F(Bsw_E2EXf_P01_Test, InverseTransform_NG_CorruptedCrcIsRejectedAsFailed)
{
    uint8 buf[5] = { 0U };
    BuildFrame(buf);
    buf[0] ^= 0xFFU;  // CRC8バイトを破壊
    E2E_P01StatusType checkStatus;

    Std_ReturnType ret = E2EXf_InverseTransform(&rxConfig, buf, sizeof(buf), &checkStatus);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(checkStatus, E2E_P01STATUS_WRONGCRC);
    EXPECT_EQ(FakeDem_LastEventStatus, DEM_EVENT_STATUS_FAILED);
}

TEST_F(Bsw_E2EXf_P01_Test, InverseTransform_NG_ResyncingAfterWrongSequenceIsNowRejectedAsFailed)
{
    /* 回帰テスト（2026-09 の合否判定変更の主目的）。WRONGSEQUENCE を検知させた
     * 後、CRC/Counter 自体は正常な次フレーム（Status=SYNC、再ロック未完了）を
     * 送る。変更前は PASSED 扱いだったが、変更後は FAILED になることを確認する。 */
    uint8 frame0[5] = { 0U };
    BuildFrame(frame0);
    E2E_P01StatusType checkStatus;
    E2EXf_InverseTransform(&rxConfig, frame0, sizeof(frame0), &checkStatus);  // INITIAL、基準値確立

    protectState.Counter = 3U;  // 基準値からdelta=3 > MaxDeltaCounter(1)
    uint8 frameJump[5] = { 0U };
    BuildFrame(frameJump);
    Std_ReturnType retJump = E2EXf_InverseTransform(&rxConfig, frameJump, sizeof(frameJump), &checkStatus);
    ASSERT_EQ(checkStatus, E2E_P01STATUS_WRONGSEQUENCE);
    EXPECT_EQ(retJump, E_NOT_OK);
    EXPECT_EQ(FakeDem_LastEventStatus, DEM_EVENT_STATUS_FAILED);

    uint8 frameResync[5] = { 0U };
    BuildFrame(frameResync);
    Std_ReturnType retResync = E2EXf_InverseTransform(&rxConfig, frameResync, sizeof(frameResync), &checkStatus);
    ASSERT_EQ(checkStatus, E2E_P01STATUS_SYNC);  // 再ロック中（SyncCounterInit=2回分）

    EXPECT_EQ(retResync, E_NOT_OK);  // 変更点: 以前は E_OK だった
    EXPECT_EQ(FakeDem_LastEventStatus, DEM_EVENT_STATUS_FAILED);  // 変更点: 以前は PASSED だった
}

}  // namespace
