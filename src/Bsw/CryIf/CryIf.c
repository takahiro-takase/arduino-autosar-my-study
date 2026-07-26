/**
 * \file    CryIf.c
 * \brief   Crypto Interface 実装 (AUTOSAR SWS_CryptoInterface 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include "CryIf.h"
#include "Crypto.h"
#include "Det.h"

#define TAG "CryIf"

static uint8 CryIf_Initialized = 0U;

void CryIf_Init(void)
{
    CryIf_Initialized = 1U;
    DET_LOGI(TAG, "Init ok");
}

void CryIf_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    if (!CryIf_Initialized)
    {
        Det_ReportError(CRYIF_MODULE_ID, 0U, CRYIF_API_ID_GET_VERSION_INFO, CRYIF_E_UNINIT);
        return;
    }

    if (versioninfo == NULL)
    {
        Det_ReportError(CRYIF_MODULE_ID, 0U, CRYIF_API_ID_GET_VERSION_INFO, CRYIF_E_PARAM_POINTER);
        return;
    }

    versioninfo->vendorID         = CRYIF_VENDOR_ID;
    versioninfo->moduleID         = CRYIF_MODULE_ID;
    versioninfo->sw_major_version = CRYIF_SW_MAJOR_VERSION;
    versioninfo->sw_minor_version = CRYIF_SW_MINOR_VERSION;
    versioninfo->sw_patch_version = CRYIF_SW_PATCH_VERSION;
}

Std_ReturnType CryIf_ProcessJob(uint32 channelId, Crypto_JobType* job)
{
    if (!CryIf_Initialized)
    {
        Det_ReportError(CRYIF_MODULE_ID, 0U, CRYIF_API_ID_PROCESS_JOB, CRYIF_E_UNINIT);
        return E_NOT_OK;
    }

    if (channelId != CRYIF_CHANNEL_ID)
    {
        Det_ReportError(CRYIF_MODULE_ID, 0U, CRYIF_API_ID_PROCESS_JOB, CRYIF_E_PARAM_HANDLE);
        return E_NOT_OK;
    }

    if (job == NULL)
    {
        Det_ReportError(CRYIF_MODULE_ID, 0U, CRYIF_API_ID_PROCESS_JOB, CRYIF_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    /* [SWS_CryIf_00044]: 対応する Crypto Driver Object へディスパッチし、
     * 戻り値をそのまま返す。本プロジェクトは Crypto Driver Object が
     * CRYPTO_OBJECT_ID の 1 個のみのため固定で渡す。 */
    return Crypto_ProcessJob(CRYPTO_OBJECT_ID, job);
}
