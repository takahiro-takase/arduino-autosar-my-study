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
#include "Crypto_Cfg.h"
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
    DET_LOGT(TAG, "called");
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
    DET_LOGT(TAG, "called");
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
    DET_LOGT(TAG, "called");
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
        /* [SWS_Csm_00982] の戻り値表に CRYPTO_E_SMALL_BUFFER が明記されている
         * ため、DET報告(CSM_E_SMALL_BUFFER、既存のまま変更なし)に加えて戻り値
         * 自体も拡張値で返す（2026-09 是正。以前は素の E_NOT_OK だった）。 */
        DET_LOGE(TAG, "MacGenerate E: macLength=%u exceeds CMAC size", (unsigned)*macLengthPtr);
        Det_ReportError(CSM_MODULE_ID, 0U, CSM_API_ID_MAC_GENERATE, CSM_E_SMALL_BUFFER);
        return CRYPTO_E_SMALL_BUFFER;
    }

    const Csm_JobConfigType* jobCfg = Csm_FindJob(jobId, CRYPTO_MACGENERATE);
    if (jobCfg == NULL)
    {
        DET_LOGW(TAG, "MacGenerate W: no matching CRYPTO_MACGENERATE job for id=%u", (unsigned)jobId);
        Det_ReportError(CSM_MODULE_ID, 0U, CSM_API_ID_MAC_GENERATE, CSM_E_PARAM_HANDLE);
        return E_NOT_OK;
    }

    if (!CryIf_IsInitialized())
    {
        /* [SWS_Csm_91010]: 下位層(CryIf)が未初期化な場合、操作を実行せず
         * Csm 自身が CSM_E_SERVICE_NOT_STARTED を報告しなければならない
         * （2026-09 是正。以前は CryIf_ProcessJob() 自身が報告する
         * CRYIF_E_UNINIT に委ねていたが、それは CryIf 自身の診断義務であって
         * Csm 自身の診断義務を代替しない）。 */
        DET_LOGW(TAG, "MacGenerate W: CryIf not initialized");
        Det_ReportError(CSM_MODULE_ID, 0U, CSM_API_ID_MAC_GENERATE, CSM_E_SERVICE_NOT_STARTED);
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

    const Std_ReturnType ret = CryIf_ProcessJob(CRYIF_CHANNEL_ID, &job);
    if (ret == E_OK)
    {
        /* [SWS_Csm_00982]: macLengthPtr は inout パラメータで、要求完了時に
         * 実際に書き込んだ MAC 長を書き戻す義務がある（2026-09 追加。以前は
         * 入力としてのみ扱い書き戻していなかった）。本実装は job.macLength
         * (呼び出し時の *macLengthPtr) をそのまま切り詰め長として使うため
         * （Crypto_ProcessJob() 参照）、書き戻す値は常に呼び出し時と同一に
         * なるが、契約としては明示的に書き戻す必要がある。エラー時
         * (CRYPTO_E_SMALL_BUFFER 等) は書き戻さない: 仕様本文はエラー時の
         * 扱いを明記していないが、MAC 生成自体が完了していない以上
         * job.macLength に「実際に書き込んだ長さ」としての意味が無いため。 */
        *macLengthPtr = job.macLength;
    }
    return ret;
}

