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

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Can_PBCfg.h"

/* ======================================================================
 * Global Variables
 * ====================================================================== */

/* ボーレート定数は Can_Cfg.h の CAN_BAUDRATE_* を使用する。
 * mcp_can_dfs.h は C++ ヘッダのため .c ファイルからはインクルード不可。 */
/**
 * \brief   CAN ドライバのポストビルド設定インスタンス。
 *
 * \details Can_Init() の引数として渡す設定データ。
 *          フィルタ・ピン番号・ボーレート・水晶周波数を保持する。
 *          値は config/data/Can.json から生成する（下の生成領域）。
 *          ピン番号は Can_Cfg.h の CAN_CS_PIN / CAN_INT_PIN、ボーレートと水晶は
 *          Can_Cfg.h の CAN_BAUDRATE_* / CAN_CRYSTAL_*MHZ を参照する。
 */
/* @@GEN-BEGIN can-config  生成元: config/data/Can.json  （自動生成: 手編集禁止） */
const Can_ConfigType Can_Config = {
    /* mask=0x000: 全 CAN ID を受け入れ、CanIf ソフトウェアフィルタに委ねる。
     * 0x100 (COM データ) と 0x7E0 (UDS 診断) を同時に受信するために必要。
     * 本来は RX バッファごとに mask/filter を設定するが、学習用実装のため省略。 */
    .filter      = { .filterId = 0x000U, .mask = 0x000U },
    .csPin       = CAN_CS_PIN,
    .intPin      = CAN_INT_PIN,
    .baudrate    = CAN_BAUDRATE_500KBPS,
    .crystalFreq = CAN_CRYSTAL_8MHZ
};
/* @@GEN-END can-config */
