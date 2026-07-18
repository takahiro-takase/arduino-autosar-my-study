/**
 * \file    Com_PBCfg.c
 * \brief   通信マネージャ ポストビルド設定データ (AUTOSAR SWS_COM 準拠)
 * \details COM モジュールのポストビルド設定インスタンス Com_Config を定義する。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツールが生成するファイルに
 *          相当し、I-PDU / シグナルのレイアウト情報を実装コードから分離して管理する。
 *
 *          本プロジェクトの設定（メータ ECU 想定）:
 *            RX I-PDU 0 (IPduId=0): CAN ID 0x100, DLC=6  EngineInfo  (エンジン ECU、E2E P01 保護)
 *              byte[0]: E2E CRC8 / byte[1]: E2E Counter (下位4bit)
 *              （AUTOSAR 標準バリアント 1A、SWS_E2E_00227 準拠のレイアウト）
 *              Signal 0: EngineSpeed   16 bit  BitPos=16  BigEndian
 *              Signal 1: CoolantTemp    8 bit  BitPos=32  BigEndian
 *              Signal 2: EngineOnFlag   1 bit  BitPos=40  BigEndian
 *            RX I-PDU 1 (IPduId=1): CAN ID 0x110, DLC=5  AbsInfo     (ABS ECU, E2E P01 保護)
 *              byte[0]: E2E CRC8 / byte[1]: E2E Counter (下位4bit)
 *              （AUTOSAR 標準バリアント 1A、SWS_E2E_00227 準拠のレイアウト）
 *              Signal 4: VehicleSpeed  16 bit  BitPos=16  BigEndian  0.01 km/h
 *              Signal 5: BrakeActive    1 bit  BitPos=32  BigEndian  0=解除/1=作動
 *              Signal 6: AbsActive      1 bit  BitPos=33  BigEndian  0=非作動/1=作動
 *            TX I-PDU 0 (IPduId=0): CAN ID 0x200, DLC=3  MeterStatus (メータ ECU、E2E P01 保護)
 *              byte[0]: E2E CRC8 / byte[1]: E2E Counter (下位4bit)
 *              （AUTOSAR 標準バリアント 1A、SWS_E2E_00227 準拠のレイアウト）
 *              Signal 3: EngineState    8 bit  BitPos=16  BigEndian
 *              ComFilterAlgorithm=MASKED_NEW_DIFFERS_MASKED_OLD（値変化時のみ送信要求。
 *              詳細は Com_Cfg.h の COM_TX_PERIODIC_FLOOR_CYCLES を参照）
 *            TX I-PDU 1 (IPduId=1): CAN ID 0x210, DLC=1  WarningStatus (メータ ECU、Signal Group)
 *              Signal 7: RunLamp        1 bit  BitPos= 0  BigEndian
 *              Signal 8: FaultLamp      1 bit  BitPos= 1  BigEndian
 *              Signal 9: AbsLamp        1 bit  BitPos= 2  BigEndian
 *              IsSignalGroup=1（Com_SendSignalGroup で 3 信号を一括コミット）
 *
 *          E2E P01 の設定・ステート実体（DataID/Counter・CRC オフセット等）は
 *          E2E Transformer 方式への移行に伴い src/Bsw/E2EXf/E2EXf_PBCfg.c へ
 *          移設した。ここでは RxIndicationCbk/TxTransformCbk 経由で Rte 層の
 *          グルー関数（Rte_COMCbk_EngineInfo 等）を紐付けるのみで、
 *          Com_PBCfg.c 自体は E2E の詳細を一切保持しない。
 *
 * =====================================================================
 * DaVinci Configurator 対応表（各フィールドと GUI パラメータの対応）
 * =====================================================================
 *
 * [Com_IPduConfigType] ←→ /ActiveEcuC/Com/ComConfig/[ComIPdu]
 *   .IPduId  ←→ ComIPduHandleId   （I-PDU の識別番号）
 *   .DLC     ←→ ComIPduLength     （I-PDU のバイト長）
 *   .PduRId  ←→ ComIPduPduRef     （PduR の対応 PDU へのリンク）
 *   .IsSignalGroup ←→ （本プロジェクト独自拡張。DaVinci では ComSignalGroup の
 *                      有無で暗黙的に表現される）1 = Signal Group、
 *                      Com_SendSignalGroup() で確定コミットする対象
 *
 * [Com_SignalConfigType] ←→ /ActiveEcuC/Com/ComConfig/[ComSignal]
 *   .SignalId    ←→ ComHandleId          （シグナルの識別番号）
 *   .IPduId      ←→ ComIPduRef           （所属する I-PDU へのリンク）
 *   .BitPosition ←→ ComBitPosition       （PDU バッファ内のビット開始位置）
 *   .BitSize     ←→ ComBitSize           （シグナルのビット長）
 *   .Endian      ←→ ComSignalEndianness  （OPAQUE=BigEndian / INTEL=LittleEndian）
 *   .FilterAlgorithm ←→ ComFilterAlgorithm （TX シグナルのみ。RX シグナルは既定の ALWAYS のまま未使用）
 *   .Mask            ←→ ComFilterMask
 *
 * =====================================================================
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "Com_PBCfg.h"
#include "Com_Cfg.h"

/* Rte 層の E2E Transformer 呼び出しグルー関数（Rte.c で定義）。
 * Os_PBCfg.c 等と同じく、レイヤ違反（Com が Rte.h を include する）を
 * 避けるためローカル extern 宣言で参照する。 */
