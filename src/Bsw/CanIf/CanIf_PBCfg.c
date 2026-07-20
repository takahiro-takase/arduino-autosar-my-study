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
 *              CanId=0x200, DLC=1 (E2E 保護なし), HTH=0
 *            TX PDU (TxPduId=1): UDS 診断応答
 *              CanId=0x7E8, DLC=8, HTH=0
 *            TX PDU (TxPduId=2): NM フレーム (Nm、PduR/Com を経由せず直接呼び出す)
 *              CanId=0x400, DLC=2, HTH=0
 *            TX PDU (TxPduId=3): WarningStatus (COM Signal Group)
 *              CanId=0x210, DLC=1, HTH=0
 *            TX PDU (TxPduId=4): E2EHealthStatus (COM PERIODIC、E2E P01 保護)
 *              CanId=0x220, DLC=4, HTH=0
 *            RX PDU (RxPduId=0): EngineInfo  (エンジン ECU)
 *              CanId=0x100, HRH=0 → PduR RxPduId=0 → COM IPduId=0
 *            RX PDU (RxPduId=1): UDS 診断要求 (診断ツール)
 *              CanId=0x7E0, HRH=0 → PduR RxPduId=1 → CanTp
 *            RX PDU (RxPduId=2): AbsInfo     (ABS ECU)
 *              CanId=0x110, HRH=0 → PduR RxPduId=2 → COM IPduId=1
 *            RX PDU (RxPduId=3): ImmobilizerCmd (KeyFobEcu 想定、SecOC 保護)
 *              CanId=0x120, HRH=0 → PduR RxPduId=3 → SecOC → (検証成功時) COM IPduId=2
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
 *   .Dlc                     ←→ CanIfRxPduDataLength（設定 DLC 未満の L-PDU は棄却）
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
        .Dlc               = 1U,          /* DaVinci: CanIfTxPduDlc (E2E 保護なし、シグナル1Bのみ) */
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
    },
    {
        /* ---------------------------------------------------------------
         * TxPduId=2: NM フレーム
         * DaVinci: /ActiveEcuC/CanIf/CanIfInitCfg/CanIfTxPduCfg/Nm_Tx
         * Nm.c は PduR/Com を経由せず CanIf_Transmit(NM_CANIF_TX_PDU_ID, ...) を
         * 直接呼び出す（実車の CanNm と同じく Com スタックとは独立して動作する）。
         * --------------------------------------------------------------- */
        .UpperLayerTxPduId = 2U,          /* DaVinci: CanIfTxPduId (Nm_Cfg.h の NM_CANIF_TX_PDU_ID と一致させること) */
        .CanId             = 0x400U,      /* DaVinci: CanIfTxPduCanId */
        .Dlc               = 2U,          /* DaVinci: CanIfTxPduDlc (Control Bit Vector 1B + Source Node ID 1B) */
        .Hth               = 0U,          /* DaVinci: CanIfTxPduHthIdRef */
        .TxConfirmFct      = NULL         /* DaVinci: CanIfTxPduUserTxConfirmationName = NULL */
    },
    {
        /* ---------------------------------------------------------------
         * TxPduId=3: WarningStatus フレーム (COM Signal Group)
         * DaVinci: /ActiveEcuC/CanIf/CanIfInitCfg/CanIfTxPduCfg/WarningStatus_Tx
         * --------------------------------------------------------------- */
        .UpperLayerTxPduId = 2U,          /* DaVinci: CanIfTxPduId。TxConfirmation で上位層(PduR)へ
                                          *          返す ID のため、CanIf 自身の TxPduId(=3) ではなく
                                          *          PduR_PBCfg.c の PduR_TxPaths[] における
                                          *          WarningStatus パスの SrcPduId(=2) と一致させる。
                                          *          （MeterStatus/UDS応答は SrcPduId と CanIf TxPduId が
                                          *          たまたま同値のため、この区別が表面化していなかった） */
        .CanId             = 0x210U,      /* DaVinci: CanIfTxPduCanId */
        .Dlc               = 1U,          /* DaVinci: CanIfTxPduDlc (RunLamp/FaultLamp/AbsLamp 各1bit) */
        .Hth               = 0U,          /* DaVinci: CanIfTxPduHthIdRef */
        .TxConfirmFct      = PduR_CanIfTxConfirmation /* DaVinci: CanIfTxPduUserTxConfirmationName */
    },
    {
        /* ---------------------------------------------------------------
         * TxPduId=4: E2EHealthStatus フレーム (COM PERIODIC)
         * DaVinci: /ActiveEcuC/CanIf/CanIfInitCfg/CanIfTxPduCfg/E2EHealthStatus_Tx
         * --------------------------------------------------------------- */
        .UpperLayerTxPduId = 3U,          /* DaVinci: CanIfTxPduId。PduR_PBCfg.c の
                                          *          E2EHealthStatus パスの SrcPduId(=3) と一致させる */
        .CanId             = 0x220U,      /* DaVinci: CanIfTxPduCanId */
        .Dlc               = 4U,          /* DaVinci: CanIfTxPduDlc (E2E P01 保護: CRC1B+Counter1B
                                            *          + CrcErrCount1B + SeqErrCount1B) */
        .Hth               = 0U,          /* DaVinci: CanIfTxPduHthIdRef */
        .TxConfirmFct      = PduR_CanIfTxConfirmation /* DaVinci: CanIfTxPduUserTxConfirmationName */
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
        .Dlc               = 6U,          /* DaVinci: CanIfRxPduDataLength (E2E P01 保護: CRC1B+Counter1B+シグナル4B) */
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
        .Dlc               = 8U,          /* DaVinci: CanIfRxPduDataLength (ISO 15765-2 診断フレームは常に8バイト) */
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
        .Dlc               = 5U,          /* DaVinci: CanIfRxPduDataLength (E2E P01 保護: CRC1B+Counter1B+シグナル3B) */
        .RxIndicationFct   = PduR_CanIfRxIndication  /* DaVinci: CanIfRxPduUserRxIndicationName */
    },
    {
        /* ---------------------------------------------------------------
         * RxPduId=3: ImmobilizerCmd フレーム (KeyFobEcu 想定 → メータ ECU)
         * DaVinci: /ActiveEcuC/CanIf/CanIfInitCfg/CanIfRxPduCfg/ImmobilizerCmd_Rx
         * SecOC Profile 1 (24Bit-CMAC-8Bit-FV) で保護される Secured I-PDU。
         * 詳細は src/Bsw/SecOC/SecOC_PBCfg.c 参照。
         * --------------------------------------------------------------- */
        .CanId             = 0x120U,      /* DaVinci: CanIfRxPduCanId */
        .Hrh               = 0U,          /* DaVinci: CanIfRxPduHrhIdRef */
        .UpperLayerRxPduId = 3U,          /* DaVinci: CanIfRxPduUpperLayerPduId
                                           *          → PduR RX パス 3 へのリンク */
        .Dlc               = 6U,          /* DaVinci: CanIfRxPduDataLength
                                           *          (Authentic 2B + Freshness 1B + 切り詰めMAC 3B) */
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
