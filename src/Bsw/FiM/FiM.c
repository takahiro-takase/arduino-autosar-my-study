/**
 * \file    FiM.c
 * \brief   機能抑止マネージャ 実装 (AUTOSAR SWS_FiM 準拠)
 * \details Dem が確定した DTC のステータスをもとに、関連するアプリ機能 (FID)
 *          の実行許可を判定する。
 *
 *          判定アルゴリズム:
 *            1. FiM_MainFunction() (100 ms 周期) が FiM_Functions[] を
 *               先頭から走査する。
 *            2. 各行について Dem_GetStatusOfEvent(EventId) を取得し、
 *               InhibitStatusMask とのビット AND が非ゼロなら抑止、
 *               ゼロなら許可と判定する。
 *            3. 判定結果が前回から変化した場合のみログを出力する
 *               (毎サイクルのログ過多を防ぐ、Can_Hw_IsBusOff と同様の方式)。
 *
 *          ASW は Rte_Call_FiM_GetFunctionPermission() 経由で許可状態を取得する。
 *          BSW モジュール同士（FiM → Dem）は RTE を介さず直接呼び出す
 *          （本プロジェクトの確立された層分離: ASW↔BSW 境界のみ RTE が仲介する）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "FiM.h"
#include "Dem.h"
#include "Det.h"

#define TAG "FiM"

/** ポストビルドコンフィグへのポインタ (FiM_Init で設定) */
static const FiM_ConfigType* FiM_Cfg = NULL;

/** FID ごとの現在の許可状態 (1=許可, 0=抑止) */
static uint8 FiM_Permitted[FIM_FUNCTION_COUNT];

/**
 * \brief   FiM モジュールを初期化する。
 *
 * \details 全 FID を「許可」で初期化する。実際の Dem 状態の反映は
 *          最初の FiM_MainFunction() 呼び出しまで行われない
 *          (起動直後の一瞬だけ、確定済み DTC があっても許可状態になる学習用簡略化)。
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void FiM_Init(const FiM_ConfigType* ConfigPtr)
{
    FiM_Cfg = ConfigPtr;

    for (uint8 i = 0U; i < ConfigPtr->FunctionCount; i++)
    {
        FiM_Permitted[i] = 1U;
    }

    DET_LOGI(TAG, "Init ok functions=%u", (unsigned)ConfigPtr->FunctionCount);
}

/**
 * \brief   FiM 周期処理。各 FID の許可状態を再評価する。
 *
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void FiM_MainFunction(void)
{
    if (FiM_Cfg == NULL)
        return;

    for (uint8 i = 0U; i < FiM_Cfg->FunctionCount; i++)
    {
        const FiM_FunctionCfgType* fn = &FiM_Cfg->Functions[i];
        const uint8 status        = Dem_GetStatusOfEvent(fn->EventId);
        const uint8 newPermitted  = ((status & fn->InhibitStatusMask) != 0U) ? 0U : 1U;

        if (newPermitted != FiM_Permitted[fn->FunctionId])
        {
            FiM_Permitted[fn->FunctionId] = newPermitted;

            if (newPermitted == 0U)
            {
                DET_LOGW(TAG, "FID%u inhibited (ev=%u status=0x%02X)",
                         (unsigned)fn->FunctionId, (unsigned)fn->EventId, (unsigned)status);
            }
            else
            {
                DET_LOGI(TAG, "FID%u permitted again", (unsigned)fn->FunctionId);
            }
        }
    }
}

/**
 * \brief   指定 FID が現在許可されているかを取得する。
 *
 * \ServiceID      {0x02}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType FiM_GetFunctionPermission(FiM_FunctionIdType FunctionId, uint8* Status)
{
    if (Status == NULL)
        return E_NOT_OK;

    if (FiM_Cfg == NULL || FunctionId >= FiM_Cfg->FunctionCount)
    {
        *Status = 0U;  /* フェールセーフ: 不明な FID は抑止扱いとする */
        return E_NOT_OK;
    }

    *Status = FiM_Permitted[FunctionId];
    return E_OK;
}
