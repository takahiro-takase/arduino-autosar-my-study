/**
 * \file    Mcu_PBCfg.h
 * \brief   MCU Driver ポストビルドコンフィグ 型定義・外部宣言
 * \details Mcu_Init() に渡すコンフィグ構造体の型定義と Mcu_Config
 *          インスタンスの外部宣言を提供する。実際の AUTOSAR 環境では
 *          コンフィギュレーションツールが生成する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef MCU_PBCFG_H
#define MCU_PBCFG_H

#include "Std_Types.h"

/**
 * \brief   Mcu ポストビルドコンフィグ型。
 * \details AUTOSAR の McuModuleConfiguration コンテナ（クロック設定・
 *          RAM セクション・電源モード等）に相当するが、本プロジェクトは
 *          クロック初期化を Arduino フレームワークに任せ、RAM セクション
 *          初期化・複数電源モードもモデル化しないため（Mcu.h 冒頭の
 *          コメント参照）、実フィールドを持たないプレースホルダとした。
 *          呼び出し規約（Mcu_Init(ConfigPtr) に NULL 禁止で渡す）だけを
 *          AUTOSAR に合わせている。
 */
typedef struct
{
    uint8 Dummy;  /**< プレースホルダ。値に意味はない。 */
} Mcu_ConfigType;

/** Mcu_PBCfg.c で定義されるポストビルドコンフィグインスタンス */
extern const Mcu_ConfigType Mcu_Config;

#endif /* MCU_PBCFG_H */
