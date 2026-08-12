/**
 * \file    E2EXf.c
 * \brief   E2E Transformer 実装 (AUTOSAR SWS_E2ELibrary 12.4 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1/4.2.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include "E2EXf.h"
#include "Det.h"

#define TAG "E2EXf"

/* E2EXf モジュール自身の初期化状態（SWS_E2EXf_00130 準拠）。
 * E2E_P01CheckStateType/E2E_P01ProtectStateType（下位の Profile 層）の
 * 初期化とは別に、Transformer 層自身が「E2EXf_Init() が呼ばれたか」を
 * 保持する必要がある（SWS_E2EXf_00133/00151）。EcuM_Init() の呼び出し
 * 順序が将来変わり、E2EXf_PBCfg_Init() より前にフレーム受信経路が
 * 有効になってしまった場合でも、初期化前の State（WaitForFirstData=0
 * の未初期化 BSS のまま）を使って誤判定することを防ぐ。
 * 本プロジェクトの他 BSW モジュール（Com_ConfigPtr 等）と同じ
 * 「未初期化アクセスを防ぐ」方針に合わせている。 */
static uint8 E2EXf_Initialized = 0U;

void E2EXf_Init(void)
{
    DET_LOGT(TAG, "called");
    E2EXf_Initialized = 1U;
}

void E2EXf_DeInit(void)
{
    DET_LOGT(TAG, "called");
    if (!E2EXf_Initialized)
    {
        Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_DEINIT, E2EXF_E_UNINIT);
        return;
    }

    E2EXf_Initialized = 0U;

    DET_LOGI(TAG, "DeInit ok");
}

Std_ReturnType E2EXf_InverseTransform(const E2EXf_RxConfigType* Config, const uint8* Buffer, uint8 Length,
                                      E2E_P01StatusType* CheckStatus)
{
    DET_LOGT(TAG, "called");
    if (CheckStatus == NULL)
    {
        Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_INVERSE_TRANSFORM, E2EXF_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    if (!E2EXf_Initialized)
    {
        Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_INVERSE_TRANSFORM, E2EXF_E_UNINIT);
        *CheckStatus = E2E_P01STATUS_ERROR;
        return E_NOT_OK;
    }

    if (Config == NULL || Buffer == NULL)
    {
        Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_INVERSE_TRANSFORM, E2EXF_E_PARAM_POINTER);
        *CheckStatus = E2E_P01STATUS_ERROR;
        return E_NOT_OK;
    }

    const E2E_P01StatusType status = E2E_P01Check(Config->E2EConfig, Config->CheckState, Buffer, Length);
    *CheckStatus = status;

    const uint8 acceptable =
        (status == E2E_P01STATUS_OK)
        || (status == E2E_P01STATUS_OKSOMELOST)
        || (status == E2E_P01STATUS_SYNC)
        || (status == E2E_P01STATUS_INITIAL);

    if (!acceptable)
        DET_LOGW(TAG, "InverseTransform NG DemEvent=%u st=%u", (unsigned)Config->DemEventId, (unsigned)status);

    Dem_ReportErrorStatus(Config->DemEventId, acceptable ? DEM_EVENT_STATUS_PASSED : DEM_EVENT_STATUS_FAILED);

    return acceptable ? E_OK : E_NOT_OK;
}