extern void Rte_COMCbk_EngineInfo(void);
extern void Rte_COMCbk_AbsInfo(void);
extern void Rte_COMTransform_MeterStatus(uint8* Data, uint8 Length);

/* -----------------------------------------------------------------------
 * RX I-PDU テーブル
 * DaVinci: /ActiveEcuC/Com/ComConfig/[ComIPdu] (Direction=RECEIVE)
 * Com_RxIndication() が受信 PDU をバッファに格納する際に参照する。
 * ----------------------------------------------------------------------- */
static const Com_IPduConfigType Com_RxIPduConfigData[COM_RX_IPDU_COUNT] = {
    {
        /* ---------------------------------------------------------------
         * RX IPduId=0: EngineInfo フレーム (エンジン ECU 送信、E2E P01 保護)
         * DaVinci: /ActiveEcuC/Com/ComConfig/EngineInfo_Rx
         * --------------------------------------------------------------- */
        .IPduId    = 0U,                        /* DaVinci: ComIPduHandleId  - I-PDU 識別番号 */
        .DLC       = 6U,                        /* DaVinci: ComIPduLength    - I-PDU バイト長
                                                 *          byte[0]=E2E CRC, byte[1]=E2E Counter, byte[2-5]=シグナル */
        .PduRId    = 0U,                        /* DaVinci: ComIPduPduRef    - PduR が Com_RxIndication へ渡す DestPduId
                                                 *          (PduR_PBCfg.c PduR_RxDests_Path0[0].DestPduId と一致させること) */
        .TimeoutMs = COM_TIMEOUT_ENGINE_INFO_MS,/* DaVinci: ComRxDeadlineMonitoringPeriod
                                                 *          エンジン ECU からの受信が途絶えたと判断するまでの時間 */
        .RxIndicationCbk = Rte_COMCbk_EngineInfo /* DaVinci: /ActiveEcuC/E2EXf/EngineInfo_Rx_E2EXf
                                                 *          （E2E Transformer 呼び出しは Rte 層が担う） */
    },
    {
        /* ---------------------------------------------------------------
         * RX IPduId=1: AbsInfo フレーム (ABS ECU 送信、E2E P01 保護)
         * DaVinci: /ActiveEcuC/Com/ComConfig/AbsInfo_Rx
         * --------------------------------------------------------------- */
        .IPduId    = 1U,                       /* DaVinci: ComIPduHandleId  - I-PDU 識別番号 */
        .DLC       = 5U,                       /* DaVinci: ComIPduLength    - I-PDU バイト長
                                                *          byte[0]=E2E CRC, byte[1]=E2E Counter, byte[2-4]=シグナル */
        .PduRId    = 1U,                       /* DaVinci: ComIPduPduRef    - PduR が Com_RxIndication へ渡す DestPduId
                                                *          (PduR_PBCfg.c PduR_RxDests_Path2[0].DestPduId と一致させること) */
        .TimeoutMs = COM_TIMEOUT_ABS_INFO_MS,  /* DaVinci: ComRxDeadlineMonitoringPeriod
                                                *          ABS ECU からの受信が途絶えたと判断するまでの時間 */
        .RxIndicationCbk = Rte_COMCbk_AbsInfo  /* DaVinci: /ActiveEcuC/E2EXf/AbsInfo_Rx_E2EXf
                                                *          （E2E Transformer 呼び出しは Rte 層が担う） */
    }
};

