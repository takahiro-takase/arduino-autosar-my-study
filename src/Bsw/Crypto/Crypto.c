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

void Crypto_Init(void)
{
    (void)Crypto_Aes128_SelfTest();
    Crypto_Initialized = 1U;
    DET_LOGI(TAG, "Init ok");
}

void Crypto_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
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

    uint8 fullMac[CRYPTO_CMAC_SIZE];
    Crypto_Cmac_Calculate(Crypto_KeyTable[job->cryptoKeyId], job->inputPtr, (uint16)job->inputLength, fullMac);

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
