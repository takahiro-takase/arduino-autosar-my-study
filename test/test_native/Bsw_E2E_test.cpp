/**
 * \file    Bsw_E2E_test.cpp
 * \brief   E2E_P05.c（src/Bsw/E2E/E2E_P05.c）の単体テスト（GoogleTest / PlatformIO native環境）
 * \details E2E_P05.c は実 HW 依存の無い自己完結したロジックのため、フェイク
 *          実装は不要で公開 API (E2E_P05Protect/E2E_P05Check とその Init) を
 *          直接呼んで検証する。E2E_CalcCrc16() 等の内部ヘルパーは static の
 *          ため、CRC の正しさは「本テストファイル内に SWS_E2E_00406 の
 *          擬似コードを独立に書き起こした参照実装」との突き合わせで検証する
 *          (E2E_P05.c 側の実装をコピーするのではなく、仕様書の記述から
 *          改めて書き起こすことで、実装のコピペミスを検出できるようにする)。
 *
 *          E2E_P05Check() はこのプロジェクト内に実行時の呼び出し元が無い
 *          (EngineHealthStatus は TX のみ) ため、正しさを検証できるのは
 *          このホストテストだけである。
 *
 *          E2E_P01.c のテストを追加する場合も、モジュール概念上は E2E
 *          ディレクトリで1つのため本ファイルに TEST_F(E2EP01Test, ...) を
 *          追加する形でまとめる（別ファイルに分けない）。
 *
 *          GoogleTest の main() は test_main.cpp に集約しているため、
 *          本ファイルでは定義しない。
 */
#include <gtest/gtest.h>

extern "C" {
#include "E2E.h"
#include "E2E_P01.h"
#include "E2E_P05.h"
}

namespace
{

/**
 * \brief  SWS_E2E_00406 の擬似コードを本テストファイル内で独立に書き起こした
 *         参照 CRC16 計算 (多項式 0x1021、開始値 0xFFFF、MSB first、非反転)。
 *         E2E_P05.c の実装が仕様書通りかを突き合わせるためのものなので、
 *         E2E_P05.c のコードをコピーせずゼロから書く。
 */
uint16_t ReferenceCrc16(const uint8_t *data, uint8_t len, uint16_t crc)
{
    for (uint8_t i = 0; i < len; i++)
    {
        crc = static_cast<uint16_t>(crc ^ (static_cast<uint16_t>(data[i]) << 8));
        for (uint8_t bit = 0; bit < 8; bit++)
        {
            if (crc & 0x8000U)
                crc = static_cast<uint16_t>((crc << 1) ^ 0x1021U);
            else
                crc = static_cast<uint16_t>(crc << 1);
        }
    }
    return crc;
}

/** 5byte PDU (CRC16 2B + Counter 1B + ユーザーデータ 2B) に対する参照CRCを計算する。 */
uint16_t ReferenceCrcForFrame(const uint8_t *data, uint8_t dataLength, uint8_t offset, uint16_t dataId)
{
    uint16_t crc = 0xFFFFU;
    crc = ReferenceCrc16(&data[offset + 2U], static_cast<uint8_t>(dataLength - offset - 2U), crc);
    const uint8_t idLow  = static_cast<uint8_t>(dataId & 0xFFU);
    const uint8_t idHigh = static_cast<uint8_t>((dataId >> 8U) & 0xFFU);
    crc = ReferenceCrc16(&idLow,  1U, crc);
    crc = ReferenceCrc16(&idHigh, 1U, crc);
    return crc;
}

class E2EP05Test : public ::testing::Test
{
protected:
    E2E_P05ConfigType config;

