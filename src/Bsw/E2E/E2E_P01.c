/**
 * \file    E2E_P01.c
 * \brief   E2E Profile 01 送信保護・受信チェック実装
 * \details AUTOSAR E2E P01 の送信保護（Protect）・受信チェック（Check）処理を
 *          実装する。Check は CRC8 SAE J1850 検証 → 4 ビットカウンタ検証の順に
 *          行い、E2E_P01StatusType（SWS_E2E_00022 準拠の 8 状態）で結果を返す。
 *          カウンタ飛びが許容超過（WRONGSEQUENCE）を検知した後は、
 *          SyncCounterInit 回分の連続正常受信があるまで SYNC を返し続け、
 *          安易に「正常」へ復帰しない再ロック機構を持つ。
 *          Protect は Check と対になるエンコード処理（Counter 更新 →
 *          CRC8 計算・書き込み）を行う。
 *
 *          CRC8 SAE J1850 仕様（SWS_E2E_00083 / SWS_E2E_00070）:
 *            多項式 : 0x1D (x^8 + x^4 + x^3 + x^2 + 1)
 *            初期値 : 0x00
 *            最終XOR: 0x00
 *            ビット反転: なし (MSB first)
 *
 *          実 AUTOSAR 環境との違い（意図的な簡略化）:
 *            実 AUTOSAR の Crc_CalculateCRC8()（CRC ライブラリ）は R4.0 以降、
 *            関数内部で常に開始値・最終 XOR とも 0xFF を適用する仕様になって
 *            いる。そのため E2E ライブラリ側は、外部から見た「実効開始値・
 *            XOR 値 0x00」を実現するために、呼び出しごとに 0xFF を渡す/XOR
 *            し返すという多段の相殺トリックを使う（詳細は
 *            docs/AUTOSAR_SWS_E2ELibrary.pdf の Figure 7-6、および
 *            SWS_E2E_00190 のコメント参照）。
 *            本実装の E2E_CalcCrc8() はそのような内部自動 XOR を行わない
 *            素の CRC8（渡された crc をそのままレジスタ初期値として使い、
 *            戻り値もそのままレジスタ値）であるため、相殺トリックを再現する
 *            必要はなく、単純に開始値 0x00・最終 XOR なしで計算すれば
 *            SWS_E2E_00083 が要求する CRC-8-SAE-J1850（開始値・XOR 値とも
 *            0x00 のバリアント）と一致する。
 *
 *          CRC の計算範囲 (AUTOSAR E2E P01 DataIDMode=BOTH、SWS_E2E_00082 /
 *          Figure 7-6 "Calculate CRC over Data ID and Data" 準拠):
 *            DataID の下位バイト → DataID の上位バイト
 *            → Data[0..CRCOffset-1]（CRC バイトより前、CRC が先頭なら 0 バイト）
 *            → Data[CRCOffset+1..DataLength-1]（CRC バイトより後、CRC が末尾なら 0 バイト）
 *          ※ CRC バイト (Data[CRCOffset]) 自身は計算範囲に含まない。
 *          CRCOffset は PDU 内の任意の位置を取りうる（AUTOSAR 標準バリアント
 *          1A/1B/1C はいずれも CRC を先頭バイトに固定するため、本実装もそれに
 *          倣い CRCOffset=0 を使用している）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License
 */

#include "E2E_P01.h"
#include "Det.h"

#define TAG "E2E_P01"

/* -----------------------------------------------------------------------
 * 内部ヘルパー
 * ----------------------------------------------------------------------- */

/**
 * \brief  CRC8 SAE J1850 を 1 バイト単位で更新する内部ルーティン。
 *         内部で自動的な開始値・XOR 補正は一切行わない素の実装。
 *         呼び出し元 (E2E_P01Check/Protect) が crc=0x00 を渡せば、
 *         SWS_E2E_00083 が要求する開始値・最終 XOR とも 0x00 の
 *         CRC8-SAE-J1850 バリアントとそのまま一致する。
 *
 * \param[in] crc   現在の CRC 値。
 * \param[in] data  処理するバイト列。
 * \param[in] len   バイト数。
 * \return    更新後 CRC 値。
 */
static uint8 E2E_CalcCrc8(uint8 crc, const uint8 *data, uint8 len)
{
    uint8 i;
    uint8 bit;
    for (i = 0U; i < len; i++)
    {
        crc ^= data[i];
        for (bit = 0U; bit < 8U; bit++)
        {
            if (crc & 0x80U)
                crc = (uint8)((crc << 1U) ^ 0x1DU);
            else
                crc = (uint8)(crc << 1U);
        }
    }
    return crc;
}

