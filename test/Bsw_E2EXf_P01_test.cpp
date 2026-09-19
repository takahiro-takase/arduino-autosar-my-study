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
 *          2026-09 追記: `E2EXf_InverseTransform()` が `E2E_SMCheck()`
 *          （[SWS_E2EXf_00028]/[00029]）を呼ばずステートマシンを完全に
 *          バイパスしていたため、単発の CRC/カウンタ異常が即座に Dem
 *          FAILED へ直結してしまっていた不具合の是正に伴い、本ファイルを
 *          全面的に書き直した。「今回のフレームが使えるか」（戻り値・
 *          `*CheckStatus`）は以前と全く同じ判定のまま変わらないが、Dem への
 *          PASSED/FAILED 報告だけが `E2E_SMCheck()` のステートマシン判定
 *          （直近 `WindowSize`=3 回中の OK/ERROR 件数、`E2EXf_PBCfg.c` の
 *          `E2EXf_SMConfigDefault` と同じしきい値をテスト側にも設定して
 *          使う）に委ねられるようになった。`E2E_SM_NODATA`/`E2E_SM_INIT`
 *          （起動直後、判定材料が揃うまでの間）は Dem 報告そのものが保留
 *          される点に注意。
 *
 *          本テストのもう1つの主目的（2026-09 是正前からの既存項目）は、
 *          `E2EXf_InverseTransform()` の合否判定を `E2E_P01MapStatusToSM()`
 *          （[SWS_E2E_00476]、profileBehavior=FALSE）経由に変更した際の
 *          回帰検知である。E2E_P01STATUS_SYNC（WRONGSEQUENCE 検知後の
 *          再ロック中）は合格（E_OK）にはならない（再ロック機構本来の目的
 *          「回復確認まで安易に正常扱いしない」との整合を優先した設計判断。
 *          E2EXf.h の宣言側コメント参照）。
 */
#include <gtest/gtest.h>

