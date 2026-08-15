/**
 * \file    Crypto.c
 * \brief   Crypto Driver 実装 (AUTOSAR SWS_CryptoDriver 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include "Crypto.h"
#include "Crypto_Aes128.h"
#include "Crypto_Cmac.h"
#include "Crypto_PBCfg.h"
#include "Det.h"

#define TAG "Crypto"

static uint8 Crypto_Initialized = 0U;

/** 鍵の RAM 実体（Crypto_PBCfg.c の Crypto_KeyTable を初期値として Init 時に
 *  コピーする）。KeyM 経由の Crypto_KeyElementSet() で書き換えられる。
 *  [SWS_KeyM_00046] の「NVM に保存された鍵は初期化時に CSM(RAM) キースロット
 *  へロードされる」という考え方を、本プロジェクトでは
 *  「Crypto_PBCfg.c の const テーブル → Init 時に RAM へコピー」に簡略化する
 *  （実際の NVM 永続化は行わない。再起動すれば PBCfg の初期値に戻る）。 */
static uint8 Crypto_KeyStore[CRYPTO_KEY_COUNT][CRYPTO_AES128_KEY_SIZE];

/** 鍵ごとの有効/無効状態。Crypto_KeyElementSet() で書き換えた直後は無効化され、
 *  Crypto_KeySetValid() が呼ばれるまで Crypto_ProcessJob() での使用を拒否する
 *  （[SWS_KeyM_00008]/[SWS_Csm_00958] の「更新した鍵はセッション終了まで無効」
 *  という仕様を、実際に MAC 生成/検証へ反映させる）。 */
static uint8 Crypto_KeyValid[CRYPTO_KEY_COUNT];

void Crypto_Init(void)
{
    DET_LOGT(TAG, "called");
    (void)Crypto_Aes128_SelfTest();

    for (uint32 k = 0U; k < CRYPTO_KEY_COUNT; k++)
    {
        for (uint32 b = 0U; b < CRYPTO_AES128_KEY_SIZE; b++)
            Crypto_KeyStore[k][b] = Crypto_KeyTable[k][b];
        Crypto_KeyValid[k] = 1U;
    }

    Crypto_Initialized = 1U;
    DET_LOGI(TAG, "Init ok");
}

void Crypto_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    DET_LOGT(TAG, "called");
    if (versioninfo == NULL)
    {
        Det_ReportError(CRYPTO_MODULE_ID, 0U, CRYPTO_API_ID_GET_VERSION_INFO, CRYPTO_E_PARAM_POINTER);
        return;
    }

    versioninfo->vendorID         = CRYPTO_VENDOR_ID;
    versioninfo->moduleID         = CRYPTO_MODULE_ID;
    versioninfo->sw_major_version = CRYPTO_SW_MAJOR_VERSION;
    versioninfo->sw_minor_version = CRYPTO_SW_MINOR_VERSION;
    versioninfo->sw_patch_version = CRYPTO_SW_PATCH_VERSION;
}

