/**
 * \file    KeyM.c
 * \brief   Key Manager 実装 (AUTOSAR SWS_KeyManager 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.4.0 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include "KeyM.h"
#include "KeyM_PBCfg.h"
#include "Csm.h"
#include "Crypto_Cfg.h"
#include "Det.h"

#define TAG "KeyM"

static uint8 KeyM_Initialized = 0U;

/** [SWS_KeyM_00003]/[SWS_KeyM_00004]: KeyM_Start() で開き、KeyM_Finalize() で
 *  閉じるセッション状態。開いている間のみ KeyM_Update() を受理する。 */
static uint8 KeyM_SessionOpen = 0U;

/** セッション中に KeyM_Update() で更新され、まだ Csm_KeySetValid() されて
 *  いない鍵のマーカー（KeyM_CryptoKeyConfigData の添字と対応）。
 *  [SWS_KeyM_00016]/[SWS_KeyM_00103] 相当。 */
static uint8 KeyM_PendingValidate[KEYM_CRYPTO_KEY_COUNT];

static const KeyM_CryptoKeyConfigType* KeyM_FindKeyByName(const uint8* keyNamePtr, uint16 keyNameLength)
{
    DET_LOGT(TAG, "called");
    if (keyNameLength != 1U)
        return NULL;

    for (uint8 i = 0U; i < KEYM_CRYPTO_KEY_COUNT; i++)
    {
        if (KeyM_CryptoKeyConfigData[i].KeyName == keyNamePtr[0])
            return &KeyM_CryptoKeyConfigData[i];
    }
    return NULL;
}

void KeyM_Init(const KeyM_ConfigType* ConfigPtr)
{
    DET_LOGT(TAG, "called");
    (void)ConfigPtr; /* [SWS_KeyM_00158]: 常に NULL_PTR の想定。内容は使わない。 */

    KeyM_SessionOpen = 0U;
    for (uint8 i = 0U; i < KEYM_CRYPTO_KEY_COUNT; i++)
        KeyM_PendingValidate[i] = 0U;

    KeyM_Initialized = 1U;
    DET_LOGI(TAG, "Init ok keys=%u", (unsigned)KEYM_CRYPTO_KEY_COUNT);
}

void KeyM_Deinit(void)
{
    DET_LOGT(TAG, "called");
    /* [SWS_KeyM_00048]: 実車では RAM 上の鍵材料を消去するが、本プロジェクトの
     * 簡略化では KeyM は鍵材料そのものを保持しない（Crypto 層が保持）ため、
     * ここではセッション状態のリセットのみ行う（スコープ外、本ファイル冒頭
     * コメント参照）。 */
    KeyM_SessionOpen = 0U;
    for (uint8 i = 0U; i < KEYM_CRYPTO_KEY_COUNT; i++)
        KeyM_PendingValidate[i] = 0U;

    KeyM_Initialized = 0U;
    DET_LOGI(TAG, "Deinit ok");
}

void KeyM_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    DET_LOGT(TAG, "called");
    if (!KeyM_Initialized)
    {
        Det_ReportError(KEYM_MODULE_ID, 0U, KEYM_API_ID_GET_VERSION_INFO, KEYM_E_UNINIT);
        return;
    }

    if (versioninfo == NULL)
    {
        Det_ReportError(KEYM_MODULE_ID, 0U, KEYM_API_ID_GET_VERSION_INFO, KEYM_E_PARAM_POINTER);
        return;
    }

    versioninfo->vendorID         = KEYM_VENDOR_ID;
    versioninfo->moduleID         = KEYM_MODULE_ID;
    versioninfo->sw_major_version = KEYM_SW_MAJOR_VERSION;
    versioninfo->sw_minor_version = KEYM_SW_MINOR_VERSION;
    versioninfo->sw_patch_version = KEYM_SW_PATCH_VERSION;
}

Std_ReturnType KeyM_Start(KeyM_StartType StartType,
                          const uint8* RequestData, uint16 RequestDataLength,
                          uint8* ResponseData, uint16* ResponseDataLength)
{
    DET_LOGT(TAG, "called");
    (void)RequestData;
    (void)RequestDataLength;
    (void)ResponseData;
    (void)ResponseDataLength;

    if (!KeyM_Initialized)
    {
        Det_ReportError(KEYM_MODULE_ID, 0U, KEYM_API_ID_START, KEYM_E_UNINIT);
        return E_NOT_OK;
    }

    /* [SWS_KeyM_00086]: 既にセッションが開いていても E_OK を返し継続する。 */
    KeyM_SessionOpen = 1U;
    DET_LOGI(TAG, "Start ok mode=%u", (unsigned)StartType);
    return E_OK;
}

