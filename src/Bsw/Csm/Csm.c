/**
 * \file    Csm.c
 * \brief   Crypto Service Manager 実装 (AUTOSAR SWS_CryptoServiceManager 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include "Csm.h"
#include "Csm_PBCfg.h"
#include "CryIf.h"
#include "Crypto_Cmac.h"
#include "Det.h"

#define TAG "Csm"

static uint8 Csm_Initialized = 0U;

/**
 * \brief   Csm_JobConfigData から jobId かつ期待するプリミティブ種別に
 *          一致するエントリを検索する。
 *
 * \details 他モジュールと同じ「配列添字に暗黙依存せず、明示 ID フィールドで
 *          線形検索する」方針に倣う（Com_FindRxIPdu 等参照）。
 */
static const Csm_JobConfigType* Csm_FindJob(uint32 jobId, Crypto_ServiceInfoType expectedService)
{
    for (uint8 i = 0U; i < CSM_JOB_COUNT; i++)
    {
        if (Csm_JobConfigData[i].JobId == jobId && Csm_JobConfigData[i].Service == expectedService)
            return &Csm_JobConfigData[i];
    }
    return NULL;
}

void Csm_Init(void)
{
    Csm_Initialized = 1U;
    DET_LOGI(TAG, "Init ok jobs=%u", (unsigned)CSM_JOB_COUNT);
}

void Csm_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    if (!Csm_Initialized)
    {
        Det_ReportError(CSM_MODULE_ID, 0U, CSM_API_ID_GET_VERSION_INFO, CSM_E_UNINIT);
        return;
    }

    if (versioninfo == NULL)
    {
        Det_ReportError(CSM_MODULE_ID, 0U, CSM_API_ID_GET_VERSION_INFO, CSM_E_PARAM_POINTER);
        return;
    }

    versioninfo->vendorID         = CSM_VENDOR_ID;
    versioninfo->moduleID         = CSM_MODULE_ID;
    versioninfo->sw_major_version = CSM_SW_MAJOR_VERSION;
    versioninfo->sw_minor_version = CSM_SW_MINOR_VERSION;
    versioninfo->sw_patch_version = CSM_SW_PATCH_VERSION;
}

Std_ReturnType Csm_MacGenerate(uint32 jobId, Crypto_OperationModeType mode,
                                const uint8* dataPtr, uint32 dataLength,
                                uint8* macPtr, uint32* macLengthPtr)
{
    if (!Csm_Initialized)
    {
        Det_ReportError(CSM_MODULE_ID, 0U, CSM_API_ID_MAC_GENERATE, CSM_E_UNINIT);
        return E_NOT_OK;
    }

    if (dataPtr == NULL || macPtr == NULL || macLengthPtr == NULL)
    {
        Det_ReportError(CSM_MODULE_ID, 0U, CSM_API_ID_MAC_GENERATE, CSM_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    if (*macLengthPtr > CRYPTO_CMAC_SIZE)
    {
        DET_LOGE(TAG, "MacGenerate E: macLength=%u exceeds CMAC size", (unsigned)*macLengthPtr);
        Det_ReportError(CSM_MODULE_ID, 0U, CSM_API_ID_MAC_GENERATE, CSM_E_SMALL_BUFFER);
        return E_NOT_OK;
    }

    const Csm_JobConfigType* jobCfg = Csm_FindJob(jobId, CRYPTO_MACGENERATE);
    if (jobCfg == NULL)
    {
        DET_LOGW(TAG, "MacGenerate W: no matching CRYPTO_MACGENERATE job for id=%u", (unsigned)jobId);
        Det_ReportError(CSM_MODULE_ID, 0U, CSM_API_ID_MAC_GENERATE, CSM_E_PARAM_HANDLE);
        return E_NOT_OK;
    }

    Crypto_JobType job;
    job.jobId           = jobId;
    job.service          = CRYPTO_MACGENERATE;
    job.mode             = mode;
    job.cryptoKeyId      = jobCfg->CryptoKeyId;
    job.inputPtr         = dataPtr;
    job.inputLength      = dataLength;
    job.macPtr           = macPtr;
    job.macLength        = *macLengthPtr;
    job.verifyResultPtr  = NULL;

    /* [SWS_Csm_91010] 相当: CryIf 未初期化なら CryIf_ProcessJob() 自身が
     * CRYIF_E_UNINIT を報告し E_NOT_OK を返す。ここでは戻り値をそのまま返す。 */
    return CryIf_ProcessJob(CRYIF_CHANNEL_ID, &job);
}

Std_ReturnType Csm_MacVerify(uint32 jobId, Crypto_OperationModeType mode,
                              const uint8* dataPtr, uint32 dataLength,
                              const uint8* macPtr, uint32 macLength,
                              Crypto_VerifyResultType* verifyPtr)
{
    if (!Csm_Initialized)
    {
        Det_ReportError(CSM_MODULE_ID, 0U, CSM_API_ID_MAC_VERIFY, CSM_E_UNINIT);
        return E_NOT_OK;
    }

    if (dataPtr == NULL || macPtr == NULL || verifyPtr == NULL)
    {
        Det_ReportError(CSM_MODULE_ID, 0U, CSM_API_ID_MAC_VERIFY, CSM_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    /* [SWS_Csm_01050] macLength はビット単位（Csm_MacGenerate の
     * macLengthPtr がバイト単位なのと非対称。実測で確認済み）。8 の倍数で
     * ない値は本プロジェクトのバイト境界前提と矛盾するため拒否する。 */
    if ((macLength % 8U) != 0U || (macLength / 8U) > CRYPTO_CMAC_SIZE)
    {
        DET_LOGE(TAG, "MacVerify E: macLength=%u bit is invalid", (unsigned)macLength);
        Det_ReportError(CSM_MODULE_ID, 0U, CSM_API_ID_MAC_VERIFY, CSM_E_PARAM_HANDLE);
        return E_NOT_OK;
    }

    const Csm_JobConfigType* jobCfg = Csm_FindJob(jobId, CRYPTO_MACVERIFY);
    if (jobCfg == NULL)
    {
        DET_LOGW(TAG, "MacVerify W: no matching CRYPTO_MACVERIFY job for id=%u", (unsigned)jobId);
        Det_ReportError(CSM_MODULE_ID, 0U, CSM_API_ID_MAC_VERIFY, CSM_E_PARAM_HANDLE);
        return E_NOT_OK;
    }

    Crypto_JobType job;
    job.jobId           = jobId;
    job.service          = CRYPTO_MACVERIFY;
    job.mode             = mode;
    job.cryptoKeyId      = jobCfg->CryptoKeyId;
    job.inputPtr         = dataPtr;
    job.inputLength      = dataLength;
    job.macPtr           = (uint8*)macPtr;  /* MACVERIFY 経路では読み取り専用にしか使わない */
    job.macLength        = macLength / 8U; /* ビット→バイト変換（本ファイル冒頭コメント参照） */
    job.verifyResultPtr  = verifyPtr;

    return CryIf_ProcessJob(CRYIF_CHANNEL_ID, &job);
}
