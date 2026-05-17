/**
 * \file    PduR_PBCfg.h
 * \brief   PDU ルータ ポストビルド設定 宣言 (AUTOSAR SWS_PDURouter 準拠)
 * \details PduR_PBCfg.c で定義されるポストビルド設定変数 PduR_Config の
 *          extern 宣言を提供する。
 *          呼び出し側（main.cpp / EcuM 等）は PduR_PBCfg.h をインクルードし、
 *          PduR_Init(&PduR_Config) のように参照する。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツールが生成する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef PDUR_PBCFG_H
#define PDUR_PBCFG_H

#include "PduR.h"

#ifdef __cplusplus
extern "C" {
#endif

/** PduR_Init() へ渡す PDU ルータのポストビルド設定インスタンス */
extern const PduR_PBConfigType PduR_Config;

#ifdef __cplusplus
}
#endif

#endif /* PDUR_PBCFG_H */
