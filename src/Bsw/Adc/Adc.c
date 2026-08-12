/**
 * \file    Adc.c
 * \brief   ADC ドライバ 実装 (AUTOSAR SWS_ADC 準拠)
 * \details 公開 API Adc_ReadChannel() を実装する。
 *          Arduino API への直接依存を避けるため、ハードウェアアクセスは
 *          Adc_Hw_ReadChannel() へ委譲する（依存逆転）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#include "Adc.h"
#include "Adc_Hw.h"
#include "Det.h"

#define TAG "Adc"

Std_ReturnType Adc_ReadChannel(uint8 channel, uint16* raw)
{
    DET_LOGT(TAG, "called");
    if (raw == NULL) {
        Det_ReportError(ADC_MODULE_ID, 0U, ADC_API_ID_READ_CHANNEL, ADC_E_PARAM_POINTER);
        return E_NOT_OK;
    }
    *raw = Adc_Hw_ReadChannel(channel);
    return E_OK;
}

void Adc_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
    DET_LOGT(TAG, "called");
    if (versioninfo == NULL) {
        Det_ReportError(ADC_MODULE_ID, 0U, ADC_API_ID_GET_VERSION_INFO, ADC_E_PARAM_POINTER);
        return;
    }

    versioninfo->vendorID         = ADC_VENDOR_ID;
    versioninfo->moduleID         = ADC_MODULE_ID;
    versioninfo->sw_major_version = ADC_SW_MAJOR_VERSION;
    versioninfo->sw_minor_version = ADC_SW_MINOR_VERSION;
    versioninfo->sw_patch_version = ADC_SW_PATCH_VERSION;
}
