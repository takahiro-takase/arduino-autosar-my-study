/**
 * \file    Adc.h
 * \brief   ADC ドライバ 公開インタフェース (AUTOSAR SWS_ADC 準拠)
 * \details MCAL ADC モジュールの公開 API を定義する。
 *          呼び出し元（IoHwAb）は本ヘッダのみをインクルードし、
 *          Adc_Hw.h / Arduino API を直接参照しない。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef ADC_H
#define ADC_H

#include "Std_Types.h"
#include "Adc_Cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   指定チャネルのアナログ生値を読み取る。
 *
 * \details Adc_Hw_ReadChannel() へ委譲し、10-bit の ADC 生値を返す。
 *          IoHwAb_MainFunction() から 10ms 周期で呼び出される。
 *
 * \param[in]  channel  アナログ入力チャネル番号（ADC_CHANNEL_* 定数）。
 * \param[out] raw      ADC 生値（0〜1023）の格納先。NULL 禁止。
 *
 * \retval  E_OK      正常読み取り。
 * \retval  E_NOT_OK  raw が NULL。
 *
 * \ServiceID      {0xD0}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType Adc_ReadChannel(uint8 channel, uint16* raw);

/**
 * \brief   ADC ドライバのバージョン情報を取得する。
 *
 * \details 本プロジェクトの Adc は Adc_Init を持たない単一チャネル即時読み取り
 *          実装（Adc_Cfg.h 冒頭コメント参照）のため、初期化状態チェックは
 *          行わず、NULL ポインタチェックのみ行う。
 *
 * \param[out]  versioninfo  バージョン情報の格納先。NULL 禁止。
 *
 * \ServiceID      {0x0A}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Adc_GetVersionInfo(Std_VersionInfoType* versioninfo);

#ifdef __cplusplus
}
#endif

#endif /* ADC_H */