/* -----------------------------------------------------------------------
 * TX I-PDU テーブル
 * DaVinci: /ActiveEcuC/Com/ComConfig/[ComIPdu] (Direction=SEND)
 * Com_TriggerIPDUSend() が送信要求を PduR へ転送する際に参照する。
 * ----------------------------------------------------------------------- */
static const Com_IPduConfigType Com_TxIPduConfigData[COM_TX_IPDU_COUNT] = {
    {
        /* ---------------------------------------------------------------
         * TX IPduId=0: MeterStatus フレーム (メータ ECU 送信、E2E P01 保護)
         * DaVinci: /ActiveEcuC/Com/ComConfig/MeterStatus_Tx
         * --------------------------------------------------------------- */
        .IPduId    = 0U,  /* DaVinci: ComIPduHandleId  - I-PDU 識別番号 */
        .DLC       = 3U,  /* DaVinci: ComIPduLength    - I-PDU バイト長
                           *          byte[0]=シグナル, byte[1]=E2E Counter, byte[2]=E2E CRC */
        .PduRId    = 0U,  /* DaVinci: ComIPduPduRef    - PduR TX パス 0 へのリンク */
        .TimeoutMs = 0U,  /* TX I-PDU のため監視無効 */
        .IsSignalGroup = 0U, /* 直接送信（既存の挙動のまま） */
        .TxTransformCbk = Rte_COMTransform_MeterStatus /* DaVinci: /ActiveEcuC/E2EXf/MeterStatus_Tx_E2EXf
                                                *          （E2E Transformer 呼び出しは Rte 層が担う） */
    },
    {
        /* ---------------------------------------------------------------
         * TX IPduId=1: WarningStatus フレーム (メータ ECU 送信、Signal Group)
         * DaVinci: /ActiveEcuC/Com/ComConfig/WarningStatus_Tx
         * App_WarningIndicator が制御する 3 本の警告灯（RUNNING/FAULT/ABS）を
         * 1 つの Signal Group としてまとめて送信する。
         * --------------------------------------------------------------- */
        .IPduId    = 1U,  /* DaVinci: ComIPduHandleId  - I-PDU 識別番号 */
        .DLC       = 1U,  /* DaVinci: ComIPduLength    - I-PDU バイト長（3bit のみ使用） */
        .PduRId    = 2U,  /* DaVinci: ComIPduPduRef    - PduR TX パス 2 へのリンク
                           *          (PduR_Transmit の SrcPduId は COM/CanTp で共通の名前空間のため、
                           *          CanTp が使用する 1U と衝突しないよう 2U を割り当てる) */
        .TimeoutMs = 0U,  /* TX I-PDU のため監視無効 */
        .IsSignalGroup = 1U /* Signal Group（Com_SendSignalGroup で確定コミット） */
    }
};

/* -----------------------------------------------------------------------
 * シグナルテーブル（RX + TX 共通）
 * DaVinci: /ActiveEcuC/Com/ComConfig/[ComSignal]
 * Com_ReceiveSignal() / Com_SendSignal() がビットパック・アンパックに使用する。
 * シグナル ID は Com_Cfg.h の COM_SIGNAL_* 定数と対応している。
 *
 * ComBitPosition の数え方（BigEndian/OPAQUE）:
 *   byte[0] の MSB = ビット位置 0、byte[0] の LSB = ビット位置 7
 *   byte[1] の MSB = ビット位置 8 ...
 * ----------------------------------------------------------------------- */
