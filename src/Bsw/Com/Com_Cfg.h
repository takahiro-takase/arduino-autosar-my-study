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

/* -----------------------------------------------------------------------
 * I-PDU Group ID 定数（Com_IpduGroupIdType、DaVinci: ComIPduGroup）
 * Com_IpduGroupStart() / Com_IpduGroupStop() の引数として使用する。
 * ----------------------------------------------------------------------- */

/** テレメトリ I-PDU Group（E2EHealthStatus のみ）。
 *  診断監視用のネットワーク健全性テレメトリであり、車両の基本動作には
 *  不要なため、独立して停止/再開できる I-PDU Group として分離している
 *  （EngineInfo/AbsInfo/MeterStatus/WarningStatus/ImmobilizerCmd は
 *  どの I-PDU Group にも属さず、常に有効。BswM が EcuM の POST_RUN/RUN
 *  遷移に応じて Com_IpduGroupStop/Start を呼ぶ。詳細は
 *  src/Bsw/BswM/BswM_PBCfg.c 参照） */
#define COM_IPDU_GROUP_TELEMETRY  0U

/** RX I-PDU テーブルのエントリ数
 *  DaVinci: /ActiveEcuC/Com/ComConfig/ 内 Direction=RECEIVE の ComIPdu 数 */
#define COM_RX_IPDU_COUNT   3U  /* [0]=EngineInfo 0x100, [1]=AbsInfo 0x110,
                                 * [2]=SecureCommand 0x120（SecOC 検証成功後に転送） */

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
 *  [0]=MeterStatus 0x200 (MIXED: 変化時送信 + 周期フロア)、
 *  [1]=WarningStatus 0x210 (Signal Group, DIRECT: 変化時のみ送信)、
 *  [2]=E2EHealthStatus 0x220 (PERIODIC、E2EMon CDD 相当が発行するネットワーク
 *  健全性テレメトリ。詳細は src/Bsw/E2EMon/E2EMon.c 参照) */
#define COM_TX_IPDU_COUNT   3U

/** E2EHealthStatus (0x220) の PERIODIC 送信周期 [ms]
 *  DaVinci: /ActiveEcuC/Com/ComConfig/[ComIPdu]/ComTxModeTimePeriodFactor */
#define COM_TX_PERIOD_E2EHEALTH_MS  6000U

/**
 * MeterStatus (0x200) の MIXED 送信モードにおける周期フロア間隔 [ms]
 * DaVinci: /ActiveEcuC/Com/ComConfig/[ComIPdu]/ComTxModeFalse/ComTxModeTimePeriodFactor
 *
 * ComFilterAlgorithm によって「変化なし」と判定され続けても、最終送信から
 * この間隔が経過したら Com_MainFunction() が強制的に再送する（実車の MIXED
 * 送信モードが持つ「周期フロア」。起動直後や瞬断から復帰した受信側が、
 * いつまでも古い EngineState を握り続けないようにするための保険）。
 * 値が変化した場合は、このフロアを待たず次回 Com_MainFunction()（Os の
 * 100ms タスク）で送信する。
 */
#define COM_TX_PERIOD_METERSTATUS_FLOOR_MS  9000U

/**
 * WarningStatus (0x210) の TMS（Transmission Mode Selector）が true と
 * 評価された（FaultLamp/AbsLamp のいずれかが点灯中の）ときに使う
 * MIXED 送信モードの周期フロア間隔 [ms]
 * DaVinci: /ActiveEcuC/Com/ComConfig/[ComIPdu]/ComTxModeTrue/ComTxModeTimePeriodFactor
 *
 * TMS が false（全ランプ消灯）の間は DIRECT（周期フロアなし、変化時のみ送信）
 * だが、FAULT/ABS のいずれかが点灯している間は他 ECU・監視ツールが途中から
 * 参加した場合でも状態を把握できるよう、この間隔で強制的に再送する
 * （MeterStatus の周期フロアより短くしているのは、警告状態の方が早く
 * 伝わってほしいという判断）。
 */
#define COM_TX_PERIOD_WARNINGSTATUS_TRUE_FLOOR_MS  2000U

/**
 * WarningStatus (0x210) の MDT（ComMinimumDelayTime、変化時送信の最小送信間隔）[ms]
 * DaVinci: /ActiveEcuC/Com/ComConfig/[ComIPdu]/ComMinimumDelayTime
 *
 * 直近の実送信からこの時間未満しか経過していなければ、値が変化しても実送信を
 * 保留する（破棄はしない。満了次第送信する）。バス輻輳防止のための保護的な
 * 既定値であり、本プロジェクトの ASW（App_WarningIndicator_Run、500ms 周期）
 * は現状これより速く値を変化させないため、通常運用でこの下限に達することは
 * ない（詳細は README の「MDT」セクション参照）。MIXED の周期フロア送信には
 * 適用しない（Com_Cfg.h の COM_TX_PERIOD_WARNINGSTATUS_TRUE_FLOOR_MS 参照）。
 */
#define COM_TX_MIN_DELAY_WARNINGSTATUS_MS  100U

/** シグナルテーブルのエントリ数（RX + TX の合計）
 *  DaVinci: /ActiveEcuC/Com/ComConfig/ 内 ComSignal ノード数の合計 */
#define COM_SIGNAL_COUNT    13U

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

/* -----------------------------------------------------------------------
 * E2EHealthStatus シグナル (CAN ID 0x220, TX, PERIODIC)
 * E2EMon（CDD 相当、src/Bsw/E2EMon/）が EngineInfo/AbsInfo 受信の E2E
 * 検証結果を集計し、Com_SendSignal() で書き込む。送信タイミング自体は
 * Com 自身の PERIODIC モードが担う（E2EMon は関与しない）。
 * ----------------------------------------------------------------------- */

/** TX: E2E CRC 不一致累積数 (8 bit, CAN ID 0x220, byte[0]、0-255 で飽和) */
#define COM_SIGNAL_E2E_CRC_ERR_COUNT  10U

/** TX: E2E シーケンス異常累積数 (8 bit, CAN ID 0x220, byte[1]、0-255 で飽和) */
#define COM_SIGNAL_E2E_SEQ_ERR_COUNT  11U

/* -----------------------------------------------------------------------
 * ImmobilizerCmd シグナル (CAN ID 0x120, RX, SecOC 保護)
 * KeyFobEcu 想定の送信元から、SecOC（src/Bsw/SecOC/）が MAC・フレッシュネス
 * 検証に成功した場合のみ Com_RxIndication() 経由でここへ届く。
 * ----------------------------------------------------------------------- */

/** RX: イモビライザー解除コマンド (8 bit, CAN ID 0x120, byte[0]、
 *  0x00=LOCK/0x01=UNLOCK。SecOC 検証成功後のみ更新される） */
#define COM_SIGNAL_IMMOBILIZER_CMD  12U

#endif /* COM_CFG_H */
