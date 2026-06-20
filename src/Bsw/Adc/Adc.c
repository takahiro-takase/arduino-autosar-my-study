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

Std_ReturnType Adc_ReadChannel(uint8 channel, uint16* raw)
{
    if (raw == NULL) {
        return E_NOT_OK;
    }
    *raw = Adc_Hw_ReadChannel(channel);
    return E_OK;
}
