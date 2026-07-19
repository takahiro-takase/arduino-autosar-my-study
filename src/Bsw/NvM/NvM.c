/**
 * \file    NvM.c
 * \brief   Non-Volatile Memory Manager 実装 (AUTOSAR SWS_NvM 準拠)
 * \details 実際の不揮発メモリへの読み書きは NvM_Hw 層に委譲し、
 *          本ファイルは MCU 固有のヘッダ (avr/eeprom.h 等) を直接知らない。
 *          上位モジュール (DEM など) も NvM の API 経由で EEPROM にアクセスする。
 *
 *          RAM ミラー設計:
 *            NvM_Init() が EEPROM → RAM ミラーを一括ロードする。
 *            NvM_ReadBlock() は RAM ミラーを参照するだけで EEPROM を読まない。
 *            NvM_WriteBlock() は src → RAM ミラーをコピーしたのち
 *            NvM_Hw_WriteBlock で差分バイトのみ EEPROM へ書く。
 *
 *          CRC によるデータ破損検出:
 *            各ブロックのデータ本体直後の 1 バイトに AUTOSAR Crc8
 *            (SAE J1850: 多項式 0x1D, 初期値 0xFF, 最終 XOR 0xFF) を保存する。
 *            NvM_Init() は読み込み直後に CRC を再計算して検証し、
 *            不一致ならビット化けや書き込み中の電源断による破損とみなして
 *            ROM デフォルト値（未設定なら全 0）で復元し EEPROM へ書き戻す
 *            (起動時、Os スケジューラ開始前のため同期処理のまま)。
 *            実際の AUTOSAR Crc モジュールへの委譲は行わず、本ファイル内に
 *            最小実装を持つ（学習用簡略化。Crc モジュール自体は本プロジェクトの
 *            対応範囲外）。
 *
 *          非同期書き込みジョブキュー:
 *            NvM_WriteBlock() / NvM_RestoreBlockDefaults() は RAM ミラーを
 *            即座に更新するが、EEPROM への実書き込みは「保留」とマークする
 *            だけで、その場ではブロックしない。NvM_MainFunction() が周期的に
 *            呼ばれるたびに、保留中のブロックのデータ本体または CRC のうち
 *            未書き込みの 1 バイトだけを書き込む。1 ブロック（最大 10 バイト、
 *            冗長ブロックはプライマリ→ミラーの順で 2 面分）を書き終えると
 *            次の保留ブロックへ移る。ブロックは同時に 1 個ずつ
 *            投入順 (FIFO) で順次処理する。呼び出し元（例: Dem）が複数ブロックを
 *            続けて書く際、電源断時の整合性のために書き込み順序そのものに
 *            意味を持たせている場合があるため（例: 有効性マーカーを最後に
 *            書くことで「途中経過」を無効データとして扱う設計）、ブロック ID の
 *            昇順ではなく投入順を維持しなければならない。
 *            処理中のブロックに対して新たに NvM_WriteBlock() が呼ばれた場合は
 *            書き込み位置（冗長ブロックはコピー選択も含め）を先頭へ巻き戻す。
 *            RAM ミラーは既に新しい値に上書きされているため、巻き戻さずに
 *            続行すると「古いバイトと新しいバイトが混在した」不整合な内容が
 *            EEPROM に残ってしまう（ちぎれ書き）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "NvM.h"
#include "NvM_Hw.h"
#include "Det.h"
#include <string.h>

#define TAG "NvM"

/** AUTOSAR Crc8 (SAE J1850) パラメータ */
#define NVM_CRC8_INITIAL  0xFFU
#define NVM_CRC8_POLY     0x1DU
#define NVM_CRC8_XOR_OUT  0xFFU

static const NvM_ConfigType* NvM_Cfg = NULL;

/** ブロックごとの「EEPROM への書き込みが保留中か」フラグ。
 *  NvM_WriteBlock()/NvM_RestoreBlockDefaults() が立て、
 *  NvM_MainFunction() がそのブロックを書き終えたら下ろす。 */
static uint8 NvM_BlockPending[NVM_BLOCK_COUNT];

/** ブロックごとの直近のジョブ結果 (NvM_GetErrorStatus() が返す値)。 */
static NvM_RequestResultType NvM_BlockResult[NVM_BLOCK_COUNT];

