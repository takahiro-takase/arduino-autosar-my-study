/**
 * \file    FiM_PBCfg.h
 * \brief   機能抑止マネージャ ポストビルドコンフィグ 型定義・外部宣言
 * \details FiM_Init() に渡すコンフィグ構造体の型定義と
 *          FiM_Config インスタンスの外部宣言を提供する。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツールが生成する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef FIM_PBCFG_H
#define FIM_PBCFG_H

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Std_Types.h"
#include "FiM_Cfg.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

/* ======================================================================
 * Type Definitions
 * ====================================================================== */

/**
 * \brief   機能 (FID) の抑止の条件 1 件分の設定（FiMInhibitionConfiguration に相当）。
 *
 * \details 1 つの FID に、抑止の条件を複数持てる（同じ FunctionId の行を複数並べる）。
 *          どれか 1 つの条件が成り立てば、その FID を抑止する（OR、[SWS_Fim_00064]）。
 *          簡略化: 実仕様の FiMSummaryEvent（複数のイベントをまとめた名前）は持たず、同じ FID に、
 *          イベントごとの行を並べる。
 */
typedef struct
{
    uint8 FunctionId;         /**< 機能 ID (FIM_FID_*)                              */
    uint8 EventId;            /**< 監視する Dem イベント ID (DemConf_DemEventParameter_*)            */
    uint8 InhibitStatusMask;  /**< Dem ステータスビット (DEM_STATUS_*)。
                                *   いずれか 1 ビットでも立っていれば当該機能を抑止する */
} FiM_FunctionCfgType;

/**
 * \brief   FiM ポストビルドコンフィグ型
 * \details FiM_Init() に渡す最上位コンフィグ構造体。FiM_PBCfg.c でインスタンス化する。
 */
typedef struct
{
    const FiM_FunctionCfgType* Functions;        /**< 抑止の条件の配列の先頭（1 行が 1 つの条件） */
    uint8                       InhibitionCount;  /**< 抑止の条件の数（Functions[] の行数）         */
    uint8                       FunctionCount;    /**< 機能（FID）の数。FunctionId は 0 〜 この値 - 1 */
} FiM_ConfigType;

/* ======================================================================
 * Global Variables
 * ====================================================================== */

/** FiM_PBCfg.c で定義されるポストビルドコンフィグインスタンス */
extern const FiM_ConfigType FiM_Config;

#endif /* FIM_PBCFG_H */
