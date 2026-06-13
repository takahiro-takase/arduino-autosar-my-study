/**
 * \file    CanIf_PBCfg.h
 * \brief   CAN インタフェース ポストビルド設定 宣言 (AUTOSAR SWS_CANInterface 準拠)
 * \details CanIf_PBCfg.c で定義されるポストビルド設定変数 CanIf_Config の
 *          extern 宣言を提供する。
 *          呼び出し側（main.cpp / EcuM 等）は CanIf_PBCfg.h をインクルードし、
 *          CanIf_Init(&CanIf_Config) のように参照する。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツールが生成する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef CANIF_PBCFG_H
#define CANIF_PBCFG_H

#include "CanIf.h"

#ifdef __cplusplus
extern "C" {
#endif

/** CanIf_Init() へ渡す CAN インタフェースのポストビルド設定インスタンス */
extern const CanIf_ConfigType CanIf_Config;

#ifdef __cplusplus
}
#endif

#endif /* CANIF_PBCFG_H */
