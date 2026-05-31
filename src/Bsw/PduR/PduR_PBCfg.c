/**
 * \file    PduR_PBCfg.c
 * \brief   PDU ルータ ポストビルド設定データ (AUTOSAR SWS_PDURouter 準拠)
 * \details PDU ルータのポストビルド設定インスタンス PduR_Config を定義する。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツールが生成するファイルに
 *          相当し、RX / TX ルーティングテーブルと上位層コールバック関数を
 *          実装コードから分離して管理する。
 *
 *          本プロジェクトの設定（メータ ECU 想定）:
 *            RX パス 0 (SrcPduId=0, CAN ID 0x100):
 *              配信先: COM のみ → Com_RxIndication (DestPduId=0: EngineInfo_Rx, エンジン ECU)
 *            RX パス 1 (SrcPduId=1, CAN ID 0x7E0):
 *              配信先: CanTp → CanTp_RxIndication (トランスポート層経由で DCM へ, 診断ツール)
 *            RX パス 2 (SrcPduId=2, CAN ID 0x110):
 *              配信先: COM のみ → Com_RxIndication (DestPduId=1: AbsInfo_Rx, ABS ECU)
 *            TX パス 0 (SrcPduId=0):
 *              CanIf TxPduId=0 → CAN 0x200, TxConfirmation → Com_TxConfirmation
 *            TX パス 1 (SrcPduId=1):
 *              CanIf TxPduId=1 → CAN 0x7E8, TxConfirmation → CanTp_TxConfirmation
 *
 * =====================================================================
 * DaVinci Configurator 対応表
 * =====================================================================
 *
 * [PduR_RxRoutingPathType] ←→ /ActiveEcuC/PduR/PduRConfig/PduRRoutingTable/
 *                               PduRRoutingPath (PduRSrcPduRef.Direction=RECEIVE)
 *   .SrcPduId  ←→ PduRSrcPdu/PduRSrcPduHandleId  (CanIf が割り当てる RxPduId)
 *   .DestPduId ←→ PduRDestPdu[n]/PduRDestPduHandleId (上位層の PDU インデックス)
 *   .RxIndFct  ←→ PduRDestPdu[n]/PduRDestModule  (配信先モジュール名)
 *
 * [PduR_TxRoutingPathType] ←→ /ActiveEcuC/PduR/PduRConfig/PduRRoutingTable/
 *                               PduRRoutingPath (PduRSrcPduRef.Direction=SEND)
 *   .SrcPduId      ←→ PduRSrcPdu/PduRSrcPduHandleId  (上位層の TxPduId)
 *   .CanIfTxPduId  ←→ PduRDestPdu[n]/PduRDestPduHandleId (CanIf TxPdu インデックス)
 *   .ConfFct       ←→ PduRTxConfirmation (送信確認コールバック)
 *
 * =====================================================================
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "PduR_PBCfg.h"
#include "PduR_Cfg.h"
#include "Com.h"
#include "CanTp.h"

/* -----------------------------------------------------------------------
 * RX 配信先テーブル（パスごと）
 * DaVinci: /ActiveEcuC/PduR/PduRConfig/PduRRoutingTable/[PduRRoutingPath]/
 *           PduRDestPdu
 * ----------------------------------------------------------------------- */

/* パス 0: CAN 0x100 → COM (EngineInfo)
 * DaVinci: /ActiveEcuC/PduR/PduRConfig/PduRRoutingTable/EngineInfo_Rx */
static const PduR_RxDestType PduR_RxDests_Path0[PDUR_RX_DEST_COUNT_PATH0] = {
    {
        .Module    = PDUR_MODULE_COM, /* DaVinci: PduRDestPdu/PduRDestModule = COM */
        .DestPduId = 0U,              /* DaVinci: PduRDestPdu/PduRDestPduHandleId
                                       *          → COM RX IPduId=0 (EngineInfo_Rx) */
        .RxIndFct  = Com_RxIndication /* DaVinci: 自動解決（PduRDestModule=COM） */
    }
};

/* パス 1: CAN 0x7E0 → CanTp (UDS 診断)
 * DaVinci: /ActiveEcuC/PduR/PduRConfig/PduRRoutingTable/DiagReq_Rx */
