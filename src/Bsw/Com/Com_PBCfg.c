/**
 * \file    Com_PBCfg.c
 * \brief   通信マネージャ ポストビルド設定データ (AUTOSAR SWS_COM 準拠)
 * \details COM モジュールのポストビルド設定インスタンス Com_Config を定義する。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツールが生成するファイルに
 *          相当し、I-PDU / シグナルのレイアウト情報を実装コードから分離して管理する。
 *
 *          本プロジェクトの設定（メータ ECU 想定）:
 *            RX I-PDU 0 (IPduId=0): CAN ID 0x100, DLC=4  EngineInfo  (エンジン ECU)
 *              Signal 0: EngineSpeed   16 bit  BitPos= 0  BigEndian
 *              Signal 1: CoolantTemp    8 bit  BitPos=16  BigEndian
 *              Signal 2: EngineOnFlag   1 bit  BitPos=24  BigEndian
 *            RX I-PDU 1 (IPduId=1): CAN ID 0x110, DLC=3  AbsInfo     (ABS ECU)
 *              Signal 4: VehicleSpeed  16 bit  BitPos= 0  BigEndian  0.01 km/h
 *              Signal 5: BrakeActive    1 bit  BitPos=16  BigEndian  0=解除/1=作動
 *              Signal 6: AbsActive      1 bit  BitPos=17  BigEndian  0=非作動/1=作動
 *            TX I-PDU 0 (IPduId=0): CAN ID 0x200, DLC=1  MeterStatus (メータ ECU)
 *              Signal 3: EngineState    8 bit  BitPos= 0  BigEndian
 *
 * =====================================================================
 * DaVinci Configurator 対応表（各フィールドと GUI パラメータの対応）
 * =====================================================================
 *
 * [Com_IPduConfigType] ←→ /ActiveEcuC/Com/ComConfig/[ComIPdu]
 *   .IPduId  ←→ ComIPduHandleId   （I-PDU の識別番号）
 *   .DLC     ←→ ComIPduLength     （I-PDU のバイト長）
 *   .PduRId  ←→ ComIPduPduRef     （PduR の対応 PDU へのリンク）
 *
 * [Com_SignalConfigType] ←→ /ActiveEcuC/Com/ComConfig/[ComSignal]
 *   .SignalId    ←→ ComHandleId          （シグナルの識別番号）
 *   .IPduId      ←→ ComIPduRef           （所属する I-PDU へのリンク）
 *   .BitPosition ←→ ComBitPosition       （PDU バッファ内のビット開始位置）
 *   .BitSize     ←→ ComBitSize           （シグナルのビット長）
 *   .Endian      ←→ ComSignalEndianness  （OPAQUE=BigEndian / INTEL=LittleEndian）
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

/* -----------------------------------------------------------------------
 * RX I-PDU テーブル
 * DaVinci: /ActiveEcuC/Com/ComConfig/[ComIPdu] (Direction=RECEIVE)
 * Com_RxIndication() が受信 PDU をバッファに格納する際に参照する。
 * ----------------------------------------------------------------------- */