Std_ReturnType E2EXf_InverseTransformP05(const E2EXf_RxConfigTypeP05* Config, const uint8* Buffer, uint8 Length,
                                          E2E_P05StatusType* CheckStatus)
{
    DET_LOGT(TAG, "called");
    if (CheckStatus == NULL)
    {
        Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_INVERSE_TRANSFORM, E2EXF_E_PARAM_POINTER);
        return E_NOT_OK;
    }

    if (!E2EXf_Initialized)
    {
        Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_INVERSE_TRANSFORM, E2EXF_E_UNINIT);
        *CheckStatus = E2E_P05STATUS_ERROR;
        return E_NOT_OK;
    }

    if (Config == NULL || Buffer == NULL)
    {
        Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_INVERSE_TRANSFORM, E2EXF_E_PARAM_POINTER);
        *CheckStatus = E2E_P05STATUS_ERROR;
        return E_NOT_OK;
    }

    E2E_P05StatusType status = E2E_P05Check(Config->E2EConfig, Config->CheckState, Buffer, Length);

    /* Profile05にはProfile01のWaitForFirstData/INITIAL相当の初回受信の特別扱いが
     * 無い(E2E_P05.c は仕様に忠実な実装として意図的にこれを持たない)。しかし
     * 実運用では起動直後、送信元ECUが既に稼働中でCounterが0以外から始まっている
     * ことが十分あり得るため、そのまま繋ぐと最初のフレーム(CRCは正しい)が
     * REPEATED/WRONGSEQUENCEと誤判定され、DEM_DEBOUNCE_LIMIT=1の設定と相まって
     * 即座に誤ったDTCが確定してしまう。CRCさえ正しければ「通信路そのものは
     * 正常」と判断し、最初の1回に限りOKへ格上げする(E2E_P05Check()側は既に
     * 内部でCounterを受信値へ同期済みのため、2回目以降は通常のdelta判定に
     * 自然に戻る)。EngineHealthStatus 用など WaitForFirstData が NULL の
     * インスタンスにはこの特別扱いを適用しない。 */
    if ((Config->WaitForFirstData != NULL) && (*Config->WaitForFirstData != 0U) && (status != E2E_P05STATUS_ERROR))
    {
        status = E2E_P05STATUS_OK;
        *Config->WaitForFirstData = 0U;
    }

    *CheckStatus = status;

    const uint8 acceptable = (status == E2E_P05STATUS_OK) || (status == E2E_P05STATUS_OKSOMELOST);

    if (!acceptable)
        DET_LOGW(TAG, "InverseTransformP05 NG DemEvent=%u st=%u", (unsigned)Config->DemEventId, (unsigned)status);

    Dem_ReportErrorStatus(Config->DemEventId, acceptable ? DEM_EVENT_STATUS_PASSED : DEM_EVENT_STATUS_FAILED);

    return acceptable ? E_OK : E_NOT_OK;
}

void E2EXf_Transform(const E2EXf_TxConfigType* Config, uint8* Buffer, uint8 Length)
{
    DET_LOGT(TAG, "called");
    if (!E2EXf_Initialized)
    {
        Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_TRANSFORM, E2EXF_E_UNINIT);
        return;
    }

    if (Config == NULL || Buffer == NULL)
    {
        Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_TRANSFORM, E2EXF_E_PARAM_POINTER);
        return;
    }

    E2E_P01Protect(Config->E2EConfig, Config->ProtectState, Buffer, Length);
}

void E2EXf_TransformP05(const E2EXf_TxConfigTypeP05* Config, uint8* Buffer, uint8 Length)
{
    DET_LOGT(TAG, "called");
    if (!E2EXf_Initialized)
    {
        Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_TRANSFORM, E2EXF_E_UNINIT);
        return;
    }

    if (Config == NULL || Buffer == NULL)
    {
        Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_TRANSFORM, E2EXF_E_PARAM_POINTER);
        return;
    }

    E2E_P05Protect(Config->E2EConfig, Config->ProtectState, Buffer, Length);
}

void E2EXf_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    DET_LOGT(TAG, "called");
    if (versioninfo == NULL)
    {
        Det_ReportError(E2EXF_MODULE_ID, 0U, E2EXF_API_ID_GET_VERSION_INFO, E2EXF_E_PARAM_POINTER);
        return;
    }

    versioninfo->vendorID         = E2EXF_VENDOR_ID;
    versioninfo->moduleID         = E2EXF_MODULE_ID;
    versioninfo->sw_major_version = E2EXF_SW_MAJOR_VERSION;
    versioninfo->sw_minor_version = E2EXF_SW_MINOR_VERSION;
    versioninfo->sw_patch_version = E2EXF_SW_PATCH_VERSION;
}
