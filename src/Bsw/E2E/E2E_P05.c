/**
 * \file    E2E_P05.c
 * \brief   E2E Profile 05 送信保護・受信チェック実装
 * \details AUTOSAR E2E P05 の送信保護（Protect）・受信チェック（Check）処理を
 *          実装する。CRC16 は SWS_E2E_00406 の擬似コードそのまま (開始値
 *          0xFFFF、Offset+2 からユーザーデータ末尾まで → DataID 下位バイト →
 *          DataID 上位バイトの順で計算)。詳細は E2E_P05.h のファイル冒頭
 *          コメント参照。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "E2E_P05.h"
#include "Det.h"

#define TAG "E2E_P05"

/* -----------------------------------------------------------------------
 * 内部ヘルパー
 * ----------------------------------------------------------------------- */

/**
 * \brief  CRC16 (多項式 0x1021、MSB first、非反転) を 1 バイト単位で更新する
 *         内部ルーティン。内部で自動的な開始値・XOR 補正は一切行わない
 *         素の実装 (E2E_P01.c の E2E_CalcCrc8() と同じ考え方)。
 *
 * \param[in] crc   現在の CRC 値。
 * \param[in] data  処理するバイト列。
 * \param[in] len   バイト数。
 * \return    更新後 CRC 値。
 */
static uint16 E2E_CalcCrc16(uint16 crc, const uint8 *data, uint8 len)
{
    DET_LOGT(TAG, "called");
    uint8 i;
    uint8 bit;
    for (i = 0U; i < len; i++)
    {
        crc ^= (uint16)((uint16)data[i] << 8U);
        for (bit = 0U; bit < 8U; bit++)
        {
            if (crc & 0x8000U)
                crc = (uint16)((crc << 1U) ^ 0x1021U);
            else
                crc = (uint16)(crc << 1U);
        }
    }
    return crc;
}

/**
 * \brief  E2E P05 の CRC16 計算範囲全体 (SWS_E2E_00406) をまとめて計算する。
 *         Data[Offset+2..DataLength-1] (Counter を含みユーザーデータまで、
 *         CRC16 バイト自身 [Offset, Offset+1] は除外) → DataID 下位バイト
 *         → DataID 上位バイトの順で 1 回の呼び出しにまとめる (Protect/Check
 *         の両方が同じ計算をするため共通化する)。
 *
 * \note   SWS_E2E_00406 の擬似コードは本来 `Config->Offset > 0` の場合、
 *         上記に先立って Data[0..Offset-1]（E2E ヘッダより前のバイト）も
 *         CRC 計算に含める分岐を持つが、本実装はこの分岐を持たない
 *         （常に Offset==0 側の経路のみを実装）。本プロジェクトの3用途
 *         (EngineHealthStatus/EngineInfo/AbsInfo、E2EXf_PBCfg.c 参照) は
 *         いずれも E2E ヘッダを PDU 先頭に置き Offset=0 固定のため実害は
 *         ないが、Offset>0 の構成を追加する場合はこの関数の拡張が必要。
 *
 * \param[in] Data        対象 PDU バッファ。
 * \param[in] DataLength  PDU 全体バイト数 (CRC16 バイトを含む)。
 * \param[in] Offset      E2E ヘッダの PDU 内バイトオフセット。
 * \param[in] DataID      CRC 計算に投入する DataID。
 * \return    計算した CRC16 値。
 */
static uint16 E2E_CalcCrc16Body(const uint8 *Data, uint8 DataLength, uint8 Offset, uint16 DataID)
{
    DET_LOGT(TAG, "called");
    uint16 crc = 0xFFFFU; /* SWS_E2E_00406: Crc_StartValue16: 0xFFFF */

    crc = E2E_CalcCrc16(crc, &Data[Offset + 2U], (uint8)(DataLength - Offset - 2U));

    {
        const uint8 idBytes[2] = { (uint8)(DataID & 0xFFU), (uint8)((DataID >> 8U) & 0xFFU) };
        crc = E2E_CalcCrc16(crc, idBytes, 2U);
    }

    return crc;
}

/* -----------------------------------------------------------------------
 * 公開 API
 * ----------------------------------------------------------------------- */

Std_ReturnType E2E_P05ProtectInit(E2E_P05ProtectStateType *State)
{
    DET_LOGT(TAG, "called");
    if (State == NULL)
        return E2E_E_INPUTERR_NULL;
    State->Counter = 0U;
    return E2E_E_OK;
}

