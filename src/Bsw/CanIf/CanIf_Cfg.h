/**
 * \file    CanIf_Cfg.h
 * \brief   CAN インタフェース プリコンパイル設定 (AUTOSAR SWS_CANInterface 準拠)
 * \details CAN インタフェースモジュールのプリコンパイル設定を定義する。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツール
 *          (EB tresos / Vector DaVinci 等) が生成するファイルに相当する。
 *          TX / RX PDU テーブルのエントリ数をコンパイル時定数として提供し、
 *          CanIf_PBCfg.c での配列サイズ指定に使用する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef CANIF_CFG_H
#define CANIF_CFG_H

#include "Platform_Types.h"

/* -----------------------------------------------------------------------
 * プリコンパイル設定定数
 * ----------------------------------------------------------------------- */

/** TX PDU テーブルのエントリ数（送信 CAN フレーム種別数） */
#define CANIF_TX_PDU_COUNT  1U

/** RX PDU テーブルのエントリ数（受信 CAN フレーム種別数） */
#define CANIF_RX_PDU_COUNT  1U

#endif /* CANIF_CFG_H */
