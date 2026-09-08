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

/**
 * \brief   CryIf モジュールが初期化済みかを返す。
 *
 * \details AUTOSAR 標準の SWS_CryptoInterface には存在しない本プロジェクト
 *          独自の拡張関数（2026-09 追加）。上位層 Csm が [SWS_Csm_91010]
 *          （「CSM API が未初期化の CryIf を呼ぶ場合、操作を実行せず
 *          CSM_E_SERVICE_NOT_STARTED を DET 報告しなければならない」）を
 *          満たすために、CryIf_ProcessJob() 等を実際に呼ぶ前に状態を
 *          問い合わせる目的で新設した（Csm.c 参照）。本関数自体は前提条件を
 *          持たないため DET 報告は行わない（常に成功する単純な状態参照）。
 *
 * \return  TRUE: 初期化済み。FALSE: 未初期化。
 *
 * \ServiceID      {0x02}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
boolean CryIf_IsInitialized(void)
{
    DET_LOGT(TAG, "called");
    return (boolean)(CryIf_Initialized != 0U);
}

void CryIf_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    DET_LOGT(TAG, "called");
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
    DET_LOGT(TAG, "called");
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

Std_ReturnType CryIf_KeyElementSet(uint32 cryIfKeyId, uint32 keyElementId,
                                    const uint8* keyPtr, uint32 keyLength)
{
    DET_LOGT(TAG, "called");
    if (!CryIf_Initialized)
    {
        Det_ReportError(CRYIF_MODULE_ID, 0U, CRYIF_API_ID_KEY_ELEMENT_SET, CRYIF_E_UNINIT);
        return E_NOT_OK;
    }

    if (keyPtr == NULL)
    {
        Det_ReportError(CRYIF_MODULE_ID, 0U, CRYIF_API_ID_KEY_ELEMENT_SET, CRYIF_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    if (keyLength == 0U)
    {
        Det_ReportError(CRYIF_MODULE_ID, 0U, CRYIF_API_ID_KEY_ELEMENT_SET, CRYIF_E_PARAM_VALUE);
        return E_NOT_OK;
    }

    /* [SWS_CryIf_00055]: 単一 Crypto Driver Object へのパススルーのため、
     * cryIfKeyId/keyElementId の範囲チェックは Crypto_KeyElementSet() に委ねる
     * （CryIf_ProcessJob() が job->cryptoKeyId の範囲チェックを Crypto 側に
     * 委ねているのと同じ方針）。 */
    return Crypto_KeyElementSet(cryIfKeyId, keyElementId, keyPtr, keyLength);
}

Std_ReturnType CryIf_KeySetValid(uint32 cryIfKeyId)
{
    DET_LOGT(TAG, "called");
    if (!CryIf_Initialized)
    {
        Det_ReportError(CRYIF_MODULE_ID, 0U, CRYIF_API_ID_KEY_SET_VALID, CRYIF_E_UNINIT);
        return E_NOT_OK;
    }

    /* [SWS_CryIf_00058]: 単一 Crypto Driver Object へのパススルー。 */
    return Crypto_KeySetValid(cryIfKeyId);
}

Std_ReturnType CryIf_KeyElementGet(uint32 cryIfKeyId, uint32 keyElementId,
                                    uint8* resultPtr, uint32* resultLengthPtr)
{
    DET_LOGT(TAG, "called");
    if (!CryIf_Initialized)
    {
        Det_ReportError(CRYIF_MODULE_ID, 0U, CRYIF_API_ID_KEY_ELEMENT_GET, CRYIF_E_UNINIT);
        return E_NOT_OK;
    }

    if (resultPtr == NULL || resultLengthPtr == NULL)
    {
        Det_ReportError(CRYIF_MODULE_ID, 0U, CRYIF_API_ID_KEY_ELEMENT_GET, CRYIF_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    if (*resultLengthPtr == 0U)
    {
        Det_ReportError(CRYIF_MODULE_ID, 0U, CRYIF_API_ID_KEY_ELEMENT_GET, CRYIF_E_PARAM_VALUE);
        return E_NOT_OK;
    }

    /* [SWS_CryIf_00065]: 単一 Crypto Driver Object へのパススルーのため、
     * cryIfKeyId/keyElementId の範囲チェックは Crypto_KeyElementGet() に委ねる
     * （CryIf_KeyElementSet() と同じ方針）。 */
    return Crypto_KeyElementGet(cryIfKeyId, keyElementId, resultPtr, resultLengthPtr);
}
