/**
 * \file    Can_PBCfg.c
 * \brief   CAN ドライバ ポストビルド設定データ (AUTOSAR SWS_Can 準拠)
 * \details CAN ドライバのポストビルド設定インスタンス Can_Config を定義する。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツールが生成するファイルに
 *          相当し、ECU ごとに異なるハードウェア設定（ピン番号・ボーレート等）を
 *          実装コードから分離して管理する。
 *
 *          本プロジェクトの設定:
 *            - RX フィルタ : CAN ID 0x100（マスク 0x7FF で完全一致）
 *            - TX CAN ID   : 0x200 (CanIf_PBCfg.c で設定)
 *            - ボーレート  : 500 kbps
 *            - 水晶        : 8 MHz
 *            - CS ピン     : 10
 *            - INT ピン    : 2
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "Can_PBCfg.h"
/* ボーレート定数は Can_Cfg.h の CAN_BAUDRATE_* を使用する。
 * mcp_can_dfs.h は C++ ヘッダのため .c ファイルからはインクルード不可。 */

/**
 * \brief   CAN ドライバのポストビルド設定インスタンス。
 *
 * \details Can_Init() の引数として渡す設定データ。
 *          フィルタ・ピン番号・ボーレート・水晶周波数を保持する。
 *          ピン番号とフィルタ値は Can_Cfg.h のプリコンパイル定数を使用し、
 *          ボーレートは mcp_can_dfs.h の CAN_500KBPS を参照する。
 */
const Can_ConfigType Can_Config = {
    .filter      = { .filterId = 0x100U, .mask = 0x7FFU },
    .csPin       = CAN_CS_PIN,
    .intPin      = CAN_INT_PIN,
    .baudrate    = CAN_BAUDRATE_500KBPS,
    .crystalFreq = CAN_CRYSTAL_8MHZ
};
