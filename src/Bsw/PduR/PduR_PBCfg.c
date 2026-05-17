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
 *              配信先 1: COM  → Com_RxIndication
 *              配信先 2: DCM  → Dcm_ComIndication
 *            TX パス 0 (SrcPduId=0):
 *              CanIf TxPduId=0, TxConfirmation → Com_TxConfirmation
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
#include "Dcm_Cbk.h"

/* -----------------------------------------------------------------------
 * RX 配信先テーブル（パスごと）
 * 1 つの RX パスに複数の配信先を持たせてマルチキャストを実現する。
 * ----------------------------------------------------------------------- */
static const PduR_RxDestType PduR_RxDests_Path0[PDUR_RX_DEST_COUNT_PATH0] = {
    {
        /* 配信先 1: COM モジュール */
        .Module   = PDUR_MODULE_COM,
        .DestPduId = 0U,
        .RxIndFct  = Com_RxIndication
    },
    {
        /* 配信先 2: DCM モジュール（診断通信スタブ） */
        .Module   = PDUR_MODULE_DCM,
        .DestPduId = 0U,
        .RxIndFct  = Dcm_ComIndication
    }
};

/* -----------------------------------------------------------------------
 * RX ルーティングパステーブル
 * CanIf からの RxPduId に対してどのパスを使うかを定義する。
 * ----------------------------------------------------------------------- */
static const PduR_RxRoutingPathType PduR_RxPaths[PDUR_RX_PATH_COUNT] = {
    {
        /* パス 0: CAN ID 0x100 受信フレーム → COM + DCM へマルチキャスト */
        .SrcPduId  = 0U,
        .Dests     = PduR_RxDests_Path0,
        .DestCount = PDUR_RX_DEST_COUNT_PATH0
    }
};

/* -----------------------------------------------------------------------
 * TX ルーティングパステーブル
 * COM からの送信要求を CanIf へ転送するルートを定義する。
 * ----------------------------------------------------------------------- */
static const PduR_TxRoutingPathType PduR_TxPaths[PDUR_TX_PATH_COUNT] = {
    {
        /* パス 0: COM TX PDU 0 → CanIf TxPduId 0 */
        .SrcPduId      = 0U,
        .CanIfTxPduId  = 0U,
        .ConfDestPduId = 0U,
        .ConfFct       = Com_TxConfirmation
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