static const Com_IPduConfigType Com_RxIPduConfigData[COM_RX_IPDU_COUNT] = {
    {
        /* ---------------------------------------------------------------
         * RX IPduId=0: EngineInfo フレーム (エンジン ECU 送信)
         * DaVinci: /ActiveEcuC/Com/ComConfig/EngineInfo_Rx
         * --------------------------------------------------------------- */
        .IPduId    = 0U,                        /* DaVinci: ComIPduHandleId  - I-PDU 識別番号 */
        .DLC       = 4U,                        /* DaVinci: ComIPduLength    - I-PDU バイト長 */
        .PduRId    = 0U,                        /* DaVinci: ComIPduPduRef    - PduR が Com_RxIndication へ渡す DestPduId
                                                 *          (PduR_PBCfg.c PduR_RxDests_Path0[0].DestPduId と一致させること) */
        .TimeoutMs = COM_TIMEOUT_ENGINE_INFO_MS /* DaVinci: ComRxDeadlineMonitoringPeriod
                                                 *          エンジン ECU からの受信が途絶えたと判断するまでの時間 */
    },
    {
        /* ---------------------------------------------------------------
         * RX IPduId=1: AbsInfo フレーム (ABS ECU 送信)
         * DaVinci: /ActiveEcuC/Com/ComConfig/AbsInfo_Rx
         * --------------------------------------------------------------- */
        .IPduId    = 1U,                       /* DaVinci: ComIPduHandleId  - I-PDU 識別番号 */
        .DLC       = 3U,                       /* DaVinci: ComIPduLength    - I-PDU バイト長 */
        .PduRId    = 1U,                       /* DaVinci: ComIPduPduRef    - PduR が Com_RxIndication へ渡す DestPduId
                                                *          (PduR_PBCfg.c PduR_RxDests_Path2[0].DestPduId と一致させること) */
        .TimeoutMs = COM_TIMEOUT_ABS_INFO_MS   /* DaVinci: ComRxDeadlineMonitoringPeriod
                                                *          ABS ECU からの受信が途絶えたと判断するまでの時間 */
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
         * TX IPduId=0: MeterStatus フレーム (メータ ECU 送信)
         * DaVinci: /ActiveEcuC/Com/ComConfig/MeterStatus_Tx
         * --------------------------------------------------------------- */
        .IPduId    = 0U,  /* DaVinci: ComIPduHandleId  - I-PDU 識別番号 */
        .DLC       = 1U,  /* DaVinci: ComIPduLength    - I-PDU バイト長 */
        .PduRId    = 0U,  /* DaVinci: ComIPduPduRef    - PduR TX パス 0 へのリンク */
        .TimeoutMs = 0U   /* TX I-PDU のため監視無効 */
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
         * Signal 0: EngineSpeed  RX 16bit  CAN 0x100 byte[0-1]
         * DaVinci: /ActiveEcuC/Com/ComConfig/EngineSpeed_Rx
         * --------------------------------------------------------------- */
        .SignalId    = COM_SIGNAL_ENGINE_SPEED, /* DaVinci: ComHandleId         */
        .IPduId      = 0U,                      /* DaVinci: ComIPduRef → EngineInfo_Rx */
        .BitPosition = 0U,                      /* DaVinci: ComBitPosition      */
        .BitSize     = 16U,                     /* DaVinci: ComBitSize          */
        .Endian      = COM_BIG_ENDIAN           /* DaVinci: ComSignalEndianness = OPAQUE */
    },
    {
        /* ---------------------------------------------------------------
         * Signal 1: CoolantTemp  RX 8bit  CAN 0x100 byte[2]
         * DaVinci: /ActiveEcuC/Com/ComConfig/CoolantTemp_Rx
         * --------------------------------------------------------------- */
        .SignalId    = COM_SIGNAL_COOLANT_TEMP, /* DaVinci: ComHandleId         */
        .IPduId      = 0U,                      /* DaVinci: ComIPduRef → EngineInfo_Rx */
        .BitPosition = 16U,                     /* DaVinci: ComBitPosition      */
        .BitSize     = 8U,                      /* DaVinci: ComBitSize          */
        .Endian      = COM_BIG_ENDIAN           /* DaVinci: ComSignalEndianness = OPAQUE */
    },
    {
        /* ---------------------------------------------------------------
         * Signal 2: EngineOnFlag  RX 1bit  CAN 0x100 byte[3] bit7
         * DaVinci: /ActiveEcuC/Com/ComConfig/EngineOnFlag_Rx
         * --------------------------------------------------------------- */
        .SignalId    = COM_SIGNAL_ENGINE_ON_FLAG, /* DaVinci: ComHandleId       */
        .IPduId      = 0U,                        /* DaVinci: ComIPduRef → EngineInfo_Rx */
        .BitPosition = 24U,                       /* DaVinci: ComBitPosition    */
        .BitSize     = 1U,                        /* DaVinci: ComBitSize        */
        .Endian      = COM_BIG_ENDIAN             /* DaVinci: ComSignalEndianness = OPAQUE */
    },
    {
        /* ---------------------------------------------------------------
         * Signal 3: EngineState  TX 8bit  CAN 0x200 byte[0]
         * DaVinci: /ActiveEcuC/Com/ComConfig/EngineState_Tx
         * --------------------------------------------------------------- */
        .SignalId    = COM_SIGNAL_ENGINE_STATE, /* DaVinci: ComHandleId         */
        .IPduId      = 0U,                      /* DaVinci: ComIPduRef → MeterStatus_Tx */
        .BitPosition = 0U,                      /* DaVinci: ComBitPosition      */
        .BitSize     = 8U,                      /* DaVinci: ComBitSize          */
        .Endian      = COM_BIG_ENDIAN           /* DaVinci: ComSignalEndianness = OPAQUE */
    },
    {
        /* ---------------------------------------------------------------
         * Signal 4: VehicleSpeed  RX 16bit  CAN 0x110 byte[0-1]  0.01 km/h
         * DaVinci: /ActiveEcuC/Com/ComConfig/VehicleSpeed_Rx
         * --------------------------------------------------------------- */
        .SignalId    = COM_SIGNAL_VEHICLE_SPEED, /* DaVinci: ComHandleId        */
        .IPduId      = 1U,                       /* DaVinci: ComIPduRef → AbsInfo_Rx */
        .BitPosition = 0U,                       /* DaVinci: ComBitPosition     */
        .BitSize     = 16U,                      /* DaVinci: ComBitSize         */
        .Endian      = COM_BIG_ENDIAN            /* DaVinci: ComSignalEndianness = OPAQUE */
    },
    {
        /* ---------------------------------------------------------------
         * Signal 5: BrakeActive  RX 1bit  CAN 0x110 byte[2] bit7
         *   0=ブレーキ解除, 1=ブレーキ作動
         * DaVinci: /ActiveEcuC/Com/ComConfig/BrakeActive_Rx
         * --------------------------------------------------------------- */
        .SignalId    = COM_SIGNAL_BRAKE_ACTIVE, /* DaVinci: ComHandleId          */
        .IPduId      = 1U,                      /* DaVinci: ComIPduRef → AbsInfo_Rx */
        .BitPosition = 16U,                     /* DaVinci: ComBitPosition       */
        .BitSize     = 1U,                      /* DaVinci: ComBitSize           */
        .Endian      = COM_BIG_ENDIAN           /* DaVinci: ComSignalEndianness = OPAQUE */
    },
    {
        /* ---------------------------------------------------------------
         * Signal 6: AbsActive  RX 1bit  CAN 0x110 byte[2] bit6
         *   0=ABS 非作動, 1=ABS 作動中
         * DaVinci: /ActiveEcuC/Com/ComConfig/AbsActive_Rx
         * --------------------------------------------------------------- */
        .SignalId    = COM_SIGNAL_ABS_ACTIVE,   /* DaVinci: ComHandleId          */
        .IPduId      = 1U,                      /* DaVinci: ComIPduRef → AbsInfo_Rx */
        .BitPosition = 17U,                     /* DaVinci: ComBitPosition       */
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