Std_ReturnType Crypto_ProcessJob(uint32 objectId, Crypto_JobType* job)
{
    DET_LOGT(TAG, "called");
    if (!Crypto_Initialized)
    {
        Det_ReportError(CRYPTO_MODULE_ID, 0U, CRYPTO_API_ID_PROCESS_JOB, CRYPTO_E_UNINIT);
        return E_NOT_OK;
    }

    if (objectId != CRYPTO_OBJECT_ID)
    {
        Det_ReportError(CRYPTO_MODULE_ID, 0U, CRYPTO_API_ID_PROCESS_JOB, CRYPTO_E_PARAM_HANDLE);
        return E_NOT_OK;
    }

    if (job == NULL || job->inputPtr == NULL || job->macPtr == NULL)
    {
        Det_ReportError(CRYPTO_MODULE_ID, 0U, CRYPTO_API_ID_PROCESS_JOB, CRYPTO_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    if (job->cryptoKeyId >= CRYPTO_KEY_COUNT || job->macLength > CRYPTO_CMAC_SIZE)
    {
        Det_ReportError(CRYPTO_MODULE_ID, 0U, CRYPTO_API_ID_PROCESS_JOB, CRYPTO_E_PARAM_HANDLE);
        return E_NOT_OK;
    }

    if (!Crypto_KeyValid[job->cryptoKeyId])
    {
        /* [SWS_Crypto_00043] CRYPTO_E_KEY_NOT_VALID は Std_ReturnType の拡張値
         * （DET の Development/Runtime Error のいずれでもない）のため
         * Det_ReportError() は呼ばない。 */
        DET_LOGW(TAG, "ProcessJob W: cryptoKeyId=%u not valid (pending KeySetValid)",
                 (unsigned)job->cryptoKeyId);
        return E_NOT_OK;
    }

    uint8 fullMac[CRYPTO_CMAC_SIZE];
    Crypto_Cmac_Calculate(Crypto_KeyStore[job->cryptoKeyId], job->inputPtr, (uint16)job->inputLength, fullMac);

    if (job->service == CRYPTO_MACGENERATE)
    {
        for (uint32 b = 0U; b < job->macLength; b++)
            job->macPtr[b] = fullMac[b];
        return E_OK;
    }

    if (job->service == CRYPTO_MACVERIFY)
    {
        if (job->verifyResultPtr == NULL)
        {
            Det_ReportError(CRYPTO_MODULE_ID, 0U, CRYPTO_API_ID_PROCESS_JOB, CRYPTO_E_PARAM_POINTER);
            return E_NOT_OK;
        }

        /* 定数時間比較（早期breakしない）。MAC検証は認証機能の核であり、不一致
         * バイト位置に応じて処理時間が変わる実装は、攻撃者に偽MACをバイト単位で
         * 総当たりさせる余地を与えるタイミングサイドチャネルになり得るため避ける
         * （元 SecOC_IfRxIndication() が行っていた比較ロジックを、責務として
         * 正しい Crypto Driver 層へ移設した）。 */
        uint8 macDiff = 0U;
        for (uint32 b = 0U; b < job->macLength; b++)
            macDiff |= (uint8)(fullMac[b] ^ job->macPtr[b]);

        *job->verifyResultPtr = (macDiff == 0U) ? CRYPTO_E_VER_OK : CRYPTO_E_VER_NOT_OK;
        return E_OK;
    }

    DET_LOGE(TAG, "ProcessJob E: unsupported service=%u", (unsigned)job->service);
    return E_NOT_OK;
}

Std_ReturnType Crypto_KeyElementSet(uint32 cryptoKeyId, uint32 keyElementId,
                                     const uint8* keyPtr, uint32 keyLength)
{
    DET_LOGT(TAG, "called");
    if (!Crypto_Initialized)
    {
        Det_ReportError(CRYPTO_MODULE_ID, 0U, CRYPTO_API_ID_KEY_ELEMENT_SET, CRYPTO_E_UNINIT);
        return E_NOT_OK;
    }

    if (cryptoKeyId >= CRYPTO_KEY_COUNT || keyElementId != CRYPTO_KEY_ELEMENT_ID_CIPHER_KEY)
    {
        Det_ReportError(CRYPTO_MODULE_ID, 0U, CRYPTO_API_ID_KEY_ELEMENT_SET, CRYPTO_E_PARAM_HANDLE);
        return E_NOT_OK;
    }

    if (keyPtr == NULL)
    {
        Det_ReportError(CRYPTO_MODULE_ID, 0U, CRYPTO_API_ID_KEY_ELEMENT_SET, CRYPTO_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    /* [SWS_Crypto_00146] 相当。本プロジェクトは部分アクセス不可の固定長鍵要素
     * のみを扱うため、長さ不一致は単純に拒否する（CRYPTO_E_KEY_SIZE_MISMATCH
     * という戻り値種別は導入せず、E_NOT_OK に統一。Crypto.h 冒頭コメント参照）。 */
    if (keyLength != CRYPTO_AES128_KEY_SIZE)
    {
        Det_ReportError(CRYPTO_MODULE_ID, 0U, CRYPTO_API_ID_KEY_ELEMENT_SET, CRYPTO_E_PARAM_VALUE);
        return E_NOT_OK;
    }

    for (uint32 b = 0U; b < CRYPTO_AES128_KEY_SIZE; b++)
        Crypto_KeyStore[cryptoKeyId][b] = keyPtr[b];

    /* [SWS_KeyM_00016] 相当: 鍵内容を書き換えた直後は無効化し、
     * Crypto_KeySetValid() が呼ばれるまで ProcessJob() での使用を拒否する。 */
    Crypto_KeyValid[cryptoKeyId] = 0U;
    DET_LOGI(TAG, "KeyElementSet ok cryptoKeyId=%u (now pending KeySetValid)", (unsigned)cryptoKeyId);
    return E_OK;
}

Std_ReturnType Crypto_KeySetValid(uint32 cryptoKeyId)
{
    DET_LOGT(TAG, "called");
    if (!Crypto_Initialized)
    {
        Det_ReportError(CRYPTO_MODULE_ID, 0U, CRYPTO_API_ID_KEY_SET_VALID, CRYPTO_E_UNINIT);
        return E_NOT_OK;
    }

    if (cryptoKeyId >= CRYPTO_KEY_COUNT)
    {
        Det_ReportError(CRYPTO_MODULE_ID, 0U, CRYPTO_API_ID_KEY_SET_VALID, CRYPTO_E_PARAM_HANDLE);
        return E_NOT_OK;
    }

    Crypto_KeyValid[cryptoKeyId] = 1U;
    DET_LOGI(TAG, "KeySetValid ok cryptoKeyId=%u", (unsigned)cryptoKeyId);
    return E_OK;
}
