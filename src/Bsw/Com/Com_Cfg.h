/**
 * \file    Com_Cfg.h
 * \brief   通信マネージャ プリコンパイル設定 (AUTOSAR SWS_COM 準拠)
 * \details COM モジュールのプリコンパイル設定を定義する。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツール
 *          (EB tresos / Vector DaVinci 等) が生成するファイルに相当する。
 *          I-PDU 数・シグナル数のコンパイル時定数と、シグナル ID の
 *          名前付き定数を提供する。
 *
 *          シグナル ID は RTE と COM の間の「インタフェース契約」であり、
 *          このファイルを唯一の定義箇所とする。
 *          Rte.c は RTE_SIGNAL_* を独自定義せず、このファイルをインクルード
 *          して COM_SIGNAL_* を参照することで ID の重複定義を防ぐ。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef COM_CFG_H
#define COM_CFG_H

#include "Platform_Types.h"

/* -----------------------------------------------------------------------
 * プリコンパイル設定定数
 * ----------------------------------------------------------------------- */

/** RX I-PDU テーブルのエントリ数 */
#define COM_RX_IPDU_COUNT   1U

/** TX I-PDU テーブルのエントリ数 */
#define COM_TX_IPDU_COUNT   1U

/** シグナルテーブルのエントリ数（RX + TX の合計） */
#define COM_SIGNAL_COUNT    4U

/* -----------------------------------------------------------------------
 * シグナル ID 定数
 * Com_ReceiveSignal() / Com_SendSignal() の第 1 引数として使用する。
 * RTE は RTE_SIGNAL_* を独自定義せず、これらの定数を参照すること。
 * ----------------------------------------------------------------------- */

/** RX: エンジン回転数シグナル (16 bit, CAN ID 0x100, byte[0-1]) */
#define COM_SIGNAL_ENGINE_SPEED    0U

/** RX: 冷却水温シグナル (8 bit, CAN ID 0x100, byte[2]) */
#define COM_SIGNAL_COOLANT_TEMP    1U

/** RX: エンジン起動フラグシグナル (1 bit, CAN ID 0x100, byte[3] bit7) */
#define COM_SIGNAL_ENGINE_ON_FLAG  2U

/** TX: エンジン状態シグナル (8 bit, CAN ID 0x200, byte[0]) */
#define COM_SIGNAL_ENGINE_STATE    3U

#endif /* COM_CFG_H */