/**
 * \brief  DataID に続けて、CRC バイト自身を除いた Data 全体に対する CRC8 を計算する。
 *
 * \details AUTOSAR E2E P01 Figure 7-6 "Calculate CRC over Data ID and Data" に準拠し、
 *          CRC バイトより前の区間 (Data[0..CRCOffset-1]) と後の区間
 *          (Data[CRCOffset+1..DataLength-1]) の 2 区間に分けて計算する。
 *          CRC が先頭バイト (CRCOffset=0、AUTOSAR 標準バリアント 1A/1B/1C) の場合は
 *          前区間が 0 バイトになり、CRC が末尾バイトの場合は後区間が 0 バイトになる。
 *          これにより CRCOffset が PDU 内のどの位置にあっても正しく計算できる。
 *
 * \param[in] crc         DataID 分まで計算済みの CRC 値。
 * \param[in] Data        対象 PDU バッファ。
 * \param[in] DataLength  PDU 全体バイト数 (CRC バイトを含む)。
 * \param[in] CRCOffset   CRC バイトの PDU 内オフセット。
 * \return    Data 全体分まで計算した CRC 値。
 */
static uint8 E2E_CalcCrc8OverDataExcludingCrcByte(
    uint8 crc, const uint8 *Data, uint8 DataLength, uint8 CRCOffset)
{
    if (CRCOffset > 0U)
        crc = E2E_CalcCrc8(crc, Data, CRCOffset);

    if ((uint8)(CRCOffset + 1U) < DataLength)
        crc = E2E_CalcCrc8(crc, &Data[CRCOffset + 1U],
                            (uint8)(DataLength - CRCOffset - 1U));

    return crc;
}

/* -----------------------------------------------------------------------
 * 公開 API
 * ----------------------------------------------------------------------- */

void E2E_P01CheckInit(E2E_P01CheckStateType *State)
{
    if (State == NULL)
        return;
    State->LastValidCounter = 0U;
    State->Status           = E2E_P01STATUS_INITIAL;
    State->WaitForFirstData = 1U;
    State->SyncCounter      = 0U;
}

