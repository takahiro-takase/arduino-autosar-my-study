/**
 * \file    CanIf_PBCfg.c
 * \brief   CAN インタフェース ポストビルド設定データ (AUTOSAR SWS_CANInterface 準拠)
 * \details CAN インタフェースのポストビルド設定インスタンス CanIf_Config を定義する。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツールが生成するファイルに
 *          相当し、TX / RX PDU ルーティングテーブルと上位層コールバック関数を
 *          実装コードから分離して管理する。
 *
 *          本プロジェクトの設定:
 *            TX PDU (TxPduId=0): EngineState
 *              CanId=0x200, DLC=1, HTH=0
 *              TxConfirmation → PduR_CanIfTxConfirmation
 *            TX PDU (TxPduId=1): UDS 診断応答
 *              CanId=0x7E8, DLC=8, HTH=0
 *              TxConfirmation → NULL (DCM は送信確認不要)
 *            RX PDU (RxPduId=0): センサデータ
 *              CanId=0x100, HRH=0
 *              RxIndication  → PduR_CanIfRxIndication
 *            RX PDU (RxPduId=1): UDS 診断要求
 *              CanId=0x7E0, HRH=0
 *              RxIndication  → PduR_CanIfRxIndication
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
 * TxPduId をインデックスとして CanIf_Transmit() が参照する。
 * ----------------------------------------------------------------------- */
static const CanIf_TxPduConfigType CanIf_TxPduConfigData[CANIF_TX_PDU_COUNT] = {
    {
        /* TxPduId=0: EngineState フレーム (CAN ID 0x200, DLC 1) */
        .UpperLayerTxPduId = 0U,
        .CanId             = 0x200U,
        .Dlc               = 1U,
        .Hth               = 0U,
        .TxConfirmFct      = PduR_CanIfTxConfirmation
    },
    {
        /* TxPduId=1: UDS 診断応答 (CAN ID 0x7E8, DLC 8) */
        .UpperLayerTxPduId = 1U,
        .CanId             = 0x7E8U,
        .Dlc               = 8U,
        .Hth               = 0U,
        .TxConfirmFct      = NULL
    }
};

/* -----------------------------------------------------------------------
 * RX PDU ルーティングテーブル
 * HOH と CAN ID の組み合わせで CanIf_RxIndication() が検索する。
 * ----------------------------------------------------------------------- */
static const CanIf_RxPduConfigType CanIf_RxPduConfigData[CANIF_RX_PDU_COUNT] = {
    {
        /* RxPduId=0: センサフレーム (CAN ID 0x100) → PduR RxPduId=0 → COM */
        .CanId             = 0x100U,
        .Hrh               = 0U,
        .UpperLayerRxPduId = 0U,
        .RxIndicationFct   = PduR_CanIfRxIndication
    },
    {
        /* RxPduId=1: UDS 診断要求 (CAN ID 0x7E0) → PduR RxPduId=1 → DCM */
        .CanId             = 0x7E0U,
        .Hrh               = 0U,
        .UpperLayerRxPduId = 1U,
        .RxIndicationFct   = PduR_CanIfRxIndication
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
