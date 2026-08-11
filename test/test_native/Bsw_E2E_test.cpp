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

    const E2E_P05StatusType status = E2E_P05Check(&config, &checkState, data, sizeof(data));

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
    const E2E_P05StatusType status = E2E_P05Check(&config, &checkState, frame2, sizeof(frame2));

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
    const E2E_P05StatusType status = E2E_P05Check(&config, &checkState, frame3, sizeof(frame3));

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

    EXPECT_EQ(E2E_P05Check(&config, &checkState, frame1, sizeof(frame1)), E2E_P05STATUS_OK);
    EXPECT_EQ(E2E_P05Check(&config, &checkState, frame2, sizeof(frame2)), E2E_P05STATUS_OK);
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

    const E2E_P05StatusType status = E2E_P05Check(&config, &checkState, data, sizeof(data));

    EXPECT_EQ(status, E2E_P05STATUS_ERROR);
    EXPECT_EQ(checkState.Counter, 5U); /* CRC不一致時は状態を更新しない */
}

}  // namespace
