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

TEST_F(E2EP05Test, MapStatusToSM_OK_MapsEachStatusPerSpecTable)
{
    /* [SWS_E2E_00453] */
    EXPECT_EQ(E2E_P05MapStatusToSM(E2E_E_OK, E2E_P05STATUS_OK), E2E_P_OK);
    EXPECT_EQ(E2E_P05MapStatusToSM(E2E_E_OK, E2E_P05STATUS_OKSOMELOST), E2E_P_OK);
    EXPECT_EQ(E2E_P05MapStatusToSM(E2E_E_OK, E2E_P05STATUS_ERROR), E2E_P_ERROR);
    EXPECT_EQ(E2E_P05MapStatusToSM(E2E_E_OK, E2E_P05STATUS_REPEATED), E2E_P_REPEATED);
    EXPECT_EQ(E2E_P05MapStatusToSM(E2E_E_OK, E2E_P05STATUS_NONEWDATA), E2E_P_NONEWDATA);
    EXPECT_EQ(E2E_P05MapStatusToSM(E2E_E_OK, E2E_P05STATUS_WRONGSEQUENCE), E2E_P_WRONGSEQUENCE);
}

TEST_F(E2EP05Test, MapStatusToSM_NG_NonOkCheckReturnAlwaysMapsToErrorRegardlessOfStatus)
{
    /* [SWS_E2E_00454] */
    EXPECT_EQ(E2E_P05MapStatusToSM(E2E_E_INPUTERR_NULL, E2E_P05STATUS_OK), E2E_P_ERROR);
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

TEST_F(E2EP01Test, MapStatusToSM_OK_R42BehaviorGroupsSyncWithOkAndInitialWithWrongSequence)
{
    /* [SWS_E2E_00383]: profileBehavior=1 (TRUE) */
    EXPECT_EQ(E2E_P01MapStatusToSM(E2E_E_OK, E2E_P01STATUS_OK, 1U), E2E_P_OK);
    EXPECT_EQ(E2E_P01MapStatusToSM(E2E_E_OK, E2E_P01STATUS_OKSOMELOST, 1U), E2E_P_OK);
    EXPECT_EQ(E2E_P01MapStatusToSM(E2E_E_OK, E2E_P01STATUS_SYNC, 1U), E2E_P_OK);
    EXPECT_EQ(E2E_P01MapStatusToSM(E2E_E_OK, E2E_P01STATUS_WRONGCRC, 1U), E2E_P_ERROR);
    EXPECT_EQ(E2E_P01MapStatusToSM(E2E_E_OK, E2E_P01STATUS_REPEATED, 1U), E2E_P_REPEATED);
    EXPECT_EQ(E2E_P01MapStatusToSM(E2E_E_OK, E2E_P01STATUS_NONEWDATA, 1U), E2E_P_NONEWDATA);
    EXPECT_EQ(E2E_P01MapStatusToSM(E2E_E_OK, E2E_P01STATUS_WRONGSEQUENCE, 1U), E2E_P_WRONGSEQUENCE);
    EXPECT_EQ(E2E_P01MapStatusToSM(E2E_E_OK, E2E_P01STATUS_INITIAL, 1U), E2E_P_WRONGSEQUENCE);
}

TEST_F(E2EP01Test, MapStatusToSM_OK_PreR42BehaviorGroupsInitialWithOkAndSyncWithWrongSequence)
{
    /* [SWS_E2E_00476]: profileBehavior=0 (FALSE)。TRUE側とちょうど
     * INITIAL/SYNCの帰属が入れ替わる点のみ異なる。 */
    EXPECT_EQ(E2E_P01MapStatusToSM(E2E_E_OK, E2E_P01STATUS_OK, 0U), E2E_P_OK);
    EXPECT_EQ(E2E_P01MapStatusToSM(E2E_E_OK, E2E_P01STATUS_OKSOMELOST, 0U), E2E_P_OK);
    EXPECT_EQ(E2E_P01MapStatusToSM(E2E_E_OK, E2E_P01STATUS_INITIAL, 0U), E2E_P_OK);
    EXPECT_EQ(E2E_P01MapStatusToSM(E2E_E_OK, E2E_P01STATUS_WRONGCRC, 0U), E2E_P_ERROR);
    EXPECT_EQ(E2E_P01MapStatusToSM(E2E_E_OK, E2E_P01STATUS_REPEATED, 0U), E2E_P_REPEATED);
    EXPECT_EQ(E2E_P01MapStatusToSM(E2E_E_OK, E2E_P01STATUS_NONEWDATA, 0U), E2E_P_NONEWDATA);
    EXPECT_EQ(E2E_P01MapStatusToSM(E2E_E_OK, E2E_P01STATUS_WRONGSEQUENCE, 0U), E2E_P_WRONGSEQUENCE);
    EXPECT_EQ(E2E_P01MapStatusToSM(E2E_E_OK, E2E_P01STATUS_SYNC, 0U), E2E_P_WRONGSEQUENCE);
}

TEST_F(E2EP01Test, MapStatusToSM_NG_NonOkCheckReturnAlwaysMapsToErrorRegardlessOfStatusOrBehavior)
{
    /* [SWS_E2E_00384] */
    EXPECT_EQ(E2E_P01MapStatusToSM(E2E_E_INPUTERR_NULL, E2E_P01STATUS_OK, 1U), E2E_P_ERROR);
    EXPECT_EQ(E2E_P01MapStatusToSM(E2E_E_INPUTERR_NULL, E2E_P01STATUS_OK, 0U), E2E_P_ERROR);
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

/**
 * \brief  E2E_SMCheck()/E2E_SMCheckInit() の単体テスト（[SWS_E2E_00340]/
 *         [SWS_E2E_00353]）。
 * \details 状態遷移の閾値は WindowSize=3、Init系=3件全OK/0件Errorで昇格、
 *          Valid系=1件OK/1件Errorまで許容、Invalid系=3件全OK/0件Errorで
 *          復帰、という小さく手計算しやすい値にしている。
 */
class E2ESMTest : public ::testing::Test
{
protected:
    uint8_t          window[3];
    E2E_SMConfigType config;
    E2E_SMCheckStateType state;

    void SetUp() override
    {
        config.WindowSize           = 3U;
        config.MinOkStateInit       = 3U;
        config.MaxErrorStateInit    = 0U;
        config.MinOkStateValid      = 1U;
        config.MaxErrorStateValid   = 1U;
        config.MinOkStateInvalid    = 3U;
        config.MaxErrorStateInvalid = 0U;

        state.ProfileStatusWindow = window;
    }
};

TEST_F(E2ESMTest, CheckInit_OK_SetsNodataStateAndClearsWindow)
{
    ASSERT_EQ(E2E_SMCheckInit(&state, &config), E2E_E_OK);

    EXPECT_EQ(state.SMState, E2E_SM_NODATA);
    EXPECT_EQ(state.WindowTopIndex, 0U);
    EXPECT_EQ(state.OkCount, 0U);
    EXPECT_EQ(state.ErrorCount, 0U);
    for (uint8_t i = 0U; i < config.WindowSize; i++)
        EXPECT_EQ(window[i], static_cast<uint8_t>(E2E_P_NOTAVAILABLE));
}

TEST_F(E2ESMTest, CheckInit_NG_NullPointerReturnsInputErrNull)
{
    EXPECT_EQ(E2E_SMCheckInit(nullptr, &config), E2E_E_INPUTERR_NULL);
    EXPECT_EQ(E2E_SMCheckInit(&state, nullptr), E2E_E_INPUTERR_NULL);
}

TEST_F(E2ESMTest, Check_NG_NullPointerReturnsInputErrNull)
{
    ASSERT_EQ(E2E_SMCheckInit(&state, &config), E2E_E_OK);

    EXPECT_EQ(E2E_SMCheck(E2E_P_OK, nullptr, &state), E2E_E_INPUTERR_NULL);
    EXPECT_EQ(E2E_SMCheck(E2E_P_OK, &config, nullptr), E2E_E_INPUTERR_NULL);
}

TEST_F(E2ESMTest, Check_NG_DeinitStateReturnsWrongStateWithoutChangingState)
{
    /* E2E_SMCheckInit() を一度も呼んでいない場合を模擬するため、State を
     * 明示的に E2E_SM_DEINIT にする。注意: [SWS_E2E_00343] の値定義は
     * E2E_SM_VALID=0x00・E2E_SM_DEINIT=0x01 のため、ゼロ初期化しただけでは
     * DEINIT にはならず（誤って VALID 扱いになってしまう）、呼び出し元は
     * 必ず E2E_SMCheckInit() を明示的に呼ぶ必要がある（本テストはその
     * 「明示的な初期化が必須」という前提を裏付けるためのもの）。 */
    state.SMState = E2E_SM_DEINIT;

    EXPECT_EQ(E2E_SMCheck(E2E_P_OK, &config, &state), E2E_E_WRONGSTATE);
    EXPECT_EQ(state.SMState, E2E_SM_DEINIT); /* 状態遷移しないこと */
}

TEST_F(E2ESMTest, Check_OK_NodataStaysUntilFirstGoodStatus)
{
    ASSERT_EQ(E2E_SMCheckInit(&state, &config), E2E_E_OK);

    ASSERT_EQ(E2E_SMCheck(E2E_P_ERROR, &config, &state), E2E_E_OK);
    EXPECT_EQ(state.SMState, E2E_SM_NODATA);

    ASSERT_EQ(E2E_SMCheck(E2E_P_NONEWDATA, &config, &state), E2E_E_OK);
    EXPECT_EQ(state.SMState, E2E_SM_NODATA);

    /* NODATA からの遷移では E2E_SMAddStatus() を呼ばない（PDFベクタ座標解析で
     * 確認済み、E2E_SMCheck() の Doxygen 参照）ため、ここまで OkCount/
     * ErrorCount は 0 のままのはず。 */
    EXPECT_EQ(state.OkCount, 0U);
    EXPECT_EQ(state.ErrorCount, 0U);

    ASSERT_EQ(E2E_SMCheck(E2E_P_OK, &config, &state), E2E_E_OK);
    EXPECT_EQ(state.SMState, E2E_SM_INIT);
    EXPECT_EQ(state.OkCount, 0U);    /* この遷移自体もウィンドウには追加しない */
    EXPECT_EQ(state.ErrorCount, 0U);
}

TEST_F(E2ESMTest, Check_OK_InitPromotesToValidOnceWindowFullOfOk)
{
    ASSERT_EQ(E2E_SMCheckInit(&state, &config), E2E_E_OK);
    ASSERT_EQ(E2E_SMCheck(E2E_P_OK, &config, &state), E2E_E_OK); /* NODATA -> INIT */
    ASSERT_EQ(state.SMState, E2E_SM_INIT);

    /* WindowSize(3) 回分 OK を積むまでは INIT に留まる */
    ASSERT_EQ(E2E_SMCheck(E2E_P_OK, &config, &state), E2E_E_OK);
    EXPECT_EQ(state.SMState, E2E_SM_INIT);
    ASSERT_EQ(E2E_SMCheck(E2E_P_OK, &config, &state), E2E_E_OK);
    EXPECT_EQ(state.SMState, E2E_SM_INIT);

    /* 3回目でウィンドウが全て OK になり VALID へ昇格 */
    ASSERT_EQ(E2E_SMCheck(E2E_P_OK, &config, &state), E2E_E_OK);
    EXPECT_EQ(state.SMState, E2E_SM_VALID);
    EXPECT_EQ(state.OkCount, 3U);
    EXPECT_EQ(state.ErrorCount, 0U);
}

TEST_F(E2ESMTest, Check_OK_InitDemotesDirectlyToInvalidWhenErrorExceedsMax)
{
    ASSERT_EQ(E2E_SMCheckInit(&state, &config), E2E_E_OK);
    ASSERT_EQ(E2E_SMCheck(E2E_P_OK, &config, &state), E2E_E_OK); /* NODATA -> INIT */

    /* MaxErrorStateInit=0 のため、INIT 中に ERROR が1回でも発生すると
     * (OK が十分溜まる前でも) 即座に INVALID へ落ちる。 */
    ASSERT_EQ(E2E_SMCheck(E2E_P_ERROR, &config, &state), E2E_E_OK);
    EXPECT_EQ(state.SMState, E2E_SM_INVALID);
}

TEST_F(E2ESMTest, Check_OK_ValidStaysValidWithinErrorLimitThenDemotesWhenExceeded)
{
    ASSERT_EQ(E2E_SMCheckInit(&state, &config), E2E_E_OK);
    ASSERT_EQ(E2E_SMCheck(E2E_P_OK, &config, &state), E2E_E_OK); /* NODATA -> INIT（AddStatus は呼ばれない） */
    ASSERT_EQ(E2E_SMCheck(E2E_P_OK, &config, &state), E2E_E_OK); /* window=[OK,NA,NA] */
    ASSERT_EQ(E2E_SMCheck(E2E_P_OK, &config, &state), E2E_E_OK); /* window=[OK,OK,NA] */
    ASSERT_EQ(E2E_SMCheck(E2E_P_OK, &config, &state), E2E_E_OK); /* window=[OK,OK,OK] -> VALID */
    ASSERT_EQ(state.SMState, E2E_SM_VALID);

    /* MaxErrorStateValid=1 のため、1件だけの ERROR は許容され VALID を維持する
     * (window=[ERROR,OK,OK] は OkCount=2>=MinOkStateValid(1)、ErrorCount=1<=1)。 */
    ASSERT_EQ(E2E_SMCheck(E2E_P_ERROR, &config, &state), E2E_E_OK);
    EXPECT_EQ(state.SMState, E2E_SM_VALID);

    /* 2件目の ERROR で許容量(1)を超え INVALID へ落ちる
     * (window=[ERROR,ERROR,OK] は ErrorCount=2>1)。 */
    ASSERT_EQ(E2E_SMCheck(E2E_P_ERROR, &config, &state), E2E_E_OK);
    EXPECT_EQ(state.SMState, E2E_SM_INVALID);
}

TEST_F(E2ESMTest, Check_OK_InvalidRecoversToValidOnceWindowFullOfOk)
{
    /* INIT を経由せず、INVALID から直接検証する（E2E_SMAddStatus() は
     * 呼び出し毎にウィンドウ全体を再走査して OkCount/ErrorCount を再計算する
     * ため、事前の Ok/ErrorCount の値そのものは結果に影響しない）。 */
    window[0] = static_cast<uint8_t>(E2E_P_ERROR);
    window[1] = static_cast<uint8_t>(E2E_P_ERROR);
    window[2] = static_cast<uint8_t>(E2E_P_ERROR);
    state.WindowTopIndex = 0U;
    state.SMState        = E2E_SM_INVALID;

    /* MinOkStateInvalid=3/MaxErrorStateInvalid=0 のため、ウィンドウ全体が
     * OK で埋まるまで INVALID に留まる。 */
    ASSERT_EQ(E2E_SMCheck(E2E_P_OK, &config, &state), E2E_E_OK);
    EXPECT_EQ(state.SMState, E2E_SM_INVALID);
    ASSERT_EQ(E2E_SMCheck(E2E_P_OK, &config, &state), E2E_E_OK);
    EXPECT_EQ(state.SMState, E2E_SM_INVALID);

    /* 3回目でウィンドウが全て OK になり VALID へ復帰 */
    ASSERT_EQ(E2E_SMCheck(E2E_P_OK, &config, &state), E2E_E_OK);
    EXPECT_EQ(state.SMState, E2E_SM_VALID);
    EXPECT_EQ(state.OkCount, 3U);
    EXPECT_EQ(state.ErrorCount, 0U);
}

}  // namespace