E2E_P01StatusType E2E_P01Check(
    const E2E_P01ConfigType *Config,
    E2E_P01CheckStateType   *State,
    const uint8             *Data,
    uint8                    Length)
{
    if (Config == NULL || State == NULL || Data == NULL)
        return E2E_P01STATUS_ERROR;

    /* DLC 不足は即エラー */
    if (Length < Config->DataLength)
    {
        State->Status = E2E_P01STATUS_ERROR;
        return E2E_P01STATUS_ERROR;
    }

    /* ------------------------------------------------------------------
     * CRC 検証
     * 計算範囲: DataID_low, DataID_high, Data[0..CRCOffset-1], Data[CRCOffset+1..DataLength-1]
     * ------------------------------------------------------------------ */
    {
        const uint8 dataId_low  = (uint8)(Config->DataID & 0xFFU);
        const uint8 dataId_high = (uint8)((Config->DataID >> 8U) & 0xFFU);

        /* SWS_E2E_00083: 開始値・最終 XOR とも 0x00（E2E_CalcCrc8 は
         * 自動補正を行わない素の実装のため、そのまま渡すだけでよい）。 */
        uint8 crc = 0x00U;
        crc = E2E_CalcCrc8(crc, &dataId_low,  1U);
        crc = E2E_CalcCrc8(crc, &dataId_high, 1U);
        crc = E2E_CalcCrc8OverDataExcludingCrcByte(crc, Data, Config->DataLength, Config->CRCOffset);

        if (crc != Data[Config->CRCOffset])
        {
            /* CRC 不一致時は Counter 側の状態を一切変更しない
             * (次に CRC が正しいフレームが来た時点で通常通り判定する) */
            State->Status = E2E_P01STATUS_WRONGCRC;
            return E2E_P01STATUS_WRONGCRC;
        }
    }

    /* ------------------------------------------------------------------
     * カウンタ検証
     * ------------------------------------------------------------------ */
    {
        const uint8 received = Data[Config->CounterOffset] & 0x0FU;

        /* 初回受信: カウンタ基準を設定して INITIAL を返す */
        if (State->WaitForFirstData)
        {
            State->LastValidCounter = received;
            State->WaitForFirstData = 0U;
            State->SyncCounter      = 0U;
            State->Status           = E2E_P01STATUS_INITIAL;
            return E2E_P01STATUS_INITIAL;
        }

        /* deltaCounter: SWS_E2E_00075 により Profile 1 のカウンタは 0〜14 の
         * 15 値を循環する（15=0xF は予約値でスキップ）。そのため折り返しの
         * 補正は mod-16 (& 0x0FU) ではなく mod-15 (+15) が正しい
         * (E2ELibrary Figure 7-7)。mod-16 (+16) は Profile 2 側の折り返し
         * 補正式 (Figure 7-12) であり、Profile 1 へ適用すると
         * LastValidCounter=14→received=0 の正常な折り返しのたびに
         * delta=2（本来は 1）と誤算出し、MaxDeltaCounter=1 の設定下では
         * 15 フレームに 1 回、データ欠落が無いにもかかわらず
         * WRONGSEQUENCE と誤判定してしまう。 */
        const uint8 delta = (received >= State->LastValidCounter)
                             ? (uint8)(received - State->LastValidCounter)
                             : (uint8)(15U + received - State->LastValidCounter);

        if (delta == 0U)
        {
            /* 同一カウンタ = フレーム重複。LastValidCounter・SyncCounter は
             * どちらも維持する (継続性の判定材料が増えたわけではないため) */
            State->Status = E2E_P01STATUS_REPEATED;
            return E2E_P01STATUS_REPEATED;
        }

        if (delta > Config->MaxDeltaCounter)
        {
            /* 許容超過 = 過剰消失。受信値を新しい基準として受け入れつつ、
             * SyncCounterInit 回分の再同期（再ロック）を開始する。
             * 再ロック中は継続性が未確定なため、OK ではなく SYNC を返す。 */
            State->LastValidCounter = received;
            State->SyncCounter      = Config->SyncCounterInit;
            State->Status           = E2E_P01STATUS_WRONGSEQUENCE;
            return E2E_P01STATUS_WRONGSEQUENCE;
        }

        /* delta は 1..MaxDeltaCounter の範囲内 = カウンタとしては正常進行 */
        State->LastValidCounter = received;

        if (State->SyncCounter > 0U)
        {
            /* 再ロック中: 個々のフレームの CRC/Counter 自体は正常でも、
             * まだ「完全に信頼できる」とは宣言せず SYNC を返す */
            State->SyncCounter--;
            State->Status = E2E_P01STATUS_SYNC;
            DET_LOGW(TAG, "st=%u sync=%u",
                     (unsigned)State->Status, (unsigned)State->SyncCounter);
            return E2E_P01STATUS_SYNC;
        }

        if (delta == 1U)
        {
            State->Status = E2E_P01STATUS_OK;
            return E2E_P01STATUS_OK;
        }
        else
        {
            /* 1 < delta <= MaxDeltaCounter: 正常だが一部フレームが消失 */
            State->Status = E2E_P01STATUS_OKSOMELOST;
            return E2E_P01STATUS_OKSOMELOST;
        }
    }
}

void E2E_P01ProtectInit(E2E_P01ProtectStateType *State)
{
    if (State == NULL)
        return;
    State->Counter = 0U;
}

void E2E_P01Protect(
    const E2E_P01ConfigType *Config,
    E2E_P01ProtectStateType *State,
    uint8                   *Data,
    uint8                    Length)
{
    if (Config == NULL || State == NULL || Data == NULL)
        return;

    if (Length < Config->DataLength)
        return;

    /* Counter 書き込み (下位 4bit のみ使用。今回送信する値を書き込んでから
     * 次回用に進める)。SWS_E2E_00075: 14 (0xE) に達したら次は 0 に戻る
     * （15=0xF はスキップ、予約値）。単純な mod-16 (+1 & 0x0FU) では 15 を
     * 経由してしまい仕様違反になる。 */
    Data[Config->CounterOffset] = State->Counter & 0x0FU;
    State->Counter = (State->Counter >= 14U) ? 0U : (uint8)(State->Counter + 1U);

    /* CRC8 計算 (E2E_P01Check と同じ範囲: DataID_low, DataID_high,
     * Data[0..CRCOffset-1], Data[CRCOffset+1..DataLength-1]) */
    {
        const uint8 dataId_low  = (uint8)(Config->DataID & 0xFFU);
        const uint8 dataId_high = (uint8)((Config->DataID >> 8U) & 0xFFU);

        /* SWS_E2E_00083: 開始値・最終 XOR とも 0x00（E2E_P01Check と同じ）。 */
        uint8 crc = 0x00U;
        crc = E2E_CalcCrc8(crc, &dataId_low,  1U);
        crc = E2E_CalcCrc8(crc, &dataId_high, 1U);
        crc = E2E_CalcCrc8OverDataExcludingCrcByte(crc, Data, Config->DataLength, Config->CRCOffset);

        Data[Config->CRCOffset] = crc;
    }
}