Std_ReturnType Csm_MacVerify(uint32 jobId, Crypto_OperationModeType mode,
                              const uint8* dataPtr, uint32 dataLength,
                              const uint8* macPtr, uint32 macLength,
                              Crypto_VerifyResultType* verifyPtr)
{
    DET_LOGT(TAG, "called");
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

    if (!CryIf_IsInitialized())
    {
        /* [SWS_Csm_91010]（Csm_MacGenerate と同じ理由。2026-09 是正） */
        DET_LOGW(TAG, "MacVerify W: CryIf not initialized");
        Det_ReportError(CSM_MODULE_ID, 0U, CSM_API_ID_MAC_VERIFY, CSM_E_SERVICE_NOT_STARTED);
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

Std_ReturnType Csm_KeyElementSet(uint32 keyId, uint32 keyElementId,
                                  const uint8* keyPtr, uint32 keyLength)
{
    DET_LOGT(TAG, "called");
    if (!Csm_Initialized)
    {
        Det_ReportError(CSM_MODULE_ID, 0U, CSM_API_ID_KEY_ELEMENT_SET, CSM_E_UNINIT);
        return E_NOT_OK;
    }

    if (keyPtr == NULL)
    {
        Det_ReportError(CSM_MODULE_ID, 0U, CSM_API_ID_KEY_ELEMENT_SET, CSM_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    if (keyId >= CRYPTO_KEY_COUNT)
    {
        /* [SWS_Csm_91011]: keyId を持つ Csm API は、下位層（Crypto.c）に
         * 委ねず Csm 自身が範囲チェックして CSM_E_PARAM_HANDLE を報告
         * しなければならない（2026-09 是正。以前は Crypto_KeyElementSet()
         * 自身の CRYPTO_E_PARAM_HANDLE 報告に委ねていたが、それは Crypto
         * 自身の診断義務であって Csm 自身の診断義務を代替しない。
         * Csm_MacGenerate() の jobId 検証と同じ理由で、Csm_KeyElementSet()
         * には Csm 独自の鍵 ID 空間が無く CryIf/Crypto と共有しているため
         * CRYPTO_KEY_COUNT を直接参照する（Csm_PBCfg.c の CryptoKeyId
         * フィールドが既に CRYPTO_KEY_* を直接使っているのと同じ方針）。 */
        DET_LOGW(TAG, "KeyElementSet W: keyId=%u out of range", (unsigned)keyId);
        Det_ReportError(CSM_MODULE_ID, 0U, CSM_API_ID_KEY_ELEMENT_SET, CSM_E_PARAM_HANDLE);
        return E_NOT_OK;
    }

    if (!CryIf_IsInitialized())
    {
        /* [SWS_Csm_91010]（Csm_MacGenerate と同じ理由。2026-09 是正） */
        DET_LOGW(TAG, "KeyElementSet W: CryIf not initialized");
        Det_ReportError(CSM_MODULE_ID, 0U, CSM_API_ID_KEY_ELEMENT_SET, CSM_E_SERVICE_NOT_STARTED);
        return E_NOT_OK;
    }

    /* [SWS_Csm_01002]: 単一 CryIf チャネルへの実質パススルー。keyId は
     * CryIf 側の cryIfKeyId へそのまま渡す（本プロジェクトは Csm/CryIf/Crypto
     * を通じて鍵 ID 空間を分割していない）。 */
    return CryIf_KeyElementSet(keyId, keyElementId, keyPtr, keyLength);
}

Std_ReturnType Csm_KeySetValid(uint32 keyId)
{
    DET_LOGT(TAG, "called");
    if (!Csm_Initialized)
    {
        Det_ReportError(CSM_MODULE_ID, 0U, CSM_API_ID_KEY_SET_VALID, CSM_E_UNINIT);
        return E_NOT_OK;
    }

    if (keyId >= CRYPTO_KEY_COUNT)
    {
        /* [SWS_Csm_91011]（Csm_KeyElementSet と同じ理由。2026-09 是正） */
        DET_LOGW(TAG, "KeySetValid W: keyId=%u out of range", (unsigned)keyId);
        Det_ReportError(CSM_MODULE_ID, 0U, CSM_API_ID_KEY_SET_VALID, CSM_E_PARAM_HANDLE);
        return E_NOT_OK;
    }

    if (!CryIf_IsInitialized())
    {
        /* [SWS_Csm_91010]（Csm_MacGenerate と同じ理由。2026-09 是正） */
        DET_LOGW(TAG, "KeySetValid W: CryIf not initialized");
        Det_ReportError(CSM_MODULE_ID, 0U, CSM_API_ID_KEY_SET_VALID, CSM_E_SERVICE_NOT_STARTED);
        return E_NOT_OK;
    }

    /* [SWS_Csm_01003]: 単一 CryIf チャネルへの実質パススルー。 */
    return CryIf_KeySetValid(keyId);
}

Std_ReturnType Csm_KeyElementGet(uint32 keyId, uint32 keyElementId,
                                  uint8* keyPtr, uint32* keyLengthPtr)
{
    DET_LOGT(TAG, "called");
    if (!Csm_Initialized)
    {
        Det_ReportError(CSM_MODULE_ID, 0U, CSM_API_ID_KEY_ELEMENT_GET, CSM_E_UNINIT);
        return E_NOT_OK;
    }

    if (keyPtr == NULL || keyLengthPtr == NULL)
    {
        Det_ReportError(CSM_MODULE_ID, 0U, CSM_API_ID_KEY_ELEMENT_GET, CSM_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    if (keyId >= CRYPTO_KEY_COUNT)
    {
        /* [SWS_Csm_91011]（Csm_KeyElementSet と同じ理由。2026-09 是正） */
        DET_LOGW(TAG, "KeyElementGet W: keyId=%u out of range", (unsigned)keyId);
        Det_ReportError(CSM_MODULE_ID, 0U, CSM_API_ID_KEY_ELEMENT_GET, CSM_E_PARAM_HANDLE);
        return E_NOT_OK;
    }

    if (!CryIf_IsInitialized())
    {
        /* [SWS_Csm_91010]（Csm_MacGenerate と同じ理由。2026-09 是正） */
        DET_LOGW(TAG, "KeyElementGet W: CryIf not initialized");
        Det_ReportError(CSM_MODULE_ID, 0U, CSM_API_ID_KEY_ELEMENT_GET, CSM_E_SERVICE_NOT_STARTED);
        return E_NOT_OK;
    }

    /* [SWS_Csm_01004]: 単一 CryIf チャネルへの実質パススルー。keyId は
     * CryIf 側の cryIfKeyId へそのまま渡す（Csm_KeyElementSet と同じ方針）。 */
    return CryIf_KeyElementGet(keyId, keyElementId, keyPtr, keyLengthPtr);
}
