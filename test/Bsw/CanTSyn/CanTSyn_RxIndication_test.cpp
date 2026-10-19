/**
 * \file    CanTSyn_RxIndication_test.cpp
 * \brief   CanTSyn_RxIndication() の単体テスト（時間スレーブ側）
 * \details SYNC（秒）とそれに続く FUP（OVS・ナノ秒）を受けると、全域時間 = T0 + T4 + T3diff
 *          （受信までの経過時間）を求め、StbM の時間ベース 1 へ反映する（[SWS_CanTSyn_00073]）。
 *          次のものは無視して、時間ベースを更新しない: 時間領域の違うフレーム、DLC の足りないフレーム、
 *          CRC の合わないフレーム、SYNC なしの FUP、SC が対の SYNC と違う FUP、待ち時間を超えた FUP、
 *          SC の増分が 0 または上限を超える SYNC。PDU ID が不正なら CANTSYN_E_INVALID_PDUID、
 *          データのポインタが NULL なら CANTSYN_E_NULL_POINTER を報告する。
 *          フレームは、テスト内の別実装の CRC-8H2F で組み立てる。
 */

/* ======================================================================
 * Includes
 * ====================================================================== */

#include <gtest/gtest.h>

extern "C" {
#include "CanTSyn.h"
#include "CanTSyn_Cfg.h"
#include "StbM.h"
#include "Fake_Millis.h"
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

/** CRC-8H2F（多項式 0x2F、初期値 0xFF、最終 XOR 0xFF）の、テスト用の別実装。 */
uint8 ReferenceCrc8H2F(const uint8* data, uint32 length)
{
    uint8 crc = 0xFFU;
    for (uint32 i = 0U; i < length; i++)
    {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++)
        {
            crc = (crc & 0x80U) ? static_cast<uint8>((crc << 1) ^ 0x2FU) : static_cast<uint8>(crc << 1);
        }
    }
    return static_cast<uint8>(crc ^ 0xFFU);
}

/** 受信フレーム（SYNC または FUP）を組み立てる。 */
struct Frame
{
    uint8 data[8];
};

/**
 * Type・SC・時間領域・Byte3・時刻の値（SYNC は秒、FUP はナノ秒）から、CRC 付きのフレームを作る。
 * CRC は Byte2〜Byte7 に DataID（受信の CAN ID 0x241 から SYNC=(0x241+SC)、FUP=(0x241+0x10+SC)）を続けて計算する。
 */
Frame MakeFrame(bool isFup, uint8 sc, uint8 domain, uint8 byte3, uint32 value, bool badCrc = false)
{
    Frame f = {};
    f.data[0] = (CANTSYN_RX_CRC_VALIDATED != 0U) ? (isFup ? 0x28U : 0x20U) : (isFup ? 0x18U : 0x10U);
    f.data[2] = static_cast<uint8>((domain << 4) | (sc & 0x0FU));
    f.data[3] = byte3;
    f.data[4] = static_cast<uint8>(value >> 24);
    f.data[5] = static_cast<uint8>(value >> 16);
    f.data[6] = static_cast<uint8>(value >> 8);
    f.data[7] = static_cast<uint8>(value);

    if (CANTSYN_RX_CRC_VALIDATED != 0U)
    {
        uint8 input[7];
        for (int i = 0; i < 6; i++)
        {
            input[i] = f.data[2 + i];
        }
        input[6] = static_cast<uint8>((0x241U + (isFup ? 0x10U : 0U) + (sc & 0x0FU)) & 0xFFU);
        f.data[1] = ReferenceCrc8H2F(input, 7U);
        if (badCrc)
        {
            f.data[1] = static_cast<uint8>(f.data[1] ^ 0x5AU);
        }
    }
    return f;
}

/* ======================================================================
 * Test Fixture
 * ====================================================================== */

/* CanTSyn_RxIndication() の単体テスト用フィクスチャ。
 * SetUp(): 模擬時刻を 12345ms にして StbM_Init(NULL)・CanTSyn_Init(NULL) を呼び、DET の記録を初期化する。
 * Receive(): フレームを CanTSyn_RxIndication() へ渡す。ReadSlave(): 時間ベース 1 の時刻と状態を読む。 */
