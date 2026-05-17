/**
 * \file    Can_Cfg.h
 * \brief   CAN ドライバ プリコンパイル設定 (AUTOSAR SWS_Can 準拠)
 * \details CAN ドライバのプリコンパイル設定を定義する。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツール
 *          (EB tresos / Vector DaVinci 等) が生成するファイルに相当する。
 *          ハードウェア依存の設定型（Can_ConfigType 等）もここで定義し、
 *          Can.h からインクルードして使用する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef CAN_CFG_H
#define CAN_CFG_H

#include "Platform_Types.h"

/* -----------------------------------------------------------------------
 * プリコンパイル設定定数 (AUTOSAR SWS_Can_00413)
 * ----------------------------------------------------------------------- */

/** CAN コントローラ数（本実装は MCP2515 を 1 個使用） */
#define CAN_CONTROLLER_COUNT   1U

/** MCP2515 チップセレクト (CS) ピン番号 */
#define CAN_CS_PIN             10U

/** MCP2515 割り込み (INT) ピン番号 */
#define CAN_INT_PIN            2U

/* -----------------------------------------------------------------------
 * 水晶発振器周波数型および定数
 * ----------------------------------------------------------------------- */

/* -----------------------------------------------------------------------
 * ボーレート定数 (mcp_can_dfs.h の enum 値と一致)
 * mcp_can_dfs.h は C++ ヘッダのため .c ファイルから直接インクルードできない。
 * ここで同等の定数を定義し、BSW 設定ファイルから参照できるようにする。
 * ----------------------------------------------------------------------- */
/* mcp_can @ 1.5.1 (mcp_can_dfs.h) の定義値と一致させること */
#define CAN_BAUDRATE_4K096BPS  0U
#define CAN_BAUDRATE_5KBPS     1U
#define CAN_BAUDRATE_10KBPS    2U
#define CAN_BAUDRATE_20KBPS    3U
#define CAN_BAUDRATE_31K25BPS  4U
#define CAN_BAUDRATE_33K3BPS   5U
#define CAN_BAUDRATE_40KBPS    6U
#define CAN_BAUDRATE_50KBPS    7U
#define CAN_BAUDRATE_80KBPS    8U
#define CAN_BAUDRATE_100KBPS   9U
#define CAN_BAUDRATE_125KBPS   10U
#define CAN_BAUDRATE_200KBPS   11U
#define CAN_BAUDRATE_250KBPS   12U
#define CAN_BAUDRATE_500KBPS   13U
#define CAN_BAUDRATE_1000KBPS  14U

/* -----------------------------------------------------------------------
 * 水晶発振器周波数型および定数
 * ----------------------------------------------------------------------- */

/** 水晶発振器周波数を表す型 */
typedef uint8 Can_CrystalFreqType;

#define CAN_CRYSTAL_8MHZ   ((Can_CrystalFreqType)8U)   /**< 8 MHz 水晶 */
#define CAN_CRYSTAL_16MHZ  ((Can_CrystalFreqType)16U)  /**< 16 MHz 水晶 */
#define CAN_CRYSTAL_20MHZ  ((Can_CrystalFreqType)20U)  /**< 20 MHz 水晶 */

/* -----------------------------------------------------------------------
 * RX フィルタ設定型
 * ----------------------------------------------------------------------- */

/**
 * \brief CAN 受信フィルタ設定型。
 *        filterId と mask の組み合わせで受信する CAN ID を絞り込む。
 */
typedef struct
{
    uint32 filterId; /**< 受け付ける CAN ID */
    uint32 mask;     /**< フィルタマスク（1 = 一致必須ビット） */
} Can_FilterConfigType;

/* -----------------------------------------------------------------------
 * CAN コントローラ設定型 (AUTOSAR SWS_Can_00413)
 * ----------------------------------------------------------------------- */

/**
 * \brief CAN ドライバ設定型。
 *        Can_Init() に渡すコントローラごとの設定を保持する。
 */
typedef struct
{
    Can_FilterConfigType filter;      /**< RX フィルタ設定 */
    uint8               csPin;       /**< SPI チップセレクトピン番号 */
    uint8               intPin;      /**< 割り込みピン番号 */
    uint32              baudrate;    /**< ボーレート（mcp_can_dfs.h の CAN_*KBPS 値） */
    Can_CrystalFreqType crystalFreq; /**< 水晶発振器周波数 (MHz) */
} Can_ConfigType;

#endif /* CAN_CFG_H */