/** 現在 NvM_MainFunction() が処理中のブロック ID。
 *  NVM_BLOCK_COUNT ならどのブロックも処理中でないことを示す。 */
static uint8 NvM_ActiveBlockId = NVM_BLOCK_COUNT;

/** 処理中ブロックの次に書き込むバイトオフセット。
 *  [0, NvMNvBlockLength) はデータ本体、NvMNvBlockLength は CRC バイトを指す。 */
static uint16 NvM_ActiveByteIndex = 0U;

/** 冗長ブロック（Redundant=1）処理中、現在どちらのコピーを書き込んでいるか。
 *  0 = プライマリ面、1 = ミラー面。非冗長ブロックでは未使用（常に 0）。
 *  プライマリを完全に書き終えてからミラーへ移るため、書き込み途中で
 *  電源が落ちても、書き込み中でない側は必ず直前の完了済みの内容を保持する。 */
static uint8 NvM_ActiveCopyIsMirror = 0U;

/** 保留ブロック ID を投入順 (FIFO) で保持するリングバッファ。
 *  呼び出し元 (Dem 等) は「後から投入したブロックほど後で物理書き込みされる」
 *  ことを前提に書き込み順序（例: MAGIC バイトを最後に書く）を設計している。
 *  ブロック ID の昇順で拾ってしまうと投入順が保証されず、電源断時の
 *  整合性設計が崩れるため、投入順を明示的に記録する。
 *  同時に pending になり得るブロックは高々 NVM_BLOCK_COUNT 個
 *  (ID の重複投入はキューに積み直さない) なのでオーバーフローしない。 */
static uint8 NvM_PendingQueue[NVM_BLOCK_COUNT];
static uint8 NvM_QueueHead = 0U;  /**< 次に取り出すエントリの index */
static uint8 NvM_QueueTail = 0U;  /**< 次に積むエントリの index     */
static uint8 NvM_QueueLen  = 0U;  /**< キュー内の有効エントリ数     */

/* -----------------------------------------------------------------------
 * 内部ヘルパー
 * ----------------------------------------------------------------------- */

static const NvM_BlockDescriptorType* NvM_GetBlock(NvM_BlockIdType id)
{
    if (NvM_Cfg == NULL || id >= NvM_Cfg->NumBlocks)
        return NULL;
    return &NvM_Cfg->Blocks[id];
}

/**
 * \brief   AUTOSAR Crc8 (SAE J1850) アルゴリズムでブロックの CRC を計算する。
 *
 * \details 多項式 0x1D、初期値 0xFF、入力/出力の反転なし、最終 XOR 0xFF。
 *          ブロックは最大 8 バイトと小さいため、テーブルなしのビット単位
 *          計算で十分な速度が得られる。
 */
static uint8 NvM_CalcCrc8(const uint8* data, uint16 length)
{
    uint8 crc = NVM_CRC8_INITIAL;
    uint16 i;
    for (i = 0U; i < length; i++)
    {
        crc ^= data[i];
        uint8 bit;
        for (bit = 0U; bit < 8U; bit++)
        {
            crc = (uint8)(((crc & 0x80U) != 0U)
                           ? ((crc << 1U) ^ NVM_CRC8_POLY)
                           : (crc << 1U));
        }
    }
    return (uint8)(crc ^ NVM_CRC8_XOR_OUT);
}

/** 指定 EEPROM ベースアドレスに対する CRC 保存先 (データ本体直後の 1 バイト)。
 *  冗長ブロックはプライマリ／ミラーそれぞれのベースアドレスに対して呼ぶ。 */
static uint16 NvM_CrcAddressForBase(uint16 base, uint16 length)
{
    return (uint16)(base + length);
}

/**
 * \brief   RAM ミラーの内容を、指定した 1 つの EEPROM コピー（データ本体+CRC）へ
 *          同期的に書き込む。
 *
 * \details 冗長ブロックのプライマリ／ミラーいずれか片方だけを書く共通処理。
 *          呼び出し元が起動時（NvM_Init 経由）やデフォルト復元時など、
 *          同期的にブロッキングしてよい文脈でのみ使うこと。
 */
static void NvM_WriteCopySync(uint16 base, const void* data, uint16 length)
{
    NvM_Hw_WriteBlock(data, base, length);
    uint8 crc = NvM_CalcCrc8((const uint8*)data, length);
    NvM_Hw_WriteByte(NvM_CrcAddressForBase(base, length), crc);
}

