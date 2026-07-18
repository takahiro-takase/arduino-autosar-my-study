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

/* -----------------------------------------------------------------------
 * 受信デッドライン監視（タイムアウト）設定
 * DaVinci: /ActiveEcuC/Com/ComConfig/[ComIPdu]/ComRxDeadlineMonitoringPeriod
 * 0 を設定すると監視無効。単位: ms。
 * ----------------------------------------------------------------------- */

/** EngineInfo (0x100) 受信タイムアウト [ms]
 *  エンジン ECU からの受信が途絶えた場合に FAULT 遷移させる基準時間。
 *  App_EngineManager_Run の周期 (3000ms) より長く設定すること。 */
#define COM_TIMEOUT_ENGINE_INFO_MS  5000U

/** AbsInfo (0x110) 受信タイムアウト [ms]
 *  ABS ECU からの受信が途絶えた場合に ABS 警告灯を消灯にフォールバックする。 */
#define COM_TIMEOUT_ABS_INFO_MS     5000U

/** TX I-PDU テーブルのエントリ数
 *  DaVinci: /ActiveEcuC/Com/ComConfig/ 内 Direction=SEND の ComIPdu 数
 *  [0]=MeterStatus 0x200 (直接送信)、[1]=WarningStatus 0x210 (Signal Group) */
#define COM_TX_IPDU_COUNT   2U

/**
 * TX I-PDU の周期送信フロア [Com_TriggerIPDUSend 呼び出し回数]
 * DaVinci: /ActiveEcuC/Com/ComConfig/[ComIPdu]/ComTxModeFalse（MIXED 送信モード相当）
 *
 * ComFilterAlgorithm によって「変化なし」と判定され続けても、この回数だけ
 * Com_TriggerIPDUSend() が呼ばれたら強制的に送信する（実車の MIXED 送信モードが
 * 持つ「周期フロア」を簡易的に再現）。値が変化した場合は判定を待たず即座に送信する。
 * MeterStatus は App_EngineManager_Run (3000ms 周期) から毎回呼ばれるため、
 * この値が 3 なら実質 9000ms ごとに無変化でも強制送信されることになる。
 */
#define COM_TX_PERIODIC_FLOOR_CYCLES  3U

/** シグナルテーブルのエントリ数（RX + TX の合計）
 *  DaVinci: /ActiveEcuC/Com/ComConfig/ 内 ComSignal ノード数の合計 */
#define COM_SIGNAL_COUNT    10U

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

/* -----------------------------------------------------------------------
 * WarningStatus シグナル (CAN ID 0x210, TX, Signal Group)
 * App_WarningIndicator が制御する 3 本の警告灯を 1 つの Signal Group として
 * まとめて送信する（Com_SendSignalGroup 経由）。
 * ----------------------------------------------------------------------- */

/** TX: RUNNING LED 状態 (1 bit, CAN ID 0x210, byte[0] bit7) */
#define COM_SIGNAL_RUN_LAMP        7U

/** TX: FAULT LED 状態 (1 bit, CAN ID 0x210, byte[0] bit6) */
#define COM_SIGNAL_FAULT_LAMP      8U

/** TX: ABS LED 状態 (1 bit, CAN ID 0x210, byte[0] bit5) */
#define COM_SIGNAL_ABS_LAMP        9U

#endif /* COM_CFG_H */
