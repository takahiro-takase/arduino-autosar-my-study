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
 *          DaVinci 対応:
 *            本ファイルは /ActiveEcuC/Com/ComConfig の
 *            ComIPdu / ComSignal ノード数に対応する。
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
 * DaVinci: /ActiveEcuC/Com/ComConfig/ 配下の ComIPdu ノード数に相当
 * ----------------------------------------------------------------------- */

/** RX I-PDU テーブルのエントリ数
 *  DaVinci: /ActiveEcuC/Com/ComConfig/ 内 Direction=RECEIVE の ComIPdu 数 */
#define COM_RX_IPDU_COUNT   2U  /* [0]=EngineInfo 0x100, [1]=AbsInfo 0x110 */

/** TX I-PDU テーブルのエントリ数
 *  DaVinci: /ActiveEcuC/Com/ComConfig/ 内 Direction=SEND の ComIPdu 数 */
#define COM_TX_IPDU_COUNT   1U  /* [0]=MeterStatus 0x200 */

/** シグナルテーブルのエントリ数（RX + TX の合計）
 *  DaVinci: /ActiveEcuC/Com/ComConfig/ 内 ComSignal ノード数の合計 */
#define COM_SIGNAL_COUNT    7U

/* -----------------------------------------------------------------------
 * シグナル ID 定数
 * DaVinci: /ActiveEcuC/Com/ComConfig/[ComSignal]/ComHandleId に相当
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

/* -----------------------------------------------------------------------
 * ABS ECU シグナル (CAN ID 0x110 AbsInfo フレーム)
 * ----------------------------------------------------------------------- */

/** RX: 車速シグナル (16 bit, CAN ID 0x110, byte[0-1], 0.01 km/h) */
#define COM_SIGNAL_VEHICLE_SPEED   4U

/** RX: ブレーキ作動フラグ (1 bit, CAN ID 0x110, byte[2] bit7, 0=解除/1=作動) */
#define COM_SIGNAL_BRAKE_ACTIVE    5U

/** RX: ABS 作動フラグ (1 bit, CAN ID 0x110, byte[2] bit6, 0=非作動/1=作動) */
#define COM_SIGNAL_ABS_ACTIVE      6U

#endif /* COM_CFG_H */