static const Com_SignalConfigType Com_SignalConfigData[COM_SIGNAL_COUNT] = {
    {
        /* ---------------------------------------------------------------
         * Signal 0: EngineSpeed  RX 16bit  CAN 0x100 byte[2-3]
         * （byte[0]=E2E CRC8, byte[1]=E2E Counter を先頭に配置する
         *   AUTOSAR 標準バリアント 1A レイアウトのため、シグナルは byte[2] から）
         * DaVinci: /ActiveEcuC/Com/ComConfig/EngineSpeed_Rx
         * --------------------------------------------------------------- */
        .SignalId    = COM_SIGNAL_ENGINE_SPEED, /* DaVinci: ComHandleId         */
        .IPduId      = 0U,                      /* DaVinci: ComIPduRef → EngineInfo_Rx */
        .BitPosition = 16U,                     /* DaVinci: ComBitPosition      */
        .BitSize     = 16U,                     /* DaVinci: ComBitSize          */
        .Endian      = COM_BIG_ENDIAN           /* DaVinci: ComSignalEndianness = OPAQUE */
    },
    {
        /* ---------------------------------------------------------------
         * Signal 1: CoolantTemp  RX 8bit  CAN 0x100 byte[4]
         * DaVinci: /ActiveEcuC/Com/ComConfig/CoolantTemp_Rx
         * --------------------------------------------------------------- */
        .SignalId    = COM_SIGNAL_COOLANT_TEMP, /* DaVinci: ComHandleId         */
        .IPduId      = 0U,                      /* DaVinci: ComIPduRef → EngineInfo_Rx */
        .BitPosition = 32U,                     /* DaVinci: ComBitPosition      */
        .BitSize     = 8U,                      /* DaVinci: ComBitSize          */
        .Endian      = COM_BIG_ENDIAN           /* DaVinci: ComSignalEndianness = OPAQUE */
    },
    {
        /* ---------------------------------------------------------------
         * Signal 2: EngineOnFlag  RX 1bit  CAN 0x100 byte[5] bit7
         * DaVinci: /ActiveEcuC/Com/ComConfig/EngineOnFlag_Rx
         * --------------------------------------------------------------- */
        .SignalId    = COM_SIGNAL_ENGINE_ON_FLAG, /* DaVinci: ComHandleId       */
        .IPduId      = 0U,                        /* DaVinci: ComIPduRef → EngineInfo_Rx */
        .BitPosition = 40U,                       /* DaVinci: ComBitPosition    */
        .BitSize     = 1U,                        /* DaVinci: ComBitSize        */
        .Endian      = COM_BIG_ENDIAN             /* DaVinci: ComSignalEndianness = OPAQUE */
    },
    {
        /* ---------------------------------------------------------------
         * Signal 3: EngineState  TX 8bit  CAN 0x200 byte[2]
         * （byte[0]=E2E CRC8, byte[1]=E2E Counter を先頭に配置する
         *   AUTOSAR 標準バリアント 1A レイアウトのため、シグナルは byte[2] から）
         * DaVinci: /ActiveEcuC/Com/ComConfig/EngineState_Tx
         * --------------------------------------------------------------- */
        .SignalId    = COM_SIGNAL_ENGINE_STATE, /* DaVinci: ComHandleId         */
        .IPduId      = 0U,                      /* DaVinci: ComIPduRef → MeterStatus_Tx */
        .BitPosition = 16U,                     /* DaVinci: ComBitPosition      */
        .BitSize     = 8U,                      /* DaVinci: ComBitSize          */
        .Endian      = COM_BIG_ENDIAN,          /* DaVinci: ComSignalEndianness = OPAQUE */
        .FilterAlgorithm = COM_FILTER_MASKED_NEW_DIFFERS_MASKED_OLD, /* DaVinci: ComFilterAlgorithm
                                                 *          値が変化したときだけ送信要求とみなす */
        .Mask            = 0xFFU                /* DaVinci: ComFilterNewValue/ComFilterMask 相当（8bit 全体を比較） */
    },
    {
        /* ---------------------------------------------------------------
         * Signal 4: VehicleSpeed  RX 16bit  CAN 0x110 byte[2-3]  0.01 km/h
         * （byte[0]=E2E CRC8, byte[1]=E2E Counter を先頭に配置する
         *   AUTOSAR 標準バリアント 1A レイアウトのため、シグナルは byte[2] から）
         * DaVinci: /ActiveEcuC/Com/ComConfig/VehicleSpeed_Rx
         * --------------------------------------------------------------- */
        .SignalId    = COM_SIGNAL_VEHICLE_SPEED, /* DaVinci: ComHandleId        */
        .IPduId      = 1U,                       /* DaVinci: ComIPduRef → AbsInfo_Rx */
        .BitPosition = 16U,                      /* DaVinci: ComBitPosition     */
        .BitSize     = 16U,                      /* DaVinci: ComBitSize         */
        .Endian      = COM_BIG_ENDIAN            /* DaVinci: ComSignalEndianness = OPAQUE */
    },
    {
        /* ---------------------------------------------------------------
         * Signal 5: BrakeActive  RX 1bit  CAN 0x110 byte[4] bit7
         *   0=ブレーキ解除, 1=ブレーキ作動
         * DaVinci: /ActiveEcuC/Com/ComConfig/BrakeActive_Rx
         * --------------------------------------------------------------- */
        .SignalId    = COM_SIGNAL_BRAKE_ACTIVE, /* DaVinci: ComHandleId          */
        .IPduId      = 1U,                      /* DaVinci: ComIPduRef → AbsInfo_Rx */
        .BitPosition = 32U,                     /* DaVinci: ComBitPosition       */
        .BitSize     = 1U,                      /* DaVinci: ComBitSize           */
        .Endian      = COM_BIG_ENDIAN           /* DaVinci: ComSignalEndianness = OPAQUE */
    },
    {
        /* ---------------------------------------------------------------
         * Signal 6: AbsActive  RX 1bit  CAN 0x110 byte[4] bit6
         *   0=ABS 非作動, 1=ABS 作動中
         * DaVinci: /ActiveEcuC/Com/ComConfig/AbsActive_Rx
         * --------------------------------------------------------------- */
        .SignalId    = COM_SIGNAL_ABS_ACTIVE,   /* DaVinci: ComHandleId          */
        .IPduId      = 1U,                      /* DaVinci: ComIPduRef → AbsInfo_Rx */
        .BitPosition = 33U,                     /* DaVinci: ComBitPosition       */
        .BitSize     = 1U,                      /* DaVinci: ComBitSize           */
        .Endian      = COM_BIG_ENDIAN           /* DaVinci: ComSignalEndianness = OPAQUE */
    },
    {
        /* ---------------------------------------------------------------
         * Signal 7: RunLamp  TX 1bit  CAN 0x210 byte[0] bit7  (Signal Group メンバー)
         * DaVinci: /ActiveEcuC/Com/ComConfig/RunLamp_Tx
         * --------------------------------------------------------------- */
        .SignalId    = COM_SIGNAL_RUN_LAMP,     /* DaVinci: ComHandleId          */
        .IPduId      = 1U,                      /* DaVinci: ComIPduRef → WarningStatus_Tx */
        .BitPosition = 0U,                      /* DaVinci: ComBitPosition       */
        .BitSize     = 1U,                      /* DaVinci: ComBitSize           */
        .Endian      = COM_BIG_ENDIAN           /* DaVinci: ComSignalEndianness = OPAQUE */
    },
    {
        /* ---------------------------------------------------------------
         * Signal 8: FaultLamp  TX 1bit  CAN 0x210 byte[0] bit6  (Signal Group メンバー)
         * DaVinci: /ActiveEcuC/Com/ComConfig/FaultLamp_Tx
         * --------------------------------------------------------------- */
        .SignalId    = COM_SIGNAL_FAULT_LAMP,   /* DaVinci: ComHandleId          */
        .IPduId      = 1U,                      /* DaVinci: ComIPduRef → WarningStatus_Tx */
        .BitPosition = 1U,                      /* DaVinci: ComBitPosition       */
        .BitSize     = 1U,                      /* DaVinci: ComBitSize           */
        .Endian      = COM_BIG_ENDIAN           /* DaVinci: ComSignalEndianness = OPAQUE */
    },
    {
        /* ---------------------------------------------------------------
         * Signal 9: AbsLamp  TX 1bit  CAN 0x210 byte[0] bit5  (Signal Group メンバー)
         * DaVinci: /ActiveEcuC/Com/ComConfig/AbsLamp_Tx
         * --------------------------------------------------------------- */
        .SignalId    = COM_SIGNAL_ABS_LAMP,     /* DaVinci: ComHandleId          */
        .IPduId      = 1U,                      /* DaVinci: ComIPduRef → WarningStatus_Tx */
        .BitPosition = 2U,                      /* DaVinci: ComBitPosition       */
        .BitSize     = 1U,                      /* DaVinci: ComBitSize           */
        .Endian      = COM_BIG_ENDIAN           /* DaVinci: ComSignalEndianness = OPAQUE */
    }
};

/* -----------------------------------------------------------------------
 * COM ポストビルド設定インスタンス
 * Com_Init() の引数として渡す。
 * ----------------------------------------------------------------------- */
const Com_ConfigType Com_Config = {
    .RxIPdus     = Com_RxIPduConfigData,
    .RxIPduCount = COM_RX_IPDU_COUNT,
    .TxIPdus     = Com_TxIPduConfigData,
    .TxIPduCount = COM_TX_IPDU_COUNT,
    .Signals     = Com_SignalConfigData,
    .SignalCount  = COM_SIGNAL_COUNT
};
