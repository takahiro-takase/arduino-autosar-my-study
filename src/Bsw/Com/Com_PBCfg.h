/**
 * \file    Com_PBCfg.h
 * \brief   通信マネージャ ポストビルド設定 宣言 (AUTOSAR SWS_COM 準拠)
 * \details Com_PBCfg.c で定義されるポストビルド設定変数 Com_Config の
 *          extern 宣言を提供する。
 *          呼び出し側（main.cpp / EcuM 等）は Com_PBCfg.h をインクルードし、
 *          Com_Init(&Com_Config) のように参照する。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツールが生成する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef COM_PBCFG_H
#define COM_PBCFG_H

#include "Com.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Com_Init() へ渡す COM モジュールのポストビルド設定インスタンス */
extern const Com_ConfigType Com_Config;

#ifdef __cplusplus
}
#endif

#endif /* COM_PBCFG_H */
