/**
 * \file    PduR_PBCfg.c
 * \brief   PDU ルータ ポストビルド設定データ (AUTOSAR SWS_PDURouter 準拠)
 * \details PDU ルータのポストビルド設定インスタンス PduR_Config を定義する。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツールが生成するファイルに
 *          相当し、RX / TX ルーティングテーブルと上位層コールバック関数を
 *          実装コードから分離して管理する。
 *
 *          本プロジェクトの設定:
 *            RX パス 0 (SrcPduId=0, CAN ID 0x100):
 *              配信先: COM のみ → Com_RxIndication
 *            RX パス 1 (SrcPduId=1, CAN ID 0x7E0):
 *              配信先: CanTp → CanTp_RxIndication (トランスポート層経由で DCM へ渡す)
 *            TX パス 0 (SrcPduId=0):
 *              CanIf TxPduId=0 → CAN 0x200, TxConfirmation → Com_TxConfirmation
 *            TX パス 1 (SrcPduId=1):
 *              CanIf TxPduId=1 → CAN 0x7E8, TxConfirmation → CanTp_TxConfirmation
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
 * 1 つの RX パスに複数の配信先を持たせてマルチキャストを実現する。
 * ----------------------------------------------------------------------- */
/* パス 0: CAN 0x100 → COM のみ（センサデータはアプリ専用） */
static const PduR_RxDestType PduR_RxDests_Path0[PDUR_RX_DEST_COUNT_PATH0] = {
    {
        .Module    = PDUR_MODULE_COM,
        .DestPduId = 0U,
        .RxIndFct  = Com_RxIndication
    }
};

/* パス 1: CAN 0x7E0 → CanTp (診断要求は CanTp でトランスポート処理後 DCM へ渡す) */
static const PduR_RxDestType PduR_RxDests_Path1[PDUR_RX_DEST_COUNT_PATH1] = {
    {
        .Module    = PDUR_MODULE_CANTP,
        .DestPduId = 0U,
        .RxIndFct  = CanTp_RxIndication
    }
};

/* -----------------------------------------------------------------------
 * RX ルーティングパステーブル
 * CanIf からの RxPduId に対してどのパスを使うかを定義する。
 * ----------------------------------------------------------------------- */
static const PduR_RxRoutingPathType PduR_RxPaths[PDUR_RX_PATH_COUNT] = {
    {
        /* パス 0: CanIf RxPduId=0 (CAN 0x100) → COM */
        .SrcPduId  = 0U,
        .Dests     = PduR_RxDests_Path0,
        .DestCount = PDUR_RX_DEST_COUNT_PATH0
    },
    {
        /* パス 1: CanIf RxPduId=1 (CAN 0x7E0) → DCM */
        .SrcPduId  = 1U,
        .Dests     = PduR_RxDests_Path1,
        .DestCount = PDUR_RX_DEST_COUNT_PATH1
    }
};

/* -----------------------------------------------------------------------
 * TX ルーティングパステーブル
 * COM からの送信要求を CanIf へ転送するルートを定義する。
 * ----------------------------------------------------------------------- */
static const PduR_TxRoutingPathType PduR_TxPaths[PDUR_TX_PATH_COUNT] = {
    {
        /* パス 0: COM (SrcPduId=0) → CanIf TxPduId=0 (CAN 0x200) */
        .SrcPduId      = 0U,
        .CanIfTxPduId  = 0U,
        .ConfDestPduId = 0U,
        .ConfFct       = Com_TxConfirmation
    },
    {
        /* パス 1: CanTp (SrcPduId=1) → CanIf TxPduId=1 (CAN 0x7E8)
         * TxConfirmation → CanTp_TxConfirmation (CF タイミング管理用) */
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
