/**
 * \file    Wdg.c
 * \brief   Watchdog Driver 実装 (AUTOSAR SWS_Wdg 準拠)
 * \details 実際の HW ウォッチドッグ（Renesas RA WDT ライブラリ）への
 *          Enable/Disable/Refresh は Wdg_Hw 層に委譲し、本ファイルは
 *          MCU 固有のヘッダを直接知らない（旧 WdgM_Hw と同じ境界の引き方。
 *          詳細は Wdg_Hw.h 参照）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include "Wdg.h"
#include "Wdg_Hw.h"
#include "Det.h"

#define TAG "Wdg"

static uint8 Wdg_Initialized = 0U;
static uint16 Wdg_ConfiguredTimeoutMs = 0U;

void Wdg_Init(const Wdg_ConfigType* ConfigPtr)
{
    DET_LOGT(TAG, "called");
    if (ConfigPtr == NULL)
    {
        DET_LOGE(TAG, "Init: NULL ConfigPtr");
        Det_ReportError(WDG_MODULE_ID, 0U, WDG_API_ID_INIT, WDG_E_PARAM_POINTER);
        return;
    }

    Wdg_ConfiguredTimeoutMs = ConfigPtr->DefaultTimeoutMs;
    Wdg_Initialized         = 1U;

    DET_LOGI(TAG, "Init ok timeout=%ums", (unsigned)Wdg_ConfiguredTimeoutMs);
}

Std_ReturnType Wdg_SetMode(WdgIf_ModeType Mode)
{
    DET_LOGT(TAG, "called");
    if (!Wdg_Initialized)
    {
        Det_ReportError(WDG_MODULE_ID, 0U, WDG_API_ID_SET_MODE, WDG_E_DRIVER_STATE);
        return E_NOT_OK;
    }

    switch (Mode)
    {
    case WDGIF_FAST_MODE:
        Wdg_Hw_Enable(Wdg_ConfiguredTimeoutMs);
        return E_OK;

    case WDGIF_OFF_MODE:
        /* Renesas RA4M1 の IWDT は一度有効化すると FSP からの無効化手段が
         * ないため（Wdg_Hw.cpp 参照）、この要求は物理的に受理できない。
         * 実 AUTOSAR の拡張プロダクションエラー WDG_E_DISABLE_REJECTED に
         * 相当する状況だが、本プロジェクトはプロダクションエラーの仕組み
         * 自体を持たないため DET_LOGW のみで通知し、開発エラーとしては
         * 報告しない（想定内の正常な拒否のため。Wdg.h 冒頭のコメント参照）。 */
        Wdg_Hw_Disable();
        DET_LOGW(TAG, "SetMode(OFF) rejected - HW cannot be disabled once armed");
        return E_NOT_OK;

    case WDGIF_SLOW_MODE:
    default:
        Det_ReportError(WDG_MODULE_ID, 0U, WDG_API_ID_SET_MODE, WDG_E_PARAM_MODE);
        return E_NOT_OK;
    }
}

void Wdg_SetTriggerCondition(uint16 timeout)
{
    DET_LOGT(TAG, "called");
    if (!Wdg_Initialized)
    {
        Det_ReportError(WDG_MODULE_ID, 0U, WDG_API_ID_SET_TRIGGER_CONDITION, WDG_E_DRIVER_STATE);
        return;
    }

    if (timeout > Wdg_ConfiguredTimeoutMs)
    {
        Det_ReportError(WDG_MODULE_ID, 0U, WDG_API_ID_SET_TRIGGER_CONDITION, WDG_E_PARAM_TIMEOUT);
        return;
    }

    /* 本プロジェクトの HW は API 経由でのタイムアウト窓の動的変更に対応
     * しないため、timeout の値によらずリフレッシュのみ行う（Wdg.h 冒頭の
     * コメント参照）。現在のモードの確認も行わない（WdgM_TriggerHwWatchdog()
     * は WdgM_SupervisionSuppressed 中も含め常にリフレッシュを要求し続ける
     * 設計のため、Wdg 側で再度モードを判定する必要はない）。 */
    Wdg_Hw_Refresh();
}

void Wdg_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    DET_LOGT(TAG, "called");
    if (versioninfo == NULL)
    {
        Det_ReportError(WDG_MODULE_ID, 0U, WDG_API_ID_GET_VERSION_INFO, WDG_E_PARAM_POINTER);
        return;
    }

    versioninfo->vendorID         = WDG_VENDOR_ID;
    versioninfo->moduleID         = WDG_MODULE_ID;
    versioninfo->sw_major_version = WDG_SW_MAJOR_VERSION;
    versioninfo->sw_minor_version = WDG_SW_MINOR_VERSION;
    versioninfo->sw_patch_version = WDG_SW_PATCH_VERSION;
}
