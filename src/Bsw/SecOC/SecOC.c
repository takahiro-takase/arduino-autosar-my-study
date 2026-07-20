/**
 * \file    SecOC.c
 * \brief   Secure Onboard Communication 実装 (AUTOSAR SWS_SecureOnboardCommunication 準拠)
 * \details PduR のルーティング経路上に直接挟まる、RX（受信検証）専用の簡易 SecOC。
 *          E2E とは異なり、Com のコールバックフック（RxIndicationCbk 等）経由
 *          ではなく、PduR の宛先モジュール（PduR_RxDestType、CanTp/Com と同じ
 *          立ち位置）として実装している。これは E2E Transformer 方式が E2E
 *          固有の統合方式であり、SecOC には対応する「Com フック経由」の
 *          統合方式が実 AUTOSAR に存在しないため（SecOC は常に独立した PduR
 *          モジュールとして構成される）。
 *
 *          Secured I-PDU を受信すると、Authentic Payload / Freshness Value /
 *          切り詰め MAC に分離し、AES-128-CMAC（SecOC_Cmac.c、自前実装）で
 *          MAC を再計算して照合、続けてフレッシュネスの単調増加を確認する
 *          （リプレイ攻撃対策）。両方成功した場合のみ Authentic Payload を
 *          Com_RxIndication() へ転送する。検証に失敗したデータは Com へ
 *          一切渡さない。
 *
 *          TX（自ら Secured I-PDU を生成して送信する）方向は未実装。本実装の
 *          デモ用フレーム（ImmobilizerCmd）は「外部の KeyFobEcu から受信する」
 *          想定の RX 専用フレームであり、送信側は uds_tester（Python、
 *          pycryptodome で本物の AES-CMAC を計算する）が模擬する。これは
 *          EngineInfo/AbsInfo の E2E 保護が RX 専用で、Python 側が送信側を
 *          模擬しているのと同じ構図である。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include "SecOC.h"
#include "SecOC_Cfg.h"
#include "SecOC_Cmac.h"
#include "Com.h"
#include "Det.h"

#define TAG "SecOC"

/* Authenticator 対象データ（DataId | AuthenticPayload | FreshnessValue）を
 * 組み立てる作業バッファの上限。CAN フレーム（最大 DLC=8）に収まる構成である
 * 限り、2(DataId) + 8(AuthenticPayload 最大) + 4(Freshness 最大) = 14 で
 * 十分収まるが、余裕を見て 16（AES の 1 ブロック長）とする。 */
#define SECOC_AUTH_INPUT_MAX  16U

static const SecOC_ConfigType* SecOC_ConfigPtr = NULL;

/* Secured I-PDU ごとの最後に受理した Freshness Value（リプレイ検知用）。
 * SecOC_HasBaseline が 0 の間は「まだ 1 度も検証成功していない」ことを示し、
 * 初回受信時はどんな Freshness Value でも（単調増加チェックをスキップして）
 * 受理する。 */
static uint8 SecOC_LastFreshness[SECOC_RX_PDU_COUNT];
static uint8 SecOC_HasBaseline[SECOC_RX_PDU_COUNT];

/**
 * \brief   SecOC_RxPduConfigType テーブルから SecOCRxPduId に一致するエントリを検索する。
 *
 * \details Com_FindRxIPdu() 等、本プロジェクトの他モジュールと同じ「配列添字に
 *          暗黙依存せず、明示 ID フィールドで線形検索する」方針に倣う。
 *
 * \param[in]   rxPduId    検索する SecOC RX Secured I-PDU の ID。
 * \param[out]  tableIndex 見つかった場合、テーブル内インデックスを書き込む
 *                         （フレッシュネス状態配列の添字に使う）。NULL 禁止。
 *
 * \return  一致するエントリへのポインタ。見つからない場合は NULL。
 */
static const SecOC_RxPduConfigType* SecOC_FindRxPdu(PduIdType rxPduId, uint8* tableIndex)
{
    for (uint8 i = 0U; i < SecOC_ConfigPtr->RxPduCount; i++)
    {
        if (SecOC_ConfigPtr->RxPdus[i].SecOCRxPduId == rxPduId)
        {
            *tableIndex = i;
            return &SecOC_ConfigPtr->RxPdus[i];
        }
    }
    return NULL;
}

void SecOC_Init(const SecOC_ConfigType* config)
{
    if (config == NULL)
    {
        DET_LOGE(TAG, "Init E: config NULL");
        return;
    }
    if (config->RxPduCount > SECOC_RX_PDU_COUNT)
    {
        DET_LOGE(TAG, "Init E: RxPduCount>max");
        return;
    }

    SecOC_ConfigPtr = config;

    for (uint8 i = 0U; i < SECOC_RX_PDU_COUNT; i++)
    {
        SecOC_LastFreshness[i] = 0U;
        SecOC_HasBaseline[i]   = 0U;
    }

    (void)SecOC_Aes128_SelfTest();

    DET_LOGI(TAG, "Init ok RX=%u", (unsigned)config->RxPduCount);
}

