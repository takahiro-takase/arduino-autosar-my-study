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

#include "Std_Types.h"
#include "FiM_Cfg.h"

/**
 * \brief   機能 (FID) 1 件分の設定。
 *
 * \details AUTOSAR の FiMFunction コンテナを学習用に単純化したもの。
 *          実際の AUTOSAR では複数イベントの論理式 (FimEventGroup) を
 *          設定できるが、本実装は 1 FID : 1 イベントの組のみ対応する。
 */
typedef struct
{
    uint8 FunctionId;         /**< 機能 ID (FIM_FID_*)                              */
    uint8 EventId;            /**< 監視する Dem イベント ID (DEM_EVENT_*)            */
    uint8 InhibitStatusMask;  /**< Dem ステータスビット (DEM_STATUS_*)。
                                *   いずれか 1 ビットでも立っていれば当該機能を抑止する */
} FiM_FunctionCfgType;

/**
 * \brief   FiM ポストビルドコンフィグ型
 * \details FiM_Init() に渡す最上位コンフィグ構造体。FiM_PBCfg.c でインスタンス化する。
 */
typedef struct
{
    const FiM_FunctionCfgType* Functions;      /**< 機能設定配列の先頭 */
    uint8                       FunctionCount;  /**< 機能数             */
} FiM_ConfigType;

/** FiM_PBCfg.c で定義されるポストビルドコンフィグインスタンス */
extern const FiM_ConfigType FiM_Config;

#endif /* FIM_PBCFG_H */
