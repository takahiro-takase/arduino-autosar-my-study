/**
 * \file    NvM_PBCfg.h
 * \brief   NvM ポストビルドコンフィグ 宣言
 * \details NvM_PBCfg.c で定義されたコンフィグインスタンスを外部公開する。
 *          [SWS_NvM_00881]により NvM_Init() の ConfigPtr 引数は常に NULL
 *          （EcuM_Init() も NvM_Init(NULL) を呼ぶ）のため、NvM.c が本ヘッダ
 *          を直接 include して NvM_Config を内部参照する（2026-09 是正）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef NVM_PBCFG_H
#define NVM_PBCFG_H

#include "NvM.h"

extern const NvM_ConfigType NvM_Config;

#endif /* NVM_PBCFG_H */