class CanTSyn_RxIndication_Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FakeMillis_Reset();
        FakeMillis_Value = 12345UL;
        FakeDetHw_Reset();
        FakeDetHw_LogSuppressed = 1U;
        StbM_Init(NULL);
        CanTSyn_Init(NULL);
        FakeDetHw_Reset();
        FakeDetHw_LogSuppressed = 0U;
    }

    static void Receive(const Frame& f)
    {
        PduInfoType info = { const_cast<uint8*>(f.data), 8U };
        CanTSyn_RxIndication(CANTSYN_RX_PDU_ID, &info);
    }

    static StbM_TimeStampType ReadSlave()
    {
        StbM_TimeStampType ts = {};
        StbM_UserDataType ud = {};
        EXPECT_EQ(StbM_GetCurrentTime(STBM_TIME_BASE_SLAVE, &ts, &ud), E_OK);
        return ts;
    }

    /** SYNC（秒 sec）を受け、elapsedMs 後に FUP（OVS ovs、ナノ秒 ns）を受ける。 */
    static void ReceivePair(uint8 sc, uint32 sec, uint32 elapsedMs, uint32 ns, uint8 ovs = 0U, uint8 byte3Extra = 0U)
    {
        Receive(MakeFrame(false, sc, CANTSYN_RX_TIME_DOMAIN_ID, 0U, sec));
        FakeMillis_Value += elapsedMs;
        Receive(MakeFrame(true, sc, CANTSYN_RX_TIME_DOMAIN_ID, static_cast<uint8>((ovs & 0x03U) | byte3Extra), ns));
    }
};

/* ======================================================================
 * Test Functions
 * ====================================================================== */

TEST_F(CanTSyn_RxIndication_Test, CanTSyn_RxIndication_NG_InvalidPduId)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Frame f = MakeFrame(false, 0U, CANTSYN_RX_TIME_DOMAIN_ID, 0U, 100U);
    PduInfoType info = { f.data, 8U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTSyn_RxIndication(CANTSYN_RX_PDU_ID + 1U, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastModuleId, CANTSYN_MODULE_ID);
    EXPECT_EQ(FakeDetHw_LastApiId, CANTSYN_API_ID_RX_INDICATION);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANTSYN_E_INVALID_PDUID);
}

TEST_F(CanTSyn_RxIndication_Test, CanTSyn_RxIndication_NG_NullPduInfo)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // なし（SetUp() で初期化済み）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTSyn_RxIndication(CANTSYN_RX_PDU_ID, NULL);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastModuleId, CANTSYN_MODULE_ID);
    EXPECT_EQ(FakeDetHw_LastApiId, CANTSYN_API_ID_RX_INDICATION);
    EXPECT_EQ(FakeDetHw_LastErrorId, CANTSYN_E_NULL_POINTER);
}

TEST_F(CanTSyn_RxIndication_Test, CanTSyn_RxIndication_NG_NullSduData)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    PduInfoType info = { NULL, 8U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTSyn_RxIndication(CANTSYN_RX_PDU_ID, &info);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(FakeDetHw_LastErrorId, CANTSYN_E_NULL_POINTER);
}

TEST_F(CanTSyn_RxIndication_Test, CanTSyn_RxIndication_NG_ShortDlcIgnored)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Frame sync = MakeFrame(false, 0U, CANTSYN_RX_TIME_DOMAIN_ID, 0U, 100U);
    Frame fup  = MakeFrame(true,  0U, CANTSYN_RX_TIME_DOMAIN_ID, 0U, 0U);
    PduInfoType shortInfo = { sync.data, 7U };

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    CanTSyn_RxIndication(CANTSYN_RX_PDU_ID, &shortInfo);   // DLC 7 の SYNC は無視される
    Receive(fup);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ReadSlave().timeBaseStatus, 0U);   // スレーブはまだ同期していない
}

TEST_F(CanTSyn_RxIndication_Test, CanTSyn_RxIndication_NG_WrongTimeDomainIgnored)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Frame sync = MakeFrame(false, 0U, CANTSYN_RX_TIME_DOMAIN_ID + 1U, 0U, 100U);
    Frame fup  = MakeFrame(true,  0U, CANTSYN_RX_TIME_DOMAIN_ID + 1U, 0U, 0U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Receive(sync);
    Receive(fup);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ReadSlave().timeBaseStatus, 0U);
}

TEST_F(CanTSyn_RxIndication_Test, CanTSyn_RxIndication_NG_FupWithoutSyncIgnored)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Frame fup = MakeFrame(true, 0U, CANTSYN_RX_TIME_DOMAIN_ID, 0U, 250000000U);

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Receive(fup);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ReadSlave().timeBaseStatus, 0U);
}