void SecOC_IfRxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)
{
    if (SecOC_ConfigPtr == NULL || PduInfoPtr == NULL || PduInfoPtr->SduDataPtr == NULL)
        return;

    uint8 tableIndex = 0U;
    const SecOC_RxPduConfigType* cfg = SecOC_FindRxPdu(RxPduId, &tableIndex);
    if (cfg == NULL)
    {
        DET_LOGW(TAG, "RxInd W: no matching SecOC RX PDU for id=%u", (unsigned)RxPduId);
        return;
    }

    if (PduInfoPtr->SduLength < cfg->SecuredPduLength)
    {
        DET_LOGW(TAG, "RxInd W: iPdu=%u length %u < expected %u, discard",
                 (unsigned)RxPduId, (unsigned)PduInfoPtr->SduLength, (unsigned)cfg->SecuredPduLength);
        return;
    }

    const uint16 authInputLen = (uint16)(2U + cfg->AuthenticPduLength + cfg->FreshnessLength);
    if (authInputLen > SECOC_AUTH_INPUT_MAX)
    {
        DET_LOGE(TAG, "RxInd E: iPdu=%u authInputLen=%u exceeds buffer",
                 (unsigned)RxPduId, (unsigned)authInputLen);
        return;
    }

    const uint8* secured = PduInfoPtr->SduDataPtr;

    /* DataToAuthenticator = DataId(2byte, Big Endian) | Authentic Payload |
     * Complete Freshness Value
     * (docs/AUTOSAR_SWS_SecureOnboardCommunication.pdf [7.1.1.2] 1707行、
     * Big Endian は [SWS_SecOC_00011]) */
    uint8 authInput[SECOC_AUTH_INPUT_MAX];
    authInput[0] = (uint8)(cfg->DataId >> 8);
    authInput[1] = (uint8)(cfg->DataId & 0xFFU);
    for (uint8 b = 0U; b < cfg->AuthenticPduLength; b++)
        authInput[2U + b] = secured[b];
    for (uint8 b = 0U; b < cfg->FreshnessLength; b++)
        authInput[2U + cfg->AuthenticPduLength + b] = secured[cfg->FreshnessOffset + b];

    uint8 mac[SECOC_CMAC_SIZE];
    SecOC_Cmac_Calculate(cfg->Key, authInput, authInputLen, mac);

    /* 切り詰め MAC は AES-CMAC 128bit 出力の上位（MSB側）MacTxLength バイトを
     * 比較する（[SWS_SecOC_00192]、Figure 5 "truncated down to the most
     * significant bits"）。 */
    /* 定数時間比較（早期breakしない）。MAC検証は認証機能の核であり、不一致バイト
     * 位置に応じて処理時間が変わる実装は、攻撃者に偽MACをバイト単位で総当たり
     * させる余地を与えるタイミングサイドチャネルになり得るため避ける。 */
    uint8 macDiff = 0U;
    for (uint8 b = 0U; b < cfg->MacTxLength; b++)
    {
        macDiff |= (uint8)(mac[b] ^ secured[cfg->MacOffset + b]);
    }
    if (macDiff != 0U)
    {
        DET_LOGW(TAG, "RxInd W: iPdu=%u MAC verification failed (tampered or wrong key)",
                 (unsigned)RxPduId);
        return;
    }

    /* フレッシュネス検証（リプレイ検知）。本実装は FreshnessLength=1（8bit）
     * 固定のみサポートする（SecOC_Types.h の簡略化に関する説明参照）。
     * 単調増加チェックは折り返し（wraparound）を考慮した「半区間」判定
     * （received-last の差が 1〜127 なら「より新しい」とみなす）を用いる。
     * 差が 0（完全一致 = リプレイ）または 128 以上（古い方向、または折り返し
     * 境界で新旧が確定できない）は拒否する。初回受信（SecOC_HasBaseline=0）は
     * 比較対象がないため、この検証自体をスキップして無条件に受理する。 */
    const uint8 freshness = secured[cfg->FreshnessOffset];
    if (SecOC_HasBaseline[tableIndex] != 0U)
    {
        const uint8 delta = (uint8)(freshness - SecOC_LastFreshness[tableIndex]);
        if (delta == 0U || delta >= 128U)
        {
            DET_LOGW(TAG, "RxInd W: iPdu=%u freshness check failed (replay or stale, got=%u last=%u)",
                     (unsigned)RxPduId, (unsigned)freshness, (unsigned)SecOC_LastFreshness[tableIndex]);
            return;
        }
    }
    SecOC_LastFreshness[tableIndex] = freshness;
    SecOC_HasBaseline[tableIndex]   = 1U;

    DET_LOGI(TAG, "RxInd: iPdu=%u verified OK (freshness=%u)", (unsigned)RxPduId, (unsigned)freshness);

    /* 検証成功: Authentic Payload（Secured I-PDU の先頭 AuthenticPduLength
     * バイト）のみを Com へ渡す。Freshness/MAC バイトは Com の関知するところ
     * ではない（E2E Transformer 方式で Com が E2E の存在を知らないのと
     * 同じ設計思想）。 */
    PduInfoType authenticPduInfo = {
        .SduDataPtr = (uint8*)secured,
        .SduLength  = cfg->AuthenticPduLength
    };
    Com_RxIndication(cfg->ComRxPduId, &authenticPduInfo);
}
