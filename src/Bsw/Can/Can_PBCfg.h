/**
 * \file    Can_PBCfg.h
 * \brief   CAN ドライバ ポストビルド設定 宣言 (AUTOSAR SWS_Can 準拠)
 * \details Can_PBCfg.c で定義されるポストビルド設定変数 Can_Config の
 *          extern 宣言を提供する。
 *          呼び出し側（main.cpp / EcuM 等）は Can_PBCfg.h をインクルードし、
 *          Can_Init(&Can_Config) のように参照する。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツールが生成する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef CAN_PBCFG_H
#define CAN_PBCFG_H

#include "Can.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Can_Init() へ渡す CAN ドライバのポストビルド設定インスタンス */
extern const Can_ConfigType Can_Config;

#ifdef __cplusplus
}
#endif

#endif /* CAN_PBCFG_H */