TEST_F(CanTSyn_RxIndication_Test, CanTSyn_RxIndication_NG_FupScMismatchIgnored)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Receive(MakeFrame(false, 3U, CANTSYN_RX_TIME_DOMAIN_ID, 0U, 100U));

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Receive(MakeFrame(true, 4U, CANTSYN_RX_TIME_DOMAIN_ID, 0U, 250000000U));   // SC が違う FUP

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ReadSlave().timeBaseStatus, 0U);
}

TEST_F(CanTSyn_RxIndication_Test, CanTSyn_RxIndication_NG_FupAfterFollowUpTimeoutIgnored)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Receive(MakeFrame(false, 0U, CANTSYN_RX_TIME_DOMAIN_ID, 0U, 100U));
    FakeMillis_Value += CANTSYN_RX_FOLLOW_UP_TIMEOUT_MS;   // 待ち時間ちょうどで FUP が届く

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Receive(MakeFrame(true, 0U, CANTSYN_RX_TIME_DOMAIN_ID, 0U, 250000000U));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ReadSlave().timeBaseStatus, 0U);
}

TEST_F(CanTSyn_RxIndication_Test, CanTSyn_RxIndication_NG_FupUsedUpBySyncIsNotReusable)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ReceivePair(0U, 100U, 20U, 0U);                    // 1 組が受け付けられる
    ASSERT_NE(ReadSlave().timeBaseStatus & STBM_TIMEBASE_STATUS_GLOBAL_TIME_BASE, 0U);
    FakeMillis_Value += 1000UL;
    StbM_TimeStampType before = ReadSlave();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Receive(MakeFrame(true, 0U, CANTSYN_RX_TIME_DOMAIN_ID, 0U, 900000000U));   // 対の SYNC がもう無い FUP

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    StbM_TimeStampType after = ReadSlave();
    EXPECT_EQ(after.seconds, before.seconds);          // 時刻は更新されず、連続して進んだまま
    EXPECT_EQ(after.nanoseconds, before.nanoseconds);
}

#if (CANTSYN_RX_CRC_VALIDATED != 0U)
TEST_F(CanTSyn_RxIndication_Test, CanTSyn_RxIndication_NG_SyncWithBadCrcIgnored)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Receive(MakeFrame(false, 0U, CANTSYN_RX_TIME_DOMAIN_ID, 0U, 100U, true));   // CRC の合わない SYNC

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Receive(MakeFrame(true, 0U, CANTSYN_RX_TIME_DOMAIN_ID, 0U, 250000000U));

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ReadSlave().timeBaseStatus, 0U);   // SYNC が捨てられたので、FUP も使われない
}

TEST_F(CanTSyn_RxIndication_Test, CanTSyn_RxIndication_NG_FupWithBadCrcIgnored)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Receive(MakeFrame(false, 0U, CANTSYN_RX_TIME_DOMAIN_ID, 0U, 100U));

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Receive(MakeFrame(true, 0U, CANTSYN_RX_TIME_DOMAIN_ID, 0U, 250000000U, true));   // CRC の合わない FUP

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ReadSlave().timeBaseStatus, 0U);
}

TEST_F(CanTSyn_RxIndication_Test, CanTSyn_RxIndication_NG_CrcLessTypeRejectedWhenCrcValidated)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    Frame sync = MakeFrame(false, 0U, CANTSYN_RX_TIME_DOMAIN_ID, 0U, 100U);
    Frame fup  = MakeFrame(true,  0U, CANTSYN_RX_TIME_DOMAIN_ID, 0U, 250000000U);
    sync.data[0] = 0x10U;   // CRC なしの Type（CRC_VALIDATED では受け付けない、[SWS_CanTSyn_00057]）
    fup.data[0]  = 0x18U;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    Receive(sync);
    Receive(fup);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ReadSlave().timeBaseStatus, 0U);
}
#endif

TEST_F(CanTSyn_RxIndication_Test, CanTSyn_RxIndication_NG_ScJumpTooLargeIgnored)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ReceivePair(0U, 100U, 20U, 0U);   // SC=0 が受け付けられる
    FakeMillis_Value += 1000UL;
    StbM_TimeStampType before = ReadSlave();

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ReceivePair(5U, 500U, 20U, 0U);   // SC が 0 → 5（増分 5 > 上限）の SYNC/FUP は無視される

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    StbM_TimeStampType after = ReadSlave();
    EXPECT_LT(after.seconds, 200U);   // 秒 500 に更新されていない（20ms 進んだだけ）
    EXPECT_GE(after.seconds, before.seconds);
}

