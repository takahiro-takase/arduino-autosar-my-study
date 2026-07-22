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

/** TX PDU テーブルのエントリ数（送信 CAN フレーム種別数）
 *  TxPduId=0: EngineState    (CAN 0x200, COM)
 *  TxPduId=1: UDS 診断応答   (CAN 0x7E8, DCM)
 *  TxPduId=2: NM フレーム    (CAN 0x400, Nm。PduR/Com を経由せず直接呼び出す)
 *  TxPduId=3: WarningStatus  (CAN 0x210, COM Signal Group)
 *  TxPduId=4: E2EHealthStatus (CAN 0x220, COM PERIODIC)
 *  TxPduId=5: ImmobilizerStatus (CAN 0x230, COM DIRECT。Signal Gateway 転送先) */
#define CANIF_TX_PDU_COUNT  6U

/** RX PDU テーブルのエントリ数（受信 CAN フレーム種別数）
 *  DaVinci: /ActiveEcuC/CanIf/CanIfInitCfg/CanIfRxPduCfg ノード数
 *  RxPduId=0: EngineInfo     (CAN 0x100, COM)   エンジン ECU
 *  RxPduId=1: UDS 診断要求   (CAN 0x7E0, DCM)   診断ツール
 *  RxPduId=2: AbsInfo        (CAN 0x110, COM)   ABS ECU
 *  RxPduId=3: ImmobilizerCmd (CAN 0x120, SecOC) KeyFobEcu 想定 */
#define CANIF_RX_PDU_COUNT  4U

#endif /* CANIF_CFG_H */