Std_ReturnType E2E_P05Protect(
    const E2E_P05ConfigType *Config,
    E2E_P05ProtectStateType *State,
    uint8                   *Data,
    uint16                   Length)
{
    DET_LOGT(TAG, "called");
    if (Config == NULL || State == NULL || Data == NULL)
        return E2E_E_INPUTERR_NULL;

    if (Length < Config->DataLength)
        return E2E_E_INPUTERR_WRONG;

    /* Write Counter (SWS_E2E_00405) */
    Data[Config->Offset + 2U] = State->Counter;

    /* Compute CRC (SWS_E2E_00406) */
    {
        const uint16 crc = E2E_CalcCrc16Body(Data, Config->DataLength, Config->Offset, Config->DataID);

        /* Write CRC (SWS_E2E_00407): リトルエンディアン */
        Data[Config->Offset]      = (uint8)(crc & 0xFFU);
        Data[Config->Offset + 1U] = (uint8)((crc >> 8U) & 0xFFU);
    }

    /* Increment Counter (SWS_E2E_00409): uint8 の自然なラップアラウンド (0xFF の次は 0) */
    State->Counter = (uint8)(State->Counter + 1U);

    return E2E_E_OK;
}

Std_ReturnType E2E_P05CheckInit(E2E_P05CheckStateType *State)
{
    DET_LOGT(TAG, "called");
    if (State == NULL)
        return E2E_E_INPUTERR_NULL;
    State->Counter = 0U;
    State->Status  = E2E_P05STATUS_NONEWDATA;
    return E2E_E_OK;
}

Std_ReturnType E2E_P05Check(
    const E2E_P05ConfigType *Config,
    E2E_P05CheckStateType   *State,
    const uint8              *Data,
    uint16                    Length)
{
    DET_LOGT(TAG, "called");
    if (Config == NULL || State == NULL)
        return E2E_E_INPUTERR_NULL;

    /* Verify inputs (SWS_E2E_00412)。本プロジェクトの呼び出し方式 (フレーム
     * 受信時にのみ Check を呼ぶ) では Data==NULL/Length==0 の組は到達しない
     * 想定だが、擬似コード通り NONEWDATA として扱う (E2E_P01STATUS_NONEWDATA
     * と同じ位置づけ)。 */
    if (Data == NULL && Length == 0U)
    {
        State->Status = E2E_P05STATUS_NONEWDATA;
        return E2E_E_OK;
    }
    if (Data == NULL || Length != Config->DataLength)
    {
        State->Status = E2E_P05STATUS_ERROR;
        return E2E_E_OK;
    }

    /* Read Counter/CRC (SWS_E2E_00413/00414) */
    {
        const uint8  receivedCounter = Data[Config->Offset + 2U];
        const uint16 receivedCrc     = (uint16)Data[Config->Offset]
                                      | (uint16)((uint16)Data[Config->Offset + 1U] << 8U);
        const uint16 computedCrc     = E2E_CalcCrc16Body(Data, Config->DataLength, Config->Offset, Config->DataID);

        /* Do checks (SWS_E2E_00416) */
        if (receivedCrc != computedCrc)
        {
            /* CRC 不一致時は Counter 側の状態を一切変更しない
             * (次に CRC が正しいフレームが来た時点で通常通り判定する) */
            State->Status = E2E_P05STATUS_ERROR;
            return E2E_E_OK;
        }

        {
            /* deltaCounter: 0xFF ラップアラウンドを含めた uint8 の単純な引き算
             * (Profile01 の mod-15 補正と違い、Profile05 は 0-255 のフル
             * レンジを使うため uint8 の自然なラップアラウンドがそのまま
             * mod-256 補正になる) */
            const uint8 deltaCounter = (uint8)(receivedCounter - State->Counter);

            if (deltaCounter > Config->MaxDeltaCounter)
            {
                State->Status = E2E_P05STATUS_WRONGSEQUENCE;
            }
            else if (deltaCounter == 0U)
            {
                State->Status = E2E_P05STATUS_REPEATED;
            }
            else if (deltaCounter == 1U)
            {
                State->Status = E2E_P05STATUS_OK;
            }
            else
            {
                State->Status = E2E_P05STATUS_OKSOMELOST;
            }

            State->Counter = receivedCounter;
        }
    }

    return E2E_E_OK;
}

E2E_PCheckStatusType E2E_P05MapStatusToSM(
    Std_ReturnType    CheckReturn,
    E2E_P05StatusType Status)
{
    /* [SWS_E2E_00216] によりライブラリは Det/Dem を呼ばない方針のため
     * DET_LOGT すら使わない（値渡しのみで NULL チェックの概念自体が無い）。 */
    if (CheckReturn != E2E_E_OK)
        return E2E_P_ERROR;  /* [SWS_E2E_00454]: Status に関わらず優先 */

    /* [SWS_E2E_00453] */
    switch (Status)
    {
    case E2E_P05STATUS_OK:
    case E2E_P05STATUS_OKSOMELOST:
        return E2E_P_OK;
    case E2E_P05STATUS_REPEATED:
        return E2E_P_REPEATED;
    case E2E_P05STATUS_NONEWDATA:
        return E2E_P_NONEWDATA;
    case E2E_P05STATUS_WRONGSEQUENCE:
        return E2E_P_WRONGSEQUENCE;
    case E2E_P05STATUS_ERROR:
    default:
        return E2E_P_ERROR;
    }
}
