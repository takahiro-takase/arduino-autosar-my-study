/**
 * \file    Com_PBCfg.c
 * \brief   通信マネージャ ポストビルド設定データ (AUTOSAR SWS_COM 準拠)
 * \details COM モジュールのポストビルド設定インスタンス Com_Config を定義する。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツールが生成するファイルに
 *          相当し、I-PDU / シグナルのレイアウト情報を実装コードから分離して管理する。
 *
 *          本プロジェクトの設定:
 *            RX I-PDU (IPduId=0): CAN ID 0x100, DLC=4
 *              Signal 0: EngineSpeed  16 bit  BitPos= 0 BigEndian
 *              Signal 1: CoolantTemp   8 bit  BitPos=16 BigEndian
 *              Signal 2: EngineOnFlag  1 bit  BitPos=24 BigEndian
 *            TX I-PDU (IPduId=0): CAN ID 0x200, DLC=1
 *              Signal 3: EngineState   8 bit  BitPos= 0 BigEndian
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
 * Com_RxIndication() が受信 PDU をバッファに格納する際に参照する。
 * ----------------------------------------------------------------------- */
static const Com_IPduConfigType Com_RxIPduConfigData[COM_RX_IPDU_COUNT] = {
    {
        /* IPduId=0: CAN ID 0x100 センサフレーム (DLC=4) */
        .IPduId = 0U,
        .DLC    = 4U,
        .PduRId = 0U
    }
};

/* -----------------------------------------------------------------------
 * TX I-PDU テーブル
 * Com_TriggerIPDUSend() が送信要求を PduR へ転送する際に参照する。
 * ----------------------------------------------------------------------- */
static const Com_IPduConfigType Com_TxIPduConfigData[COM_TX_IPDU_COUNT] = {
    {
        /* IPduId=0: CAN ID 0x200 エンジン状態フレーム (DLC=1) */
        .IPduId = 0U,
        .DLC    = 1U,
        .PduRId = 0U
    }
};

/* -----------------------------------------------------------------------
 * シグナルテーブル（RX + TX 共通）
 * Com_ReceiveSignal() / Com_SendSignal() がビットパック・アンパックに使用する。
 * シグナル ID は Com_Cfg.h の COM_SIGNAL_* 定数と対応している。
 * ----------------------------------------------------------------------- */
static const Com_SignalConfigType Com_SignalConfigData[COM_SIGNAL_COUNT] = {
    {
        /* Signal 0: EngineSpeed (RX, 16 bit, BitPos=0, BigEndian) */
        .SignalId    = COM_SIGNAL_ENGINE_SPEED,
        .IPduId      = 0U,
        .BitPosition = 0U,
        .BitSize     = 16U,
        .Endian      = COM_BIG_ENDIAN
    },
    {
        /* Signal 1: CoolantTemp (RX, 8 bit, BitPos=16, BigEndian) */
        .SignalId    = COM_SIGNAL_COOLANT_TEMP,
        .IPduId      = 0U,
        .BitPosition = 16U,
        .BitSize     = 8U,
        .Endian      = COM_BIG_ENDIAN
    },
    {
        /* Signal 2: EngineOnFlag (RX, 1 bit, BitPos=24, BigEndian) */
        .SignalId    = COM_SIGNAL_ENGINE_ON_FLAG,
        .IPduId      = 0U,
        .BitPosition = 24U,
        .BitSize     = 1U,
        .Endian      = COM_BIG_ENDIAN
    },
    {
        /* Signal 3: EngineState (TX, 8 bit, BitPos=0, BigEndian) */
        .SignalId    = COM_SIGNAL_ENGINE_STATE,
        .IPduId      = 0U,
        .BitPosition = 0U,
        .BitSize     = 8U,
        .Endian      = COM_BIG_ENDIAN
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
