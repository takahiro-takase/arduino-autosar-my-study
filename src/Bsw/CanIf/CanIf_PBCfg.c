/**
 * \file    CanIf_PBCfg.c
 * \brief   CAN インタフェース ポストビルド設定データ (AUTOSAR SWS_CANInterface 準拠)
 * \details CAN インタフェースのポストビルド設定インスタンス CanIf_Config を定義する。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツールが生成するファイルに
 *          相当し、TX / RX PDU ルーティングテーブルと上位層コールバック関数を
 *          実装コードから分離して管理する。
 *
 *          本プロジェクトの設定（メータ ECU 想定）:
 *            TX PDU (TxPduId=0): MeterStatus
 *              CanId=0x200, DLC=1, HTH=0
 *            TX PDU (TxPduId=1): UDS 診断応答
 *              CanId=0x7E8, DLC=8, HTH=0
 *            RX PDU (RxPduId=0): EngineInfo  (エンジン ECU)
 *              CanId=0x100, HRH=0 → PduR RxPduId=0 → COM IPduId=0
 *            RX PDU (RxPduId=1): UDS 診断要求 (診断ツール)
 *              CanId=0x7E0, HRH=0 → PduR RxPduId=1 → CanTp
 *            RX PDU (RxPduId=2): AbsInfo     (ABS ECU)
 *              CanId=0x110, HRH=0 → PduR RxPduId=2 → COM IPduId=1
 *
 * =====================================================================
 * DaVinci Configurator 対応表
 * =====================================================================
 *
 * [CanIf_TxPduConfigType] ←→ /ActiveEcuC/CanIf/CanIfInitCfg/CanIfTxPduCfg
 *   (配列インデックス)   ←→ CanIfTxPduId（暗黙の連番）
 *   .CanId             ←→ CanIfTxPduCanId
 *   .Dlc               ←→ CanIfTxPduDlc
 *   .Hth               ←→ CanIfTxPduHthIdRef → CanIf HOH テーブルへのリンク
 *   .TxConfirmFct      ←→ CanIfTxPduUserTxConfirmationName（上位層コールバック）
 *
 * [CanIf_RxPduConfigType] ←→ /ActiveEcuC/CanIf/CanIfInitCfg/CanIfRxPduCfg
 *   (配列インデックス)        ←→ CanIfRxPduId（暗黙の連番）
 *   .CanId                   ←→ CanIfRxPduCanId
 *   .Hrh                     ←→ CanIfRxPduHrhIdRef → CanIf HOH テーブルへのリンク
 *   .UpperLayerRxPduId        ←→ CanIfRxPduUpperLayerPduId（PduR への ID）
 *   .RxIndicationFct          ←→ CanIfRxPduUserRxIndicationName
 *
 * =====================================================================
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "CanIf_PBCfg.h"
#include "CanIf_Cfg.h"
#include "PduR_CanIf.h"

/* -----------------------------------------------------------------------
 * TX PDU ルーティングテーブル
 * DaVinci: /ActiveEcuC/CanIf/CanIfInitCfg/CanIfTxPduCfg
 * TxPduId をインデックスとして CanIf_Transmit() が参照する。
 * ----------------------------------------------------------------------- */
static const CanIf_TxPduConfigType CanIf_TxPduConfigData[CANIF_TX_PDU_COUNT] = {
    {
        /* ---------------------------------------------------------------
         * TxPduId=0: MeterStatus フレーム
         * DaVinci: /ActiveEcuC/CanIf/CanIfInitCfg/CanIfTxPduCfg/MeterStatus_Tx
         * --------------------------------------------------------------- */
        .UpperLayerTxPduId = 0U,          /* DaVinci: CanIfTxPduId（上位層の PDU ID） */
        .CanId             = 0x200U,      /* DaVinci: CanIfTxPduCanId */
        .Dlc               = 1U,          /* DaVinci: CanIfTxPduDlc */
        .Hth               = 0U,          /* DaVinci: CanIfTxPduHthIdRef */
        .TxConfirmFct      = PduR_CanIfTxConfirmation /* DaVinci: CanIfTxPduUserTxConfirmationName */
    },
    {
        /* ---------------------------------------------------------------
         * TxPduId=1: UDS 診断応答フレーム
         * DaVinci: /ActiveEcuC/CanIf/CanIfInitCfg/CanIfTxPduCfg/DiagResp_Tx
         * --------------------------------------------------------------- */
        .UpperLayerTxPduId = 1U,          /* DaVinci: CanIfTxPduId */
        .CanId             = 0x7E8U,      /* DaVinci: CanIfTxPduCanId */
        .Dlc               = 8U,          /* DaVinci: CanIfTxPduDlc */
        .Hth               = 0U,          /* DaVinci: CanIfTxPduHthIdRef */
        .TxConfirmFct      = NULL         /* DaVinci: CanIfTxPduUserTxConfirmationName = NULL */
    }
};