/**
 * \brief   ブロックの RAM ミラーへデフォルト値を適用し、CRC とともに
 *          EEPROM へ同期的に書き戻す。
 *
 * \details NvM_Init() の CRC 不一致時にのみ使う。起動時（Os スケジューラ
 *          開始前）のため、ここで数バイト分ブロッキングしても他タスクを
 *          巻き込まず無害。実行中に呼ぶ NvM_RestoreBlockDefaults() は
 *          これとは別に、RAM ミラー更新のみ同期で行い EEPROM 書き込みは
 *          非同期ジョブキューへ回す（下記参照）。
 *          冗長ブロック（Redundant=1）ではプライマリ・ミラー両面へ
 *          同じデフォルト値を書く（どちらか一方だけが正しい値を持つ
 *          半端な状態を作らないため）。
 */
static void NvM_ApplyDefaultSync(NvM_BlockIdType id, const NvM_BlockDescriptorType* blk)
{
    if (blk->RomBlockDataAddress != NULL)
        memcpy(blk->RamBlockDataAddress, blk->RomBlockDataAddress, blk->NvMNvBlockLength);
    else
        memset(blk->RamBlockDataAddress, 0, blk->NvMNvBlockLength);

    NvM_WriteCopySync(blk->NvMNvBlockBaseNumber, blk->RamBlockDataAddress, blk->NvMNvBlockLength);
    if (blk->Redundant != 0U)
        NvM_WriteCopySync(blk->NvMNvBlockBaseNumberMirror, blk->RamBlockDataAddress, blk->NvMNvBlockLength);

    DET_LOGW(TAG, "block=%u defaults restored (%s)", (unsigned)id,
             (blk->RomBlockDataAddress != NULL) ? "ROM default" : "zero-fill");
}

/**
 * \brief   ブロックを EEPROM から読み込み、CRC 検証・（冗長ブロックなら）
 *          自己修復・デフォルト復元までを行う。NvM_Init() から呼ばれる。
 *
 * \details 非冗長ブロック（Redundant=0）: 単純に CRC を検証し、不一致なら
 *          NvM_ApplyDefaultSync() でデフォルト値へ復元する（従来の挙動）。
 *
 *          冗長ブロック（Redundant=1）: プライマリ・ミラー両面を読み、
 *          それぞれ CRC を検証する。
 *            - 両面とも正常 → プライマリの内容を採用（ミラーはそのまま）。
 *            - 片面のみ正常 → 正常な方の内容を RAM ミラーへ採用した上で、
 *              破損した方をその内容で上書きして自己修復する。
 *            - 両面とも破損 → 通常ブロックと同様、デフォルト値へ復元する
 *              （プライマリ・ミラー両面に書く）。
 *          これにより、書き込み中の電源断で片方のコピーが不完全な状態に
 *          なっても、もう片方（プライマリ→ミラーの順で完全に書き終えてから
 *          次のブロックへ移るため、書き込み中でない側は必ず直前の完了済みの
 *          内容を保持している）からデータを失わずに復旧できる。
 */