extern "C" {
#include "E2EXf.h"
#include "E2E.h"
#include "Wrap_Dem.h"
#include "Fake_Hal_Det_Hw.h"
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

/** E2EXf_PBCfg.c の E2EXf_SMConfigDefault と同じしきい値（本ファイル冒頭
 *  コメント参照）。本番設定と乖離しないよう値を合わせている。 */
const E2E_SMConfigType kSMConfig = {
    3U, /* WindowSize           */
    2U, /* MinOkStateInit       */
    1U, /* MaxErrorStateInit    */
    2U, /* MinOkStateValid      */
    1U, /* MaxErrorStateValid   */
    2U, /* MinOkStateInvalid    */
    1U  /* MaxErrorStateInvalid */
};

class Bsw_E2EXf_P01_Test : public ::testing::Test
{
protected:
    E2E_P01ProtectStateType protectState;
    E2E_P01CheckStateType   checkState;
    uint8                   smWindow[3];
    E2E_SMCheckStateType    smState;
    E2EXf_RxConfigType      rxConfig;

    void SetUp() override
    {
        FakeDetHw_LogSuppressed = 1U;
        WrapDemSetEventStatus_Reset();
        Dem_Init(NULL);  // Demの内部状態を毎テスト決定的にリセットする（Fake_NvM.cにより常に「初回起動」）
        E2E_P01ProtectInit(&protectState);
        E2E_P01CheckInit(&checkState);
        smState.ProfileStatusWindow = smWindow;
        ASSERT_EQ(E2E_SMCheckInit(&smState, &kSMConfig), E2E_E_OK);
        E2EXf_Init(NULL);

        rxConfig.E2EConfig  = &kConfig;
        rxConfig.CheckState = &checkState;
        rxConfig.DemEventId = DEM_EVENT_E2E_ABSINFO;  // 適当な既存イベントを流用
        rxConfig.SMConfig   = &kSMConfig;
        rxConfig.SMState    = &smState;

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

// ------------------------------------------------------------
// SM_NODATA/SM_INIT の間（起動直後）は Dem 報告そのものを保留する。
// 「今回のフレームは使ってよい」（E_OK）という判定自体は変わらない。
// ------------------------------------------------------------
TEST_F(Bsw_E2EXf_P01_Test, InverseTransform_OK_FirstFrameInitialIsAcceptedButDemNotYetReported)
{
    uint8 buf[5] = { 0U };
    BuildFrame(buf);
    E2E_P01StatusType checkStatus;

    Std_ReturnType ret = E2EXf_InverseTransform(&rxConfig, buf, sizeof(buf), &checkStatus);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(checkStatus, E2E_P01STATUS_INITIAL);
    // NODATA -> INIT（1回目は AddStatus されない）。まだ VALID に確定して
    // いないため Dem 報告は保留される。
    EXPECT_EQ(smState.SMState, E2E_SM_INIT);
    EXPECT_EQ(WrapDemSetEventStatus_CallCount, 0U);
}

TEST_F(Bsw_E2EXf_P01_Test, InverseTransform_OK_SecondConsecutiveFrameIsAcceptedButDemNotYetReported)
{
    uint8 buf1[5] = { 0U };
    uint8 buf2[5] = { 0U };
    BuildFrame(buf1);
    BuildFrame(buf2);
    E2E_P01StatusType checkStatus;

    E2EXf_InverseTransform(&rxConfig, buf1, sizeof(buf1), &checkStatus);  // INITIAL、NODATA->INIT
    Std_ReturnType ret = E2EXf_InverseTransform(&rxConfig, buf2, sizeof(buf2), &checkStatus);

    EXPECT_EQ(ret, E_OK);
    EXPECT_EQ(checkStatus, E2E_P01STATUS_OK);
    // INIT で AddStatus 1回目: OkCount=1 < MinOkStateInit(2) のため VALID
    // にはまだ届かず INIT のまま。Dem 報告も引き続き保留。
    EXPECT_EQ(smState.SMState, E2E_SM_INIT);
    EXPECT_EQ(WrapDemSetEventStatus_CallCount, 0U);
}

TEST_F(Bsw_E2EXf_P01_Test, InverseTransform_OK_ThirdConsecutiveFrameReachesValidAndReportsPassed)
{
    uint8 buf1[5] = { 0U };
    uint8 buf2[5] = { 0U };
    uint8 buf3[5] = { 0U };
    BuildFrame(buf1);
    BuildFrame(buf2);
    BuildFrame(buf3);
    E2E_P01StatusType checkStatus;

    E2EXf_InverseTransform(&rxConfig, buf1, sizeof(buf1), &checkStatus);
    E2EXf_InverseTransform(&rxConfig, buf2, sizeof(buf2), &checkStatus);
    Std_ReturnType ret = E2EXf_InverseTransform(&rxConfig, buf3, sizeof(buf3), &checkStatus);

    EXPECT_EQ(ret, E_OK);
    // INIT で AddStatus 2回目: OkCount=2 >= MinOkStateInit(2) かつ
    // ErrorCount=0 <= MaxErrorStateInit(1) のため VALID へ昇格する。
    EXPECT_EQ(smState.SMState, E2E_SM_VALID);
    EXPECT_EQ(WrapDemSetEventStatus_CallCount, 1U);
    EXPECT_EQ(WrapDemSetEventStatus_LastEventStatus, DEM_EVENT_STATUS_PASSED);
}

// ------------------------------------------------------------
// CRC破壊: 起動直後（NODATA）にいきなり壊れたフレームが届いても、
// NODATA -> INIT の遷移条件（ProfileStatus != ERROR）を満たさないため
// NODATA のまま据え置かれ、Dem 報告も（まだ判定材料が無いため）保留される。
// 「今回のフレームは使えない」（E_NOT_OK）という判定自体は変わらない。
// ------------------------------------------------------------
TEST_F(Bsw_E2EXf_P01_Test, InverseTransform_NG_CorruptedCrcIsRejectedButDemNotYetReported)
{
    uint8 buf[5] = { 0U };
    BuildFrame(buf);
    buf[0] ^= 0xFFU;  // CRC8バイトを破壊
    E2E_P01StatusType checkStatus;

    Std_ReturnType ret = E2EXf_InverseTransform(&rxConfig, buf, sizeof(buf), &checkStatus);

    EXPECT_EQ(ret, E_NOT_OK);
    EXPECT_EQ(checkStatus, E2E_P01STATUS_WRONGCRC);
    EXPECT_EQ(smState.SMState, E2E_SM_NODATA);
    EXPECT_EQ(WrapDemSetEventStatus_CallCount, 0U);
}

// ------------------------------------------------------------
// 2026-09 是正の主目的: 一度 VALID に確定した後の単発の CRC 異常は
// （MaxErrorStateValid=1 の許容範囲内のため）通信路全体としては健全と
// 判定され、Dem は FAILED に倒れず PASSED のまま維持される。ただし
// 「このフレーム自体」は引き続き E_NOT_OK（使ってはいけない）のまま。
// ------------------------------------------------------------
TEST_F(Bsw_E2EXf_P01_Test, InverseTransform_OK_SingleGlitchAfterValidIsToleratedAndStaysPassed)
{
    uint8 buf1[5] = { 0U };
    uint8 buf2[5] = { 0U };
    uint8 buf3[5] = { 0U };
    uint8 buf4[5] = { 0U };
    BuildFrame(buf1);
    BuildFrame(buf2);
    BuildFrame(buf3);
    BuildFrame(buf4);
    buf4[0] ^= 0xFFU;  // 4フレーム目だけ CRC 破壊
    E2E_P01StatusType checkStatus;

    E2EXf_InverseTransform(&rxConfig, buf1, sizeof(buf1), &checkStatus);
    E2EXf_InverseTransform(&rxConfig, buf2, sizeof(buf2), &checkStatus);
    E2EXf_InverseTransform(&rxConfig, buf3, sizeof(buf3), &checkStatus);
    ASSERT_EQ(smState.SMState, E2E_SM_VALID);
    WrapDemSetEventStatus_Reset();  // ここまでの PASSED 報告をリセットし、以降だけを見る

    Std_ReturnType ret4 = E2EXf_InverseTransform(&rxConfig, buf4, sizeof(buf4), &checkStatus);

    EXPECT_EQ(ret4, E_NOT_OK);  // このフレーム自体は使えない
    EXPECT_EQ(checkStatus, E2E_P01STATUS_WRONGCRC);
    EXPECT_EQ(smState.SMState, E2E_SM_VALID);  // ErrorCount=1 <= 許容値のため維持
    EXPECT_EQ(WrapDemSetEventStatus_CallCount, 1U);
    EXPECT_EQ(WrapDemSetEventStatus_LastEventStatus, DEM_EVENT_STATUS_PASSED);  // FAILEDへ倒れない
}

TEST_F(Bsw_E2EXf_P01_Test, InverseTransform_NG_TwoConsecutiveGlitchesAfterValidReportFailed)
{
    uint8 buf1[5] = { 0U };
    uint8 buf2[5] = { 0U };
    uint8 buf3[5] = { 0U };
    uint8 buf4[5] = { 0U };
    uint8 buf5[5] = { 0U };
    BuildFrame(buf1);
    BuildFrame(buf2);
    BuildFrame(buf3);
    BuildFrame(buf4);
    BuildFrame(buf5);
    buf4[0] ^= 0xFFU;
    buf5[0] ^= 0xFFU;
    E2E_P01StatusType checkStatus;

    E2EXf_InverseTransform(&rxConfig, buf1, sizeof(buf1), &checkStatus);
    E2EXf_InverseTransform(&rxConfig, buf2, sizeof(buf2), &checkStatus);
    E2EXf_InverseTransform(&rxConfig, buf3, sizeof(buf3), &checkStatus);
    ASSERT_EQ(smState.SMState, E2E_SM_VALID);
    E2EXf_InverseTransform(&rxConfig, buf4, sizeof(buf4), &checkStatus);
    ASSERT_EQ(smState.SMState, E2E_SM_VALID);  // 1件目はまだ許容範囲内
    WrapDemSetEventStatus_Reset();

    E2EXf_InverseTransform(&rxConfig, buf5, sizeof(buf5), &checkStatus);

    // 直近3件中2件が ERROR (> MaxErrorStateValid=1) のため INVALID へ転落する。
    EXPECT_EQ(smState.SMState, E2E_SM_INVALID);
    EXPECT_EQ(WrapDemSetEventStatus_CallCount, 1U);
    EXPECT_EQ(WrapDemSetEventStatus_LastEventStatus, DEM_EVENT_STATUS_FAILED);
}

// ------------------------------------------------------------
// 回帰テスト: WRONGSEQUENCE 検知後の再ロック中（SYNC）は合格にならない
// （2026-09 是正前からの既存項目、ファイル冒頭コメント参照）。
// ------------------------------------------------------------
TEST_F(Bsw_E2EXf_P01_Test, InverseTransform_NG_ResyncingAfterWrongSequenceIsRejected)
{
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
    // WRONGSEQUENCE は OK でも ERROR でもないため OkCount/ErrorCount いずれも
    // 増えず、INIT のまま(昇格もINVALID転落もしない)。Dem報告は保留のまま。
    EXPECT_EQ(smState.SMState, E2E_SM_INIT);
    EXPECT_EQ(WrapDemSetEventStatus_CallCount, 0U);

    uint8 frameResync[5] = { 0U };
    BuildFrame(frameResync);
    Std_ReturnType retResync = E2EXf_InverseTransform(&rxConfig, frameResync, sizeof(frameResync), &checkStatus);
    ASSERT_EQ(checkStatus, E2E_P01STATUS_SYNC);  // 再ロック中（SyncCounterInit=2回分）

    EXPECT_EQ(retResync, E_NOT_OK);
    EXPECT_EQ(smState.SMState, E2E_SM_INIT);
    EXPECT_EQ(WrapDemSetEventStatus_CallCount, 0U);
}

}  // namespace
