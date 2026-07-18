/**
 * \file    Os_PBCfg.h
 * \brief   OS ポストビルドコンフィグ 宣言
 * \details Os_PBCfg.c で定義されたコンフィグインスタンスを外部公開する。
 *          EcuM_Init() が Os_Init(&Os_Config) に渡す。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef OS_PBCFG_H
#define OS_PBCFG_H

#include "Os.h"

extern const Os_ConfigType Os_Config;

#endif /* OS_PBCFG_H */