static void NvM_LoadAndVerifyBlock(NvM_BlockIdType id, const NvM_BlockDescriptorType* blk)
{
    if (blk->RamBlockDataAddress == NULL)
        return;

    NvM_Hw_ReadBlock(blk->RamBlockDataAddress, blk->NvMNvBlockBaseNumber, blk->NvMNvBlockLength);
    const uint8 storedCrcPrimary = NvM_Hw_ReadByte(
        NvM_CrcAddressForBase(blk->NvMNvBlockBaseNumber, blk->NvMNvBlockLength));
    const uint8 calcCrcPrimary = NvM_CalcCrc8((const uint8*)blk->RamBlockDataAddress, blk->NvMNvBlockLength);
    const uint8 primaryValid = (storedCrcPrimary == calcCrcPrimary) ? 1U : 0U;

    if (blk->Redundant == 0U)
    {
        if (!primaryValid)
        {
            DET_LOGE(TAG, "block=%u CRC mismatch (stored=0x%02X calc=0x%02X)",
                     (unsigned)id, (unsigned)storedCrcPrimary, (unsigned)calcCrcPrimary);
            NvM_ApplyDefaultSync(id, blk);
        }
        return;
    }

    /* 冗長ブロック: ミラー面をスクラッチバッファへ読み、CRC を検証する
     * （採用する側が決まるまで RAM ミラーへは反映しない）。 */
    uint8 mirrorBuf[NVM_MAX_BLOCK_LENGTH];
    NvM_Hw_ReadBlock(mirrorBuf, blk->NvMNvBlockBaseNumberMirror, blk->NvMNvBlockLength);
    const uint8 storedCrcMirror = NvM_Hw_ReadByte(
        NvM_CrcAddressForBase(blk->NvMNvBlockBaseNumberMirror, blk->NvMNvBlockLength));
    const uint8 calcCrcMirror = NvM_CalcCrc8(mirrorBuf, blk->NvMNvBlockLength);
    const uint8 mirrorValid = (storedCrcMirror == calcCrcMirror) ? 1U : 0U;

    if (primaryValid)
    {
        if (!mirrorValid)
        {
            DET_LOGW(TAG, "block=%u redundant: mirror CRC mismatch, repairing from primary", (unsigned)id);
            NvM_WriteCopySync(blk->NvMNvBlockBaseNumberMirror, blk->RamBlockDataAddress, blk->NvMNvBlockLength);
        }
        return;
    }

    if (mirrorValid)
    {
        DET_LOGW(TAG, "block=%u redundant: primary CRC mismatch, recovered from mirror", (unsigned)id);
        memcpy(blk->RamBlockDataAddress, mirrorBuf, blk->NvMNvBlockLength);
        NvM_WriteCopySync(blk->NvMNvBlockBaseNumber, blk->RamBlockDataAddress, blk->NvMNvBlockLength);
        return;
    }

    DET_LOGE(TAG, "block=%u redundant: both copies CRC mismatch, restoring defaults", (unsigned)id);
    NvM_ApplyDefaultSync(id, blk);
}

/**
 * \brief   ブロックの EEPROM 書き込みジョブを保留キューへ積む（RAM ミラー
 *          自体は呼び出し元が既に更新済みであること）。
 *
 * \details 対象ブロックを NvM_MainFunction() が今まさに処理中だった場合は、
 *          書き込み位置を先頭 (byte 0) へ巻き戻す。RAM ミラーは直前に
 *          最新値へ上書きされているため、巻き戻さずに続きから書くと
 *          古いバイトと新しいバイトが混在した不整合な内容が EEPROM に
 *          残ってしまう（ちぎれ書き）。冗長ブロックの場合はコピー選択
 *          （プライマリ／ミラー）も先頭（プライマリ）へ巻き戻す。
 *          まだ pending でないブロックのみ FIFO キューの末尾に積む
 *          （既に pending 中のブロックを再度積むと、投入順が崩れたり
 *          キューが枯渇前に重複エントリで溢れたりする）。
 */
static void NvM_MarkPending(NvM_BlockIdType id)
{
    if (NvM_BlockPending[id] == 0U)
    {
        NvM_PendingQueue[NvM_QueueTail] = id;
        NvM_QueueTail = (uint8)((NvM_QueueTail + 1U) % NVM_BLOCK_COUNT);
        NvM_QueueLen++;
    }

    NvM_BlockPending[id] = 1U;
    NvM_BlockResult[id]  = NVM_REQ_PENDING;

    if (NvM_ActiveBlockId == id)
    {
        NvM_ActiveByteIndex    = 0U;
        NvM_ActiveCopyIsMirror = 0U;
    }
}

/* -----------------------------------------------------------------------
 * 公開 API
 * ----------------------------------------------------------------------- */