    void SetUp() override
    {
        config.DataID          = 0x0220U;
        config.DataLength       = 5U; /* CRC16(2B) + Counter(1B) + userdata(2B) */
        config.MaxDeltaCounter  = 1U;
        config.Offset           = 0U;
    }
};

TEST_F(E2EP05Test, ProtectComputesCrcMatchingReferenceImplementation)
{
    E2E_P05ProtectStateType state;
    E2E_P05ProtectInit(&state);

    uint8_t data[5] = {0U, 0U, 0U, 0x12U, 0x34U}; /* byte[3-4] = ユーザーデータ */
    E2E_P05Protect(&config, &state, data, sizeof(data));

    const uint16_t receivedCrc  = static_cast<uint16_t>(data[0]) | static_cast<uint16_t>(data[1] << 8);
    const uint16_t expectedCrc  = ReferenceCrcForFrame(data, config.DataLength, config.Offset, config.DataID);

    EXPECT_EQ(receivedCrc, expectedCrc);
    EXPECT_EQ(data[2], 0U); /* 1回目の Counter は 0 */
}

TEST_F(E2EP05Test, ProtectIncrementsCounterAndWrapsAt0xFF)
{
    E2E_P05ProtectStateType state;
    E2E_P05ProtectInit(&state);
    state.Counter = 0xFFU; /* 折り返し直前まで進めておく */

    uint8_t data[5] = {0U, 0U, 0U, 0U, 0U};
    E2E_P05Protect(&config, &state, data, sizeof(data));

    EXPECT_EQ(data[2], 0xFFU);   /* 送信されたCounterは折り返し前の0xFF */
    EXPECT_EQ(state.Counter, 0U); /* 次回用の内部Counterは0に折り返す */
}

TEST_F(E2EP05Test, FirstCheckAfterInitIsRepeatedBecauseBothStartAtCounterZero)
{
    /* Profile05にはProfile01のようなINITIAL状態が無く、初回のCheck呼び出しも
     * 通常のdelta計算にそのまま乗る。ProtectStateもCheckStateも初期値は
     * Counter=0なので、1回目のフレームは delta=0 (REPEATED) と判定される
     * ことを確認する (E2E_P05.h ファイル冒頭コメントに明記した仕様通りの挙動)。*/
    E2E_P05ProtectStateType protectState;
    E2E_P05CheckStateType   checkState;
    E2E_P05ProtectInit(&protectState);
    E2E_P05CheckInit(&checkState);

    uint8_t data[5] = {0U, 0U, 0U, 0x01U, 0x02U};
    E2E_P05Protect(&config, &protectState, data, sizeof(data));

    ASSERT_EQ(E2E_P05Check(&config, &checkState, data, sizeof(data)), E2E_E_OK);
    const E2E_P05StatusType status = checkState.Status;

    EXPECT_EQ(status, E2E_P05STATUS_REPEATED);
    EXPECT_EQ(checkState.Counter, 0U);
}

TEST_F(E2EP05Test, SecondConsecutiveFrameIsOk)
{
    E2E_P05ProtectStateType protectState;
    E2E_P05CheckStateType   checkState;
    E2E_P05ProtectInit(&protectState);
    E2E_P05CheckInit(&checkState);

    uint8_t frame1[5] = {0U, 0U, 0U, 0x01U, 0x02U};
    uint8_t frame2[5] = {0U, 0U, 0U, 0x01U, 0x02U};
    E2E_P05Protect(&config, &protectState, frame1, sizeof(frame1));
    E2E_P05Protect(&config, &protectState, frame2, sizeof(frame2));

    E2E_P05Check(&config, &checkState, frame1, sizeof(frame1)); /* 1回目: REPEATED */
    ASSERT_EQ(E2E_P05Check(&config, &checkState, frame2, sizeof(frame2)), E2E_E_OK);
    const E2E_P05StatusType status = checkState.Status;

    EXPECT_EQ(status, E2E_P05STATUS_OK);
    EXPECT_EQ(checkState.Counter, 1U);
}

TEST_F(E2EP05Test, CounterJumpBeyondMaxDeltaIsWrongSequence)
{
    E2E_P05ProtectStateType protectState;
    E2E_P05CheckStateType   checkState;
    E2E_P05ProtectInit(&protectState);
    E2E_P05CheckInit(&checkState);

    uint8_t frame1[5] = {0U, 0U, 0U, 0U, 0U};
    uint8_t frame2[5] = {0U, 0U, 0U, 0U, 0U};
    uint8_t frame3[5] = {0U, 0U, 0U, 0U, 0U};
    E2E_P05Protect(&config, &protectState, frame1, sizeof(frame1)); /* Counter=0 */
    E2E_P05Protect(&config, &protectState, frame2, sizeof(frame2)); /* Counter=1 */
    E2E_P05Protect(&config, &protectState, frame3, sizeof(frame3)); /* Counter=2 */

    E2E_P05Check(&config, &checkState, frame1, sizeof(frame1)); /* REPEATED、checkState.Counter=0 */
    /* frame2 を飛ばして frame3 (Counter=2) を Check する → delta=2 > MaxDeltaCounter(1) */
    ASSERT_EQ(E2E_P05Check(&config, &checkState, frame3, sizeof(frame3)), E2E_E_OK);
    const E2E_P05StatusType status = checkState.Status;

    EXPECT_EQ(status, E2E_P05STATUS_WRONGSEQUENCE);
    EXPECT_EQ(checkState.Counter, 2U); /* WRONGSEQUENCEでも状態は受信値へ更新される (CRC正常なため) */
}

TEST_F(E2EP05Test, CounterWrapsFrom0xFFTo0IsRecognizedAsOk)
{
    E2E_P05ProtectStateType protectState;
    E2E_P05CheckStateType   checkState;
    protectState.Counter = 0xFFU;
    checkState.Counter   = 0xFEU; /* 直前に受け付けた値が0xFEだったと仮定 */
    checkState.Status    = E2E_P05STATUS_OK;

    uint8_t frame1[5] = {0U, 0U, 0U, 0U, 0U}; /* Counter=0xFF (delta=1 from 0xFE) */
    uint8_t frame2[5] = {0U, 0U, 0U, 0U, 0U}; /* Counter=0x00 (delta=1 from 0xFF、折り返し) */
    E2E_P05Protect(&config, &protectState, frame1, sizeof(frame1));
    E2E_P05Protect(&config, &protectState, frame2, sizeof(frame2));

    ASSERT_EQ(E2E_P05Check(&config, &checkState, frame1, sizeof(frame1)), E2E_E_OK);
    EXPECT_EQ(checkState.Status, E2E_P05STATUS_OK);
    ASSERT_EQ(E2E_P05Check(&config, &checkState, frame2, sizeof(frame2)), E2E_E_OK);
    EXPECT_EQ(checkState.Status, E2E_P05STATUS_OK);
    EXPECT_EQ(checkState.Counter, 0U);
}

TEST_F(E2EP05Test, CrcMismatchReturnsErrorAndDoesNotUpdateState)
{
    E2E_P05ProtectStateType protectState;
    E2E_P05CheckStateType   checkState;
    E2E_P05ProtectInit(&protectState);
    E2E_P05CheckInit(&checkState);
    checkState.Counter = 5U; /* 直前の正常な状態を模擬 */

    uint8_t data[5] = {0U, 0U, 6U, 0U, 0U}; /* Counter=6 だがCRCは未計算(0,0)のまま=不一致 */

    ASSERT_EQ(E2E_P05Check(&config, &checkState, data, sizeof(data)), E2E_E_OK);
    const E2E_P05StatusType status = checkState.Status;

    EXPECT_EQ(status, E2E_P05STATUS_ERROR);
    EXPECT_EQ(checkState.Counter, 5U); /* CRC不一致時は状態を更新しない */
}

/**
 * \brief  SWS_E2E_00083 の CRC8 SAE-J1850 (多項式 0x1D、開始値・最終XORとも
 *         0x00、MSB first) を本テストファイル内で独立に書き起こした参照実装。
 *         E2E_P01.c の実装をコピーせずゼロから書く（ReferenceCrc16 と同じ方針）。
 */
uint8_t ReferenceCrc8(const uint8_t *data, uint8_t len, uint8_t crc)
{
    for (uint8_t i = 0; i < len; i++)
    {
        crc = static_cast<uint8_t>(crc ^ data[i]);
        for (uint8_t bit = 0; bit < 8; bit++)
        {
            if (crc & 0x80U)
                crc = static_cast<uint8_t>((crc << 1) ^ 0x1DU);
            else
                crc = static_cast<uint8_t>(crc << 1);
        }
    }
    return crc;
}

/** CRC 計算範囲: DataID(下位→上位) → Data[0..CRCOffset-1] → Data[CRCOffset+1..DataLength-1]
 *  (CRC バイト自身を除く、SWS_E2E_00082 Figure 7-6 準拠)。 */
uint8_t ReferenceCrc8ForFrame(const uint8_t *data, uint8_t dataLength, uint8_t crcOffset, uint16_t dataId)
{
    uint8_t crc = 0x00U;
    const uint8_t idLow  = static_cast<uint8_t>(dataId & 0xFFU);
    const uint8_t idHigh = static_cast<uint8_t>((dataId >> 8U) & 0xFFU);
    crc = ReferenceCrc8(&idLow,  1U, crc);
    crc = ReferenceCrc8(&idHigh, 1U, crc);
    if (crcOffset > 0U)
        crc = ReferenceCrc8(data, crcOffset, crc);
    if (static_cast<uint8_t>(crcOffset + 1U) < dataLength)
        crc = ReferenceCrc8(&data[crcOffset + 1U], static_cast<uint8_t>(dataLength - crcOffset - 1U), crc);
    return crc;
}

class E2EP01Test : public ::testing::Test
{
protected:
    E2E_P01ConfigType config;