TEST_F(CanTSyn_RxIndication_Test, CanTSyn_RxIndication_NG_ScNotIncrementedIgnored)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ReceivePair(2U, 100U, 20U, 0U);
    FakeMillis_Value += 1000UL;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ReceivePair(2U, 500U, 20U, 0U);   // 同じ SC の SYNC（増分 0）は無視される

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_LT(ReadSlave().seconds, 200U);   // 秒 500 に更新されていない
}

TEST_F(CanTSyn_RxIndication_Test, CanTSyn_RxIndication_OK_ValidPairSetsSlaveTime)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // SYNC: T0 = 100 秒。20ms 後に FUP: T4 = 250ms。全域時間 = 100 + 0.25 + 0.02 = 100.27 秒（FUP の受信時点）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ReceivePair(0U, 100U, 20U, 250000000U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    StbM_TimeStampType ts = ReadSlave();
    EXPECT_EQ(ts.seconds, 100U);
    EXPECT_EQ(ts.nanoseconds, 270000000U);
    EXPECT_NE(ts.timeBaseStatus & STBM_TIMEBASE_STATUS_GLOBAL_TIME_BASE, 0U);
    EXPECT_EQ(ts.timeBaseStatus & STBM_TIMEBASE_STATUS_SYNC_TO_GATEWAY, 0U);
    EXPECT_EQ(FakeDetHw_ReportCount, 0U);

    // 時刻が進めば、全域時間も同じだけ進む
    FakeMillis_Value += 730UL;
    ts = ReadSlave();
    EXPECT_EQ(ts.seconds, 101U);
    EXPECT_EQ(ts.nanoseconds, 0U);
}

TEST_F(CanTSyn_RxIndication_Test, CanTSyn_RxIndication_OK_OvsAddsSeconds)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // T0 = 100 秒、OVS = 2（FUP のナノ秒が 1 秒を超えた分）、ナノ秒 = 50ms → T4 = 2.05 秒

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ReceivePair(0U, 100U, 10U, 50000000U, 2U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    StbM_TimeStampType ts = ReadSlave();
    EXPECT_EQ(ts.seconds, 102U);              // 100 + 2 + 0.05 + 0.01 = 102.06 秒
    EXPECT_EQ(ts.nanoseconds, 60000000U);
}

TEST_F(CanTSyn_RxIndication_Test, CanTSyn_RxIndication_OK_SgwBitSetsSyncToGateway)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    // FUP の Byte3 の Bit2（SGW）= 1（SyncToSubDomain）

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ReceivePair(0U, 100U, 10U, 0U, 0U, 0x04U);

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_NE(ReadSlave().timeBaseStatus & STBM_TIMEBASE_STATUS_SYNC_TO_GATEWAY, 0U);
}

TEST_F(CanTSyn_RxIndication_Test, CanTSyn_RxIndication_OK_ConsecutiveScAcceptedAcrossWrap)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ReceivePair(15U, 100U, 10U, 0U);
    FakeMillis_Value += 1000UL;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ReceivePair(0U, 300U, 10U, 0U);   // SC は 15 → 0（増分 1）

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    StbM_TimeStampType ts = ReadSlave();
    EXPECT_EQ(ts.seconds, 300U);      // 300 + 0.01 秒に更新された
    EXPECT_EQ(ts.nanoseconds, 10000000U);
}

TEST_F(CanTSyn_RxIndication_Test, CanTSyn_RxIndication_OK_RecoversAfterIgnoredJump)
{
    /* ----------------------- */
    /* ---- 準備 (Arrange) --- */
    /* ----------------------- */
    ReceivePair(0U, 100U, 10U, 0U);
    FakeMillis_Value += 1000UL;
    ReceivePair(5U, 500U, 10U, 0U);   // SC が 5 に飛んだ SYNC は無視されるが、直前の SC としては記録される
    FakeMillis_Value += 1000UL;

    /* ----------------------- */
    /* ---- 実行 (Act) ------- */
    /* ----------------------- */
    ReceivePair(6U, 700U, 10U, 0U);   // 5 → 6（増分 1）なので受け付けられる

    /* ----------------------- */
    /* ---- 評価 (Assert) ---- */
    /* ----------------------- */
    EXPECT_EQ(ReadSlave().seconds, 700U);
}

}  // namespace
