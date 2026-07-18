/**
 * \file    FiM.h
 * \brief   機能抑止マネージャ 公開インタフェース (AUTOSAR SWS_FiM 準拠)
 * \details Dem が確定 (CONFIRMED) した DTC をもとに、関連するアプリ機能
 *          (FID: Function ID) の実行許可を判定するルールエンジン。
 *
 *          使い方:
 *            1. EcuM_Init 内で FiM_Init(&FiM_Config) を呼ぶ。
 *            2. Os スケジューラが FiM_MainFunction() を周期実行し、
 *               FiM_PBCfg.c の FiM_Functions[] に従って各 FID の許可状態を
 *               再評価する。
 *            3. ASW は Rte_Call_FiM_GetFunctionPermission() で許可状態を取得し、
 *               抑止中であれば当該機能の実行を見送る。
 *
 *          AUTOSAR との主な違い (学習用簡略化):
 *            - FimEventGroup（複数イベントの論理式）未対応。1 FID : 1 イベントのみ。
 *            - FID は監視対象イベントの CONFIRMED 状態のみで判定する
 *              （実際の AUTOSAR は任意のステータスビット組み合わせを設定可能）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef FIM_H
#define FIM_H

#include "Std_Types.h"
#include "FiM_Cfg.h"
#include "FiM_PBCfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 機能 ID 型 (FIM_FID_* 定数を渡す) */
typedef uint8 FiM_FunctionIdType;

/**
 * \brief   FiM モジュールを初期化する。
 *
 * \details 全 FID の許可状態を「許可」で初期化する
 *          (Dem の復元状態を反映するのは最初の FiM_MainFunction() まで待つ)。
 *          EcuM_Init() から、Dem_Init() の後に呼び出すこと。
 *
 * \param[in]  ConfigPtr  ポストビルドコンフィグへのポインタ。NULL 禁止。
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void FiM_Init(const FiM_ConfigType* ConfigPtr);

/**
 * \brief   FiM 周期処理。各 FID の許可状態を再評価する。
 *
 * \details FiM_Functions[] の各行について Dem_GetStatusOfEvent() を確認し、
 *          InhibitStatusMask のいずれかのビットが立っていれば該当 FID を
 *          「抑止」、立っていなければ「許可」にする。許可状態が変化した
 *          瞬間にのみログを出力する。
 *
 * \ServiceID      {0x01}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void FiM_MainFunction(void);

/**
 * \brief   指定 FID が現在許可されているかを取得する。
 *
 * \param[in]   FunctionId  機能 ID (FIM_FID_*)。
 * \param[out]  Status      1=許可 / 0=抑止 の格納先。NULL 禁止。
 *
 * \retval  E_OK      正常取得。
 * \retval  E_NOT_OK  FunctionId が範囲外、または Status が NULL
 *                     （この場合 Status には安全側のフェールセーフとして
 *                       書き込み可能であれば 0 を設定する）。
 *
 * \ServiceID      {0x02}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType FiM_GetFunctionPermission(FiM_FunctionIdType FunctionId, uint8* Status);

#ifdef __cplusplus
}
#endif

#endif /* FIM_H */
