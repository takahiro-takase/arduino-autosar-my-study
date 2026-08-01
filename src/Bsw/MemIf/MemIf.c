/**
 * \file    MemIf.c
 * \brief   Memory Abstraction Interface 実装 (AUTOSAR SWS_MemIf 準拠)
 * \details 本プロジェクトが対応する MCU は Renesas RA (Arduino UNO R4) のみ
 *          (AVR/UNO 無印は初代のプログラムサイズ制限により UNO R4 へ移行済み。
 *          対になる Ea モジュールと AVR 分岐は削除済み)。そのため下位ドライバは
 *          常に Fee（フラッシュエミュレーション EEPROM）1 個のみで、本ファイルは
 *          実質的に Fee_* をそのまま呼ぶだけの薄い層になる。MemIf_DeviceType/
 *          MemIf_CheckDevice() は AUTOSAR 仕様上は単一デバイス構成でも省略せず
 *          持てるため（[SWS_MemIf_00018]/[SWS_MemIf_00019]/[SWS_MemIf_00022]）、
 *          学習用に意図的に残している。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include "MemIf.h"
#include "Fee.h"
#include "Det.h"

#define TAG "MemIf"

static uint8 MemIf_CheckDevice(MemIf_DeviceType Device, uint8 ApiId)
{
    if (Device != MEMIF_DEVICE_0)
    {
        Det_ReportError(MEMIF_MODULE_ID, 0U, ApiId, MEMIF_E_PARAM_DEVICE);
        return 0U;
    }
    return 1U;
}

void MemIf_Init(void)
{
    Fee_Init();
    DET_LOGI(TAG, "Init ok");
}

void MemIf_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    if (versioninfo == NULL)
    {
        Det_ReportError(MEMIF_MODULE_ID, 0U, MEMIF_API_ID_GET_VERSION_INFO, MEMIF_E_PARAM_POINTER);
        return;
    }

    versioninfo->vendorID         = MEMIF_VENDOR_ID;
    versioninfo->moduleID         = MEMIF_MODULE_ID;
    versioninfo->sw_major_version = MEMIF_SW_MAJOR_VERSION;
    versioninfo->sw_minor_version = MEMIF_SW_MINOR_VERSION;
    versioninfo->sw_patch_version = MEMIF_SW_PATCH_VERSION;
}

Std_ReturnType MemIf_Read(MemIf_DeviceType Device, uint16 Address, uint8* DataBufferPtr, uint16 Length)
{
    if (!MemIf_CheckDevice(Device, MEMIF_API_ID_READ))
        return E_NOT_OK;
    return Fee_Read(Address, DataBufferPtr, Length);
}

Std_ReturnType MemIf_Write(MemIf_DeviceType Device, uint16 Address, const uint8* DataBufferPtr, uint16 Length)
{
    if (!MemIf_CheckDevice(Device, MEMIF_API_ID_WRITE))
        return E_NOT_OK;
    return Fee_Write(Address, DataBufferPtr, Length);
}

Std_ReturnType MemIf_WriteImmediate(MemIf_DeviceType Device, uint16 Address, const uint8* DataBufferPtr, uint16 Length)
{
    if (!MemIf_CheckDevice(Device, MEMIF_API_ID_WRITE_IMMEDIATE))
        return E_NOT_OK;
    return Fee_WriteImmediate(Address, DataBufferPtr, Length);
}

Std_ReturnType MemIf_Cancel(MemIf_DeviceType Device)
{
    if (!MemIf_CheckDevice(Device, MEMIF_API_ID_CANCEL))
        return E_NOT_OK;
    return Fee_Cancel();
}

MemIf_StatusType MemIf_GetStatus(MemIf_DeviceType Device)
{
    /* [SWS_MemIf_00022] は「Memory Abstraction Interface API の各関数」が
     * DeviceIndex を検査し MEMIF_E_PARAM_DEVICE を報告することを要求して
     * おり、GetStatus/GetJobResult も対象に含まれる（Read/Write/Cancel と
     * 同じ MemIf_CheckDevice() を通す）。 */
    if (!MemIf_CheckDevice(Device, MEMIF_API_ID_GET_STATUS))
        return MEMIF_UNINIT;
    return Fee_GetStatus();
}

MemIf_JobResultType MemIf_GetJobResult(MemIf_DeviceType Device)
{
    /* [SWS_MemIf_00043] の Return value 記述: development error 検出時は
     * （[SWS_MemIf_00022] に従いエラー報告した上で）MEMIF_JOB_FAILED を
     * 返す。MemIf_GetStatus() と同じ理由で MemIf_CheckDevice() を通す。 */
    if (!MemIf_CheckDevice(Device, MEMIF_API_ID_GET_JOB_RESULT))
        return MEMIF_JOB_FAILED;
    return Fee_GetJobResult();
}

void MemIf_MainFunction(void)
{
    Fee_MainFunction();
}