/* -----------------------------------------------------------------------
 * RX PDU ルーティングテーブル
 * DaVinci: /ActiveEcuC/CanIf/CanIfInitCfg/CanIfRxPduCfg
 * HOH と CAN ID の組み合わせで CanIf_RxIndication() が検索する。
 * ----------------------------------------------------------------------- */
static const CanIf_RxPduConfigType CanIf_RxPduConfigData[CANIF_RX_PDU_COUNT] = {
    {
        /* ---------------------------------------------------------------
         * RxPduId=0: EngineInfo フレーム (エンジン ECU → メータ ECU)
         * DaVinci: /ActiveEcuC/CanIf/CanIfInitCfg/CanIfRxPduCfg/EngineInfo_Rx
         * --------------------------------------------------------------- */
        .CanId             = 0x100U,      /* DaVinci: CanIfRxPduCanId */
        .Hrh               = 0U,          /* DaVinci: CanIfRxPduHrhIdRef */
        .UpperLayerRxPduId = 0U,          /* DaVinci: CanIfRxPduUpperLayerPduId
                                           *          → PduR RX パス 0 へのリンク */
        .RxIndicationFct   = PduR_CanIfRxIndication /* DaVinci: CanIfRxPduUserRxIndicationName */
    },
    {
        /* ---------------------------------------------------------------
         * RxPduId=1: UDS 診断要求フレーム (診断ツール → メータ ECU)
         * DaVinci: /ActiveEcuC/CanIf/CanIfInitCfg/CanIfRxPduCfg/DiagReq_Rx
         * --------------------------------------------------------------- */
        .CanId             = 0x7E0U,      /* DaVinci: CanIfRxPduCanId */
        .Hrh               = 0U,          /* DaVinci: CanIfRxPduHrhIdRef */
        .UpperLayerRxPduId = 1U,          /* DaVinci: CanIfRxPduUpperLayerPduId
                                           *          → PduR RX パス 1 へのリンク */
        .RxIndicationFct   = PduR_CanIfRxIndication /* DaVinci: CanIfRxPduUserRxIndicationName */
    },
    {
        /* ---------------------------------------------------------------
         * RxPduId=2: AbsInfo フレーム (ABS ECU → メータ ECU)
         * DaVinci: /ActiveEcuC/CanIf/CanIfInitCfg/CanIfRxPduCfg/AbsInfo_Rx
         * --------------------------------------------------------------- */
        .CanId             = 0x110U,      /* DaVinci: CanIfRxPduCanId */
        .Hrh               = 0U,          /* DaVinci: CanIfRxPduHrhIdRef */
        .UpperLayerRxPduId = 2U,          /* DaVinci: CanIfRxPduUpperLayerPduId
                                           *          → PduR RX パス 2 へのリンク */
        .RxIndicationFct   = PduR_CanIfRxIndication  /* DaVinci: CanIfRxPduUserRxIndicationName */
    }
};

/* -----------------------------------------------------------------------
 * CanIf ポストビルド設定インスタンス
 * CanIf_Init() の引数として渡す。
 * ----------------------------------------------------------------------- */
const CanIf_ConfigType CanIf_Config = {
    .TxPduConfig = CanIf_TxPduConfigData,
    .TxPduCount  = CANIF_TX_PDU_COUNT,
    .RxPduConfig = CanIf_RxPduConfigData,
    .RxPduCount  = CANIF_RX_PDU_COUNT
};
