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

/** RX ルーティングパス数
 *  DaVinci: /ActiveEcuC/PduR/PduRConfig/PduRRoutingTable 内の
 *           RECEIVE 方向 PduRRoutingPath ノード数
 *  パス 0: CAN 0x100 → COM   (EngineInfo,  エンジン ECU)
 *  パス 1: CAN 0x7E0 → CanTp (UDS 診断要求, 診断ツール)
 *  パス 2: CAN 0x110 → COM   (AbsInfo,     ABS ECU)
 *  パス 3: CAN 0x120 → SecOC (ImmobilizerCmd, KeyFobEcu 想定。検証成功後
 *           SecOC 自身が Com_RxIndication() を直接呼ぶため、COM への
 *           配信先はここには現れない） */
#define PDUR_RX_PATH_COUNT   4U

/** RX パス 0 の配信先数（COM のみ） */
#define PDUR_RX_DEST_COUNT_PATH0  1U

/** RX パス 1 の配信先数（CanTp のみ） */
#define PDUR_RX_DEST_COUNT_PATH1  1U

/** RX パス 2 の配信先数（COM のみ: AbsInfo は ABS ECU からの受信専用） */
#define PDUR_RX_DEST_COUNT_PATH2  1U

/** RX パス 3 の配信先数（SecOC のみ） */
#define PDUR_RX_DEST_COUNT_PATH3  1U

/** TX ルーティングパス数
 *  パス 0: COM   → CanIf TxPduId=0 (CAN 0x200, EngineState)
 *  パス 1: CanTp → CanIf TxPduId=1 (CAN 0x7E8, UDS 診断応答)
 *  パス 2: COM   → CanIf TxPduId=3 (CAN 0x210, WarningStatus Signal Group)
 *  パス 3: COM   → CanIf TxPduId=4 (CAN 0x220, E2EHealthStatus PERIODIC)
 *  SrcPduId は COM と CanTp が共通の名前空間として PduR_Transmit() へ渡すため、
 *  各パスで重複しない値を割り当てること。 */
#define PDUR_TX_PATH_COUNT   4U

#endif /* PDUR_CFG_H */
