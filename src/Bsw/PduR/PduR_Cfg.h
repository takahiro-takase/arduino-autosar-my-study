/**
 * \file    PduR_Cfg.h
 * \brief   PDU ルータ プリコンパイル設定 (AUTOSAR SWS_PDURouter 準拠)
 * \details PDU ルータモジュールのプリコンパイル設定を定義する。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツール
 *          (EB tresos / Vector DaVinci 等) が生成するファイルに相当する。
 *          RX / TX ルーティングパス数をコンパイル時定数として提供し、
 *          PduR_PBCfg.c での配列サイズ指定に使用する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef PDUR_CFG_H
#define PDUR_CFG_H

#include "Platform_Types.h"

/* -----------------------------------------------------------------------
 * プリコンパイル設定定数
 * ----------------------------------------------------------------------- */

/** RX ルーティングパス数（受信 CAN フレームの配信先グループ数） */
#define PDUR_RX_PATH_COUNT   1U

/** RX パス 0 の配信先数（COM + DCM の 2 モジュールへマルチキャスト） */
#define PDUR_RX_DEST_COUNT_PATH0  2U

/** TX ルーティングパス数（送信 CAN フレームの種別数） */
#define PDUR_TX_PATH_COUNT   1U

#endif /* PDUR_CFG_H */