/**
 * \brief   全ブロックの EEPROM 内容を RAM ミラーへ一括ロードする。
 *
 * \details NvM_Hw_ReadBlock() で EEPROM アドレス → RAM へバイト列をコピーする
 *          (引数順は dst_RAM, src_EEPROM, size)。
 *          読み込み直後に各ブロックの CRC を検証し、不一致ならデフォルト値
 *          (NvM_ApplyDefaultSync()) で復元する。
 *
 * \ServiceID      {0x06}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void NvM_Init(const NvM_ConfigType* ConfigPtr)
{
    NvM_Cfg = ConfigPtr;

    for (uint8 i = 0U; i < ConfigPtr->NumBlocks && i < NVM_BLOCK_COUNT; i++)
    {
        NvM_BlockPending[i] = 0U;
        NvM_BlockResult[i]  = NVM_REQ_OK;

        NvM_LoadAndVerifyBlock(i, &ConfigPtr->Blocks[i]);
    }

    NvM_ActiveBlockId      = NVM_BLOCK_COUNT;
    NvM_ActiveByteIndex    = 0U;
    NvM_ActiveCopyIsMirror = 0U;
    NvM_QueueHead = 0U;
    NvM_QueueTail = 0U;
    NvM_QueueLen  = 0U;

    DET_LOGI(TAG, "Init ok blocks=%u", (unsigned)ConfigPtr->NumBlocks);
}

/**
 * \brief   RAM ミラーの内容を NvM_DstPtr へコピーする。
 *
 * \details EEPROM アクセスは発生しない。NvM_Init() 後であれば RAM ミラーは
 *          常に最新の EEPROM 値を保持している。
 *
 * \ServiceID      {0x16}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType NvM_ReadBlock(NvM_BlockIdType BlockId, void* NvM_DstPtr)
{
    const NvM_BlockDescriptorType* blk = NvM_GetBlock(BlockId);
    if (blk == NULL || NvM_DstPtr == NULL || blk->RamBlockDataAddress == NULL)
        return E_NOT_OK;

    memcpy(NvM_DstPtr, blk->RamBlockDataAddress, blk->NvMNvBlockLength);
    return E_OK;
}

/**
 * \brief   NvM_SrcPtr を RAM ミラーへコピーし、EEPROM 書き込みジョブを
 *          保留キューへ積む。
 *
 * \details RAM ミラーの更新は同期的で即座に反映される。実際の EEPROM 書き込みは
 *          NvM_MainFunction() が非同期に 1 バイトずつ行うため、ここでは
 *          ブロックしない（詳細はファイル冒頭のコメント参照）。
 *
 * \ServiceID      {0x0D}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Asynchronous}
 */
Std_ReturnType NvM_WriteBlock(NvM_BlockIdType BlockId, const void* NvM_SrcPtr)
{
    const NvM_BlockDescriptorType* blk = NvM_GetBlock(BlockId);
    if (blk == NULL || NvM_SrcPtr == NULL || blk->RamBlockDataAddress == NULL)
        return E_NOT_OK;

    /* RAM ミラーを最新値で更新 (同期) */
    memcpy(blk->RamBlockDataAddress, NvM_SrcPtr, blk->NvMNvBlockLength);

    /* EEPROM への実書き込みは NvM_MainFunction() へ委譲 (非同期) */
    NvM_MarkPending(BlockId);

    return E_OK;
}

/**
 * \brief   指定ブロックを ROM デフォルト値（未設定なら全 0）へ復元する。
 *
 * \details RAM ミラーの更新は同期的に行い、EEPROM への書き戻しは
 *          NvM_WriteBlock() と同じ非同期ジョブキュー経由で行う。
 *
 * \ServiceID      {0x0F}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Asynchronous}
 */
Std_ReturnType NvM_RestoreBlockDefaults(NvM_BlockIdType BlockId)
{
    const NvM_BlockDescriptorType* blk = NvM_GetBlock(BlockId);
    if (blk == NULL || blk->RamBlockDataAddress == NULL)
        return E_NOT_OK;

    if (blk->RomBlockDataAddress != NULL)
        memcpy(blk->RamBlockDataAddress, blk->RomBlockDataAddress, blk->NvMNvBlockLength);
    else
        memset(blk->RamBlockDataAddress, 0, blk->NvMNvBlockLength);

    DET_LOGW(TAG, "block=%u defaults restored (%s), write queued", (unsigned)BlockId,
             (blk->RomBlockDataAddress != NULL) ? "ROM default" : "zero-fill");

    NvM_MarkPending(BlockId);

    return E_OK;
}

