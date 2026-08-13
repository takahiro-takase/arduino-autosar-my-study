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
 * \brief   Adc_Init() の設定引数型（不透明型）。
 *
 * \details SWS_Adc_00365 は post-build 設定データへのポインタを要求する。
 *          本プロジェクトは単一チャネル・即時読み取りの簡略実装で
 *          post-build バリアント切替を持たないため、中身を定義しない
 *          不透明型とし、ポインタとしてのみ扱う（`KeyM_ConfigType` と
 *          同じパターン。KeyM.h 冒頭コメント参照）。
 */
typedef struct Adc_ConfigType_Tag Adc_ConfigType;

/**
 * \brief   ADC ドライバを初期化する。
 *
 * \details 本実装はハードウェア初期化状態を持たない（Adc_Hw_ReadChannel は
 *          Arduino analogRead() を都度呼ぶだけで、事前初期化を必要としない）
 *          ため、実処理は行わない。シグネチャを [SWS_Adc_00365] に合わせる
 *          ことが目的。
 *
 * \param[in]  ConfigPtr  常に NULL を渡すこと（本プロジェクトは post-build 設定を
 *                        持たないため）。
 *
 * \AUTOSARReq     {SWS_Adc_00365}
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void Adc_Init(const Adc_ConfigType* ConfigPtr);

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
 * \details 本プロジェクトの Adc_Init はハードウェア初期化状態を持たない
 *          単一チャネル即時読み取り実装（Adc_Cfg.h 冒頭コメント参照）のため、
 *          初期化状態チェックは行わず、NULL ポインタチェックのみ行う。
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