static const PduR_RxDestType PduR_RxDests_Path1[PDUR_RX_DEST_COUNT_PATH1] = {
    {
        .Module    = PDUR_MODULE_CANTP, /* DaVinci: PduRDestPdu/PduRDestModule = CANTP */
        .DestPduId = 0U,                /* DaVinci: PduRDestPdu/PduRDestPduHandleId */
        .RxIndFct  = CanTp_RxIndication /* DaVinci: 自動解決（PduRDestModule=CANTP） */
    }
};

/* パス 2: CAN 0x110 → COM (AbsInfo)
 * DaVinci: /ActiveEcuC/PduR/PduRConfig/PduRRoutingTable/AbsInfo_Rx */
static const PduR_RxDestType PduR_RxDests_Path2[PDUR_RX_DEST_COUNT_PATH2] = {
    {
        .Module    = PDUR_MODULE_COM, /* DaVinci: PduRDestPdu/PduRDestModule = COM */
        .DestPduId = 1U,              /* DaVinci: PduRDestPdu/PduRDestPduHandleId
                                       *          → COM RX IPduId=1 (AbsInfo_Rx) */
        .RxIndFct  = Com_RxIndication /* DaVinci: 自動解決（PduRDestModule=COM） */
    }
};

/* -----------------------------------------------------------------------
 * RX ルーティングパステーブル
 * DaVinci: /ActiveEcuC/PduR/PduRConfig/PduRRoutingTable/[PduRRoutingPath]
 * CanIf からの RxPduId に対してどのパスを使うかを定義する。
 * ----------------------------------------------------------------------- */
static const PduR_RxRoutingPathType PduR_RxPaths[PDUR_RX_PATH_COUNT] = {
    {
        /* パス 0: CanIf RxPduId=0 (CAN 0x100) → COM/EngineInfo
         * DaVinci: PduRSrcPdu/PduRSrcPduHandleId = 0
         *          (CanIf_PBCfg の RxPduId=0 と一致) */
        .SrcPduId  = 0U,
        .Dests     = PduR_RxDests_Path0,
        .DestCount = PDUR_RX_DEST_COUNT_PATH0
    },
    {
        /* パス 1: CanIf RxPduId=1 (CAN 0x7E0) → CanTp/DCM */
        .SrcPduId  = 1U,
        .Dests     = PduR_RxDests_Path1,
        .DestCount = PDUR_RX_DEST_COUNT_PATH1
    },
    {
        /* パス 2: CanIf RxPduId=2 (CAN 0x110) → COM/AbsInfo
         * DaVinci: PduRSrcPdu/PduRSrcPduHandleId = 2
         *          (CanIf_PBCfg の RxPduId=2 と一致) */
        .SrcPduId  = 2U,
        .Dests     = PduR_RxDests_Path2,
        .DestCount = PDUR_RX_DEST_COUNT_PATH2
    }
};

/* -----------------------------------------------------------------------
 * TX ルーティングパステーブル
 * DaVinci: /ActiveEcuC/PduR/PduRConfig/PduRRoutingTable/[PduRRoutingPath]
 * COM からの送信要求を CanIf へ転送するルートを定義する。
 * ----------------------------------------------------------------------- */
static const PduR_TxRoutingPathType PduR_TxPaths[PDUR_TX_PATH_COUNT] = {
    {
        /* パス 0: COM (SrcPduId=0) → CanIf TxPduId=0 (CAN 0x200)
         * DaVinci: PduRRoutingPath/MeterStatus_Tx */
        .SrcPduId      = 0U,              /* DaVinci: PduRSrcPdu/PduRSrcPduHandleId */
        .CanIfTxPduId  = 0U,              /* DaVinci: PduRDestPdu/PduRDestPduHandleId */
        .ConfDestPduId = 0U,
        .ConfFct       = Com_TxConfirmation /* DaVinci: PduRTxConfirmation */
    },
    {
        /* パス 1: CanTp (SrcPduId=1) → CanIf TxPduId=1 (CAN 0x7E8) */
        .SrcPduId      = 1U,
        .CanIfTxPduId  = 1U,
        .ConfDestPduId = 0U,
        .ConfFct       = CanTp_TxConfirmation
    }
};

/* -----------------------------------------------------------------------
 * PduR ポストビルド設定インスタンス
 * PduR_Init() の引数として渡す。
 * ----------------------------------------------------------------------- */
const PduR_PBConfigType PduR_Config = {
    .RxPaths     = PduR_RxPaths,
    .RxPathCount = PDUR_RX_PATH_COUNT,
    .TxPaths     = PduR_TxPaths,
    .TxPathCount = PDUR_TX_PATH_COUNT
};