Std_ReturnType KeyM_Update(const uint8* KeyNamePtr, uint16 KeyNameLength,
                           const uint8* RequestDataPtr, uint16 RequestDataLength,
                           uint8* ResultDataPtr, uint16 ResultDataMaxLength)
{
    DET_LOGT(TAG, "called");
    (void)ResultDataPtr;
    (void)ResultDataMaxLength;

    if (!KeyM_Initialized)
    {
        Det_ReportError(KEYM_MODULE_ID, 0U, KEYM_API_ID_UPDATE, KEYM_E_UNINIT);
        return E_NOT_OK;
    }

    if (KeyNamePtr == NULL || RequestDataPtr == NULL)
    {
        Det_ReportError(KEYM_MODULE_ID, 0U, KEYM_API_ID_UPDATE, KEYM_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    /* [SWS_KeyM_00003]: セッション未開始中の鍵更新は拒否する。仕様に対応する
     * 専用 DET コードが定義されていないため、警告ログのみで E_NOT_OK を返す。 */
    if (!KeyM_SessionOpen)
    {
        DET_LOGW(TAG, "Update W: no active session (call KeyM_Start() first)");
        return E_NOT_OK;
    }

    /* [SWS_KeyM_00154] の SHE M1M2M3 経路（KeyNameLength=0）は未対応
     * （本ファイル冒頭コメント参照）。 */
    const KeyM_CryptoKeyConfigType* keyCfg = KeyM_FindKeyByName(KeyNamePtr, KeyNameLength);
    if (keyCfg == NULL)
    {
        /* [SWS_KeyM_00013]: 「見つからなければ E_NOT_OK」。専用 DET コードは
         * 定義されていない。 */
        DET_LOGW(TAG, "Update W: unknown key name");
        return E_NOT_OK;
    }

    /* [SWS_KeyM_00016]: KEYM_STORED_KEY 方式。鍵要素 id は 1 固定。 */
    Std_ReturnType ret = Csm_KeyElementSet(keyCfg->CsmKeyTargetRef, CRYPTO_KEY_ELEMENT_ID_CIPHER_KEY,
                                           RequestDataPtr, RequestDataLength);
    if (ret != E_OK)
    {
        DET_LOGW(TAG, "Update W: Csm_KeyElementSet failed for keyName=0x%02X", (unsigned)KeyNamePtr[0]);
        return E_NOT_OK;
    }

    for (uint8 i = 0U; i < KEYM_CRYPTO_KEY_COUNT; i++)
    {
        if (&KeyM_CryptoKeyConfigData[i] == keyCfg)
        {
            KeyM_PendingValidate[i] = 1U;
            break;
        }
    }

    DET_LOGI(TAG, "Update ok keyName=0x%02X (pending Finalize)", (unsigned)KeyNamePtr[0]);
    return E_OK;
}

Std_ReturnType KeyM_Finalize(const uint8* RequestDataPtr, uint16 RequestDataLength,
                             uint8* ResponseDataPtr, uint16 ResponseMaxDataLength)
{
    DET_LOGT(TAG, "called");
    (void)RequestDataPtr;
    (void)RequestDataLength;
    (void)ResponseDataPtr;
    (void)ResponseMaxDataLength;

    if (!KeyM_Initialized)
    {
        Det_ReportError(KEYM_MODULE_ID, 0U, KEYM_API_ID_FINALIZE, KEYM_E_UNINIT);
        return E_NOT_OK;
    }

    if (!KeyM_SessionOpen)
    {
        DET_LOGW(TAG, "Finalize W: no active session (call KeyM_Start() first)");
        return E_NOT_OK;
    }

    /* [SWS_KeyM_00103]: 1件失敗しても残りの鍵の有効化は継続する。 */
    uint8 allOk = 1U;
    for (uint8 i = 0U; i < KEYM_CRYPTO_KEY_COUNT; i++)
    {
        if (!KeyM_PendingValidate[i])
            continue;

        if (Csm_KeySetValid(KeyM_CryptoKeyConfigData[i].CsmKeyTargetRef) != E_OK)
        {
            DET_LOGW(TAG, "Finalize W: Csm_KeySetValid failed for keyName=0x%02X",
                     (unsigned)KeyM_CryptoKeyConfigData[i].KeyName);
            allOk = 0U;
        }
        KeyM_PendingValidate[i] = 0U;
    }

    /* [SWS_KeyM_00106]: 成否に関わらずセッションを閉じる。 */
    KeyM_SessionOpen = 0U;

    DET_LOGI(TAG, "Finalize %s", allOk ? "ok" : "done with failures");
    return allOk ? E_OK : E_NOT_OK;
}