    void SetUp() override
    {
        /* AbsInfo (CAN 0x110, DLC=5) 相当: byte[0]=CRC8, byte[1]=Counter, byte[2-4]=データ */
        config.DataID          = 0x0110U;
        config.DataLength       = 5U;
        config.MaxDeltaCounter  = 1U;
        config.CounterOffset    = 1U;
        config.CRCOffset        = 0U;
        config.SyncCounterInit  = 2U;
    }
};

TEST_F(E2EP01Test, ProtectComputesCrcMatchingReferenceImplementationAndStartsAtCounterZero)
{
    E2E_P01ProtectStateType state;
    ASSERT_EQ(E2E_P01ProtectInit(&state), E2E_E_OK);

    uint8_t data[5] = {0U, 0U, 0x12U, 0x34U, 0x56U};
    ASSERT_EQ(E2E_P01Protect(&config, &state, data), E2E_E_OK);

    const uint8_t expectedCrc = ReferenceCrc8ForFrame(data, config.DataLength, config.CRCOffset, config.DataID);
    EXPECT_EQ(data[0], expectedCrc);
    EXPECT_EQ(data[1], 0U); /* 1回目の Counter は 0 */
}

TEST_F(E2EP01Test, ProtectIncrementsCounterAndWrapsAt14SkippingReservedValue15)
{
    E2E_P01ProtectStateType state;
    ASSERT_EQ(E2E_P01ProtectInit(&state), E2E_E_OK);
    state.Counter = 14U; /* 折り返し直前 (4bit、15は予約値のためスキップ) */

    uint8_t data[5] = {0U, 0U, 0U, 0U, 0U};
    ASSERT_EQ(E2E_P01Protect(&config, &state, data), E2E_E_OK);

    EXPECT_EQ(data[1], 14U);      /* 送信された Counter は折り返し前の 14 */
    EXPECT_EQ(state.Counter, 0U); /* 次回用の内部 Counter は 15 を飛ばして 0 */
}

TEST_F(E2EP01Test, FirstCheckAfterInitReturnsInitial)
{
    E2E_P01ProtectStateType protectState;
    E2E_P01CheckStateType   checkState;
    ASSERT_EQ(E2E_P01ProtectInit(&protectState), E2E_E_OK);
    ASSERT_EQ(E2E_P01CheckInit(&checkState), E2E_E_OK);

    uint8_t data[5] = {0U, 0U, 0x01U, 0x02U, 0x03U};
    ASSERT_EQ(E2E_P01Protect(&config, &protectState, data), E2E_E_OK);

    ASSERT_EQ(E2E_P01Check(&config, &checkState, data), E2E_E_OK);
    EXPECT_EQ(checkState.Status, E2E_P01STATUS_INITIAL);
    EXPECT_EQ(checkState.LastValidCounter, 0U);
}

TEST_F(E2EP01Test, SecondConsecutiveFrameIsOk)
{
    E2E_P01ProtectStateType protectState;
    E2E_P01CheckStateType   checkState;
    ASSERT_EQ(E2E_P01ProtectInit(&protectState), E2E_E_OK);
    ASSERT_EQ(E2E_P01CheckInit(&checkState), E2E_E_OK);

    uint8_t frame1[5] = {0U, 0U, 0U, 0U, 0U};
    uint8_t frame2[5] = {0U, 0U, 0U, 0U, 0U};
    ASSERT_EQ(E2E_P01Protect(&config, &protectState, frame1), E2E_E_OK);
    ASSERT_EQ(E2E_P01Protect(&config, &protectState, frame2), E2E_E_OK);

    ASSERT_EQ(E2E_P01Check(&config, &checkState, frame1), E2E_E_OK); /* INITIAL */
    ASSERT_EQ(E2E_P01Check(&config, &checkState, frame2), E2E_E_OK);

    EXPECT_EQ(checkState.Status, E2E_P01STATUS_OK);
    EXPECT_EQ(checkState.LastValidCounter, 1U);
}

TEST_F(E2EP01Test, CrcMismatchReturnsWrongCrcAndDoesNotUpdateCounter)
{
    E2E_P01CheckStateType checkState;
    ASSERT_EQ(E2E_P01CheckInit(&checkState), E2E_E_OK);
    checkState.WaitForFirstData = 0U;
    checkState.LastValidCounter = 3U;

    uint8_t data[5] = {0U, 4U, 0U, 0U, 0U}; /* Counter=4 だが CRC は未計算(0)のまま=不一致 */

    ASSERT_EQ(E2E_P01Check(&config, &checkState, data), E2E_E_OK);

    EXPECT_EQ(checkState.Status, E2E_P01STATUS_WRONGCRC);
    EXPECT_EQ(checkState.LastValidCounter, 3U); /* CRC不一致時は状態を更新しない */
}

TEST_F(E2EP01Test, CounterJumpBeyondMaxDeltaTriggersWrongSequenceThenSyncUntilRelocked)
{
    E2E_P01ProtectStateType protectState;
    E2E_P01CheckStateType   checkState;
    ASSERT_EQ(E2E_P01ProtectInit(&protectState), E2E_E_OK);
    ASSERT_EQ(E2E_P01CheckInit(&checkState), E2E_E_OK);

    uint8_t frame0[5] = {0U, 0U, 0U, 0U, 0U};
    ASSERT_EQ(E2E_P01Protect(&config, &protectState, frame0), E2E_E_OK); /* Counter=0 */
    ASSERT_EQ(E2E_P01Check(&config, &checkState, frame0), E2E_E_OK);     /* INITIAL、基準値=0 */

    protectState.Counter = 3U; /* frame0(基準0)からdelta=3 > MaxDeltaCounter(1) */
    uint8_t frameJump[5] = {0U, 0U, 0U, 0U, 0U};
    ASSERT_EQ(E2E_P01Protect(&config, &protectState, frameJump), E2E_E_OK);
    ASSERT_EQ(E2E_P01Check(&config, &checkState, frameJump), E2E_E_OK);

    EXPECT_EQ(checkState.Status, E2E_P01STATUS_WRONGSEQUENCE);
    EXPECT_EQ(checkState.LastValidCounter, 3U);
    EXPECT_EQ(checkState.SyncCounter, config.SyncCounterInit);

    /* 再ロック中 (SyncCounterInit=2回分) は CRC/Counter が正常でも SYNC を返す */
    for (uint8_t i = 0U; i < config.SyncCounterInit; i++)
    {
        uint8_t frame[5] = {0U, 0U, 0U, 0U, 0U};
        ASSERT_EQ(E2E_P01Protect(&config, &protectState, frame), E2E_E_OK);
        ASSERT_EQ(E2E_P01Check(&config, &checkState, frame), E2E_E_OK);
        EXPECT_EQ(checkState.Status, E2E_P01STATUS_SYNC);
    }

    /* 再ロック完了後は通常の OK に戻る */
    uint8_t frameRelocked[5] = {0U, 0U, 0U, 0U, 0U};
    ASSERT_EQ(E2E_P01Protect(&config, &protectState, frameRelocked), E2E_E_OK);
    ASSERT_EQ(E2E_P01Check(&config, &checkState, frameRelocked), E2E_E_OK);
    EXPECT_EQ(checkState.Status, E2E_P01STATUS_OK);
}

TEST_F(E2EP01Test, NullPointerReturnsInputErrNullWithoutTouchingState)
{
    E2E_P01CheckStateType   checkState;
    E2E_P01ProtectStateType protectState;
    ASSERT_EQ(E2E_P01CheckInit(&checkState), E2E_E_OK);
    ASSERT_EQ(E2E_P01ProtectInit(&protectState), E2E_E_OK);
    uint8_t data[5] = {0U, 0U, 0U, 0U, 0U};
    const E2E_P01CheckStateType   checkStateBefore   = checkState;
    const E2E_P01ProtectStateType protectStateBefore = protectState;

    EXPECT_EQ(E2E_P01CheckInit(nullptr), E2E_E_INPUTERR_NULL);
    EXPECT_EQ(E2E_P01ProtectInit(nullptr), E2E_E_INPUTERR_NULL);
    EXPECT_EQ(E2E_P01Check(nullptr, &checkState, data), E2E_E_INPUTERR_NULL);
    EXPECT_EQ(E2E_P01Check(&config, nullptr, data), E2E_E_INPUTERR_NULL);
    EXPECT_EQ(E2E_P01Check(&config, &checkState, nullptr), E2E_E_INPUTERR_NULL);
    EXPECT_EQ(E2E_P01Protect(nullptr, &protectState, data), E2E_E_INPUTERR_NULL);
    EXPECT_EQ(E2E_P01Protect(&config, nullptr, data), E2E_E_INPUTERR_NULL);
    EXPECT_EQ(E2E_P01Protect(&config, &protectState, nullptr), E2E_E_INPUTERR_NULL);

    /* NULL 引数エラー時は State を一切書き換えないこと（テスト名の裏付け） */
    EXPECT_EQ(checkState.LastValidCounter, checkStateBefore.LastValidCounter);
    EXPECT_EQ(checkState.Status, checkStateBefore.Status);
    EXPECT_EQ(checkState.WaitForFirstData, checkStateBefore.WaitForFirstData);
    EXPECT_EQ(checkState.SyncCounter, checkStateBefore.SyncCounter);
    EXPECT_EQ(protectState.Counter, protectStateBefore.Counter);
}

class E2ETest : public ::testing::Test
{
};

TEST_F(E2ETest, GetVersionInfoFillsExpectedModuleId)
{
    Std_VersionInfoType info;

    E2E_GetVersionInfo(&info);

    EXPECT_EQ(info.moduleID, E2E_MODULE_ID);
}

TEST_F(E2ETest, GetVersionInfoSilentlyIgnoresNullPointer)
{
    /* [SWS_E2E_00216]: ライブラリは DET/DEM を呼んではならないため、NULL
     * でもクラッシュせず何もせず戻ることだけを確認する（報告先が無い）。
     * EXPECT_NO_FATAL_FAILURE は使わない（GoogleTest 1.17.0+MinGW の
     * ThreadLocalRegistryImpl 絡みでハングする既知の組み合わせのため。
     * 単純に直接呼び、クラッシュしなければ後続の行に到達する事実だけで
     * 十分検証になる）。 */
    E2E_GetVersionInfo(nullptr);
}

}  // namespace
