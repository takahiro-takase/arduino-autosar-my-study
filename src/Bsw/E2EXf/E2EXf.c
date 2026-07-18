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
    E2EXf_Initialized = 1U;
}

Std_ReturnType E2EXf_InverseTransform(const E2EXf_RxConfigType* Config, const uint8* Buffer, uint8 Length,
                                      E2E_P01StatusType* CheckStatus)
{
    if (CheckStatus == NULL)
        return E_NOT_OK;

    if (!E2EXf_Initialized || Config == NULL || Buffer == NULL)
    {
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

void E2EXf_Transform(const E2EXf_TxConfigType* Config, uint8* Buffer, uint8 Length)
{
    if (!E2EXf_Initialized)
        return;

    if (Config == NULL || Buffer == NULL)
        return;

    E2E_P01Protect(Config->E2EConfig, Config->ProtectState, Buffer, Length);
}
