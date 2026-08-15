/**
 * \file    Mcu_Cfg.h
 * \brief   MCU Driver プリコンパイル設定 (AUTOSAR SWS_Mcu 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef MCU_CFG_H
#define MCU_CFG_H

/* -----------------------------------------------------------------------
 * DET（Default Error Tracer）関連定数
 *
 * ModuleId は SWS 本文には明記されないため、AUTOSAR_TR_BSWModuleList
 * （Release 4.3.1、docs/ 配下）の「List of Basic Software Modules」表で
 * MCU Driver (Mcu) に割り当てられた固定値 101 を使う。
 *
 * 開発エラーコードは docs/4.3.1/AUTOSAR_SWS_MCUDriver.pdf
 * ([SWS_Mcu_00012] 7.2.1 Development Errors 表、8.3 章 Service ID[hex])
 * を実測して確認済み。MCU_E_PARAM_CLOCK/MCU_E_PARAM_MODE/
 * MCU_E_PARAM_RAMSECTION/MCU_E_PLL_NOT_LOCKED/MCU_E_INIT_FAILED は、
 * 本プロジェクトが Mcu_InitClock()/Mcu_SetMode()/Mcu_InitRamSection()/
 * Mcu_DistributePllClock() を実装しない（Arduino フレームワークが
 * クロック初期化を担い、RAM セクション初期化・複数電源モードを
 * モデル化しないため。Mcu.h 冒頭のコメント参照）ため未使用。
 * MCU_E_PARAM_CONFIG は本来 Mcu_Init() の NULL ConfigPtr チェック用
 * （[SWS_Mcu_00012] の表には載るが、この用途を明示する個別の "shall"
 * 要求は本書には存在しない。他モジュールの ConfigPtr NULL チェックと
 * 同じ扱いとして採用）だが、Mcu_Init() は Serial.begin() より前に呼ばれる
 * ため DET_LOGx/Det_ReportError を一切呼ばない設計（Mcu.c の Mcu_Init()
 * 冒頭コメント参照）。そのため本コードは定義のみで実際には報告されない
 * （NULL ConfigPtr は黙って早期 return するのみ）。
 * ----------------------------------------------------------------------- */

/** AUTOSAR MCU Driver の ModuleId（AUTOSAR_TR_BSWModuleList 参照、固定値 101） */
#define MCU_MODULE_ID  101U

/** 開発エラーコード（[SWS_Mcu_00012] 7.2.1 表より、実際に使用する分のみ） */
#define MCU_E_PARAM_CONFIG   0x0AU  /**< Mcu_Init() の ConfigPtr が NULL              */
#define MCU_E_UNINIT         0x0FU  /**< [SWS_Mcu_00125]: Mcu_Init 前の API 呼び出し
                                      *   （Mcu_GetVersionInfo を除く）                */
#define MCU_E_PARAM_POINTER  0x10U  /**< Mcu_GetVersionInfo() の versioninfo が NULL
                                      *   （他モジュールと同じ慣例。本書に個別の
                                      *   "shall" 要求はない） */

/** ApiId（各関数の Doxygen \ServiceID タグと一致させること。
 *  8.3 章の Service ID[hex] を実測して確認済み） */
#define MCU_API_ID_INIT                   0x00U
#define MCU_API_ID_GET_RESET_REASON       0x05U
#define MCU_API_ID_GET_RESET_RAW_VALUE    0x06U
#define MCU_API_ID_GET_VERSION_INFO       0x09U

/** バージョン情報（Wdg/Com/PduR 等の既存モジュールと同じ命名規則） */
#define MCU_VENDOR_ID          0U
#define MCU_SW_MAJOR_VERSION   1U
#define MCU_SW_MINOR_VERSION   0U
#define MCU_SW_PATCH_VERSION   0U

#endif /* MCU_CFG_H */