/**
 * \brief   ブロックの直近のジョブ結果を取得する。
 *
 * \ServiceID      {0x10}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
NvM_RequestResultType NvM_GetErrorStatus(NvM_BlockIdType BlockId)
{
    if (BlockId >= NVM_BLOCK_COUNT)
        return NVM_REQ_NOT_OK;
    return NvM_BlockResult[BlockId];
}

/**
 * \brief   NvM 周期処理。保留中の書き込みジョブを 1 バイトずつ処理する。
 *
 * \details 処理中のブロックが無ければ、投入順 (FIFO) キューの先頭ブロックを
 *          取り出して処理を開始する。1 回の呼び出しで書き込むのはデータ本体の
 *          1 バイト、またはブロック末尾に到達していれば CRC の 1 バイトのみ。
 *          CRC まで書き終えたらそのブロックの保留フラグを下ろし、
 *          ジョブ結果を NVM_REQ_OK にして次のブロックへ移る。
 *
 * \ServiceID      {0x11}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void NvM_MainFunction(void)
{
    if (NvM_Cfg == NULL)
        return;

    if (NvM_ActiveBlockId >= NVM_BLOCK_COUNT)
    {
        /* 処理中のブロックが無い: FIFO キューの先頭 (最も古く投入された
         * ブロック) を取り出す。ブロック ID 昇順で拾うと投入順が崩れ、
         * 呼び出し元が意図した書き込み順序（例: Dem が MAGIC バイトを
         * 最後に書くことで電源断時の整合性を担保している設計）を
         * 壊してしまうため、必ず投入順で処理する。 */
        if (NvM_QueueLen == 0U)
            return;  /* 保留ジョブなし */

        NvM_ActiveBlockId = NvM_PendingQueue[NvM_QueueHead];
        NvM_QueueHead = (uint8)((NvM_QueueHead + 1U) % NVM_BLOCK_COUNT);
        NvM_QueueLen--;
        NvM_ActiveByteIndex    = 0U;
        NvM_ActiveCopyIsMirror = 0U;  /* 冗長ブロックは必ずプライマリ面から書き始める */
    }

    const NvM_BlockDescriptorType* blk = &NvM_Cfg->Blocks[NvM_ActiveBlockId];
    /* 冗長ブロックでミラー面を処理中ならミラーのベースアドレスを、
     * それ以外（非冗長、または冗長のプライマリ面処理中）はプライマリの
     * ベースアドレスを使う。 */
    const uint16 activeBase = (blk->Redundant != 0U && NvM_ActiveCopyIsMirror != 0U)
                               ? blk->NvMNvBlockBaseNumberMirror
                               : blk->NvMNvBlockBaseNumber;

    if (NvM_ActiveByteIndex < blk->NvMNvBlockLength)
    {
        /* データ本体を 1 バイトだけ書く */
        const uint8* ram = (const uint8*)blk->RamBlockDataAddress;
        NvM_Hw_WriteByte((uint16)(activeBase + NvM_ActiveByteIndex),
                         ram[NvM_ActiveByteIndex]);
        NvM_ActiveByteIndex++;
    }
    else if (NvM_ActiveByteIndex == blk->NvMNvBlockLength)
    {
        /* データ本体を書き終えた: 最後に CRC を 1 バイト書く。
         * NvM_MarkPending() がこのブロック処理中の再書き込みを byte 0 から
         * やり直させるため、ここに到達した時点の RAM ミラーはこのジョブの
         * 開始以降変化していないことが保証されている。 */
        uint8 crc = NvM_CalcCrc8((const uint8*)blk->RamBlockDataAddress, blk->NvMNvBlockLength);
        NvM_Hw_WriteByte(NvM_CrcAddressForBase(activeBase, blk->NvMNvBlockLength), crc);
        NvM_ActiveByteIndex++;
    }
    else if (blk->Redundant != 0U && NvM_ActiveCopyIsMirror == 0U)
    {
        /* 冗長ブロックのプライマリ面を書き終えた: 続けてミラー面を
         * 先頭から書く（ジョブはまだ完了扱いにしない）。プライマリを
         * 完全に書き終えてからでないとミラーへ移らないため、この時点で
         * 電源が落ちてもプライマリは既に整合した新データを保持している。 */
        NvM_ActiveCopyIsMirror = 1U;
        NvM_ActiveByteIndex    = 0U;
    }
    else
    {
        /* ブロック完了（非冗長ブロック、または冗長ブロックの両面完了） */
        NvM_BlockPending[NvM_ActiveBlockId] = 0U;
        NvM_BlockResult[NvM_ActiveBlockId]  = NVM_REQ_OK;
        NvM_ActiveBlockId      = NVM_BLOCK_COUNT;
        NvM_ActiveByteIndex    = 0U;
        NvM_ActiveCopyIsMirror = 0U;
    }
}
