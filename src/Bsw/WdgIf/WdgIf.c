/**
 * \file    WdgIf.c
 * \brief   Watchdog Interface 実装 (AUTOSAR SWS_WdgIf 準拠)
 * \details 本プロジェクトが対応する MCU は Renesas RA (Arduino UNO R4) のみで、
 *          物理ウォッチドッグ（IWDT）も 1 個のみのため、Device 引数の妥当性
 *          チェック後は唯一の下位ドライバ Wdg へ実質パススルーする薄い層に
 *          なる。WdgIf_DeviceType/WdgIf_CheckDevice() は AUTOSAR 仕様上は
 *          単一デバイス構成でも省略せず持てるため（[SWS_WdgIf_00018] は
 *          「ドライバが 1 個のみの場合、Device 引数は無視してよい」と
 *          規定するが、本実装はあえてチェックを残す。MemIf.c と同じ理由:
 *          CryIf_ProcessJob() の channelId 検証と同様、複数デバイス構成
 *          という一般形の存在を読み手に示すため）学習用に意図的に残している。
 *
 *          実 AUTOSAR の WdgIf に Init/MainFunction が存在しない理由:
 *          [SWS_WdgIf_00018] のとおり、WdgM は Wdg_Init() を直接呼ぶ設計
 *          （EcuM.c 参照）。MemIf.c と異なり本モジュールは
 *          `#if defined(__AVR__)` のようなプラットフォーム分岐を隠す必要が
 *          ないため、Init 相当の非標準拡張も追加していない。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include "WdgIf.h"
#include "Wdg.h"
#include "Det.h"

#define TAG "WdgIf"

static uint8 WdgIf_CheckDevice(WdgIf_DeviceType Device, uint8 ApiId)
{
    DET_LOGT(TAG, "called");
    if (Device != WDGIF_DEVICE_0)
    {
        Det_ReportError(WDGIF_MODULE_ID, 0U, ApiId, WDGIF_E_PARAM_DEVICE);
        return 0U;
    }
    return 1U;
}

Std_ReturnType WdgIf_SetMode(WdgIf_DeviceType Device, WdgIf_ModeType WdgMode)
{
    DET_LOGT(TAG, "called");
    if (!WdgIf_CheckDevice(Device, WDGIF_API_ID_SET_MODE))
        return E_NOT_OK;
    return Wdg_SetMode(WdgMode);
}

void WdgIf_SetTriggerCondition(WdgIf_DeviceType Device, uint16 Timeout)
{
    DET_LOGT(TAG, "called");
    if (!WdgIf_CheckDevice(Device, WDGIF_API_ID_SET_TRIGGER_CONDITION))
        return;
    Wdg_SetTriggerCondition(Timeout);
}

void WdgIf_GetVersionInfo(Std_VersionInfoType* VersionInfoPtr)
{
    DET_LOGT(TAG, "called");
    if (VersionInfoPtr == NULL)
    {
        Det_ReportError(WDGIF_MODULE_ID, 0U, WDGIF_API_ID_GET_VERSION_INFO, WDGIF_E_PARAM_POINTER);
        return;
    }

    VersionInfoPtr->vendorID         = WDGIF_VENDOR_ID;
    VersionInfoPtr->moduleID         = WDGIF_MODULE_ID;
    VersionInfoPtr->sw_major_version = WDGIF_SW_MAJOR_VERSION;
    VersionInfoPtr->sw_minor_version = WDGIF_SW_MINOR_VERSION;
    VersionInfoPtr->sw_patch_version = WDGIF_SW_PATCH_VERSION;
}
