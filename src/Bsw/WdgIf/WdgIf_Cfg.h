/**
 * \file    WdgIf_Cfg.h
 * \brief   Watchdog Interface プリコンパイル設定 (AUTOSAR SWS_WdgIf 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef WDGIF_CFG_H
#define WDGIF_CFG_H

/* -----------------------------------------------------------------------
 * DET（Default Error Tracer）関連定数
 *
 * ModuleId は SWS 本文には明記されないため、AUTOSAR_TR_BSWModuleList
 * （Release 4.3.1、docs/ 配下）の「List of Basic Software Modules」表で
 * Watchdog Interface (WdgIf) に割り当てられた固定値 43 を使う。
 *
 * 開発エラーコード・ApiId は docs/4.3.1/AUTOSAR_SWS_WatchdogInterface.pdf
 * ([SWS_WdgIf_00006] 7.2.1 Development Errors 表、8.3 章 Service ID[hex])
 * を実測して確認済み。
 * ----------------------------------------------------------------------- */

/** AUTOSAR Watchdog Interface の ModuleId（AUTOSAR_TR_BSWModuleList 参照、固定値 43） */
#define WDGIF_MODULE_ID  43U

/** 開発エラーコード（[SWS_WdgIf_00006] 7.2.1 表より。WDGIF_E_INV_POINTER
 *  (0x02、ポインタ引数を取る API 向け) は本プロジェクトの WdgIf API に
 *  該当する引数がないため未使用） */
#define WDGIF_E_PARAM_DEVICE   0x01U  /**< Device が WDGIF_DEVICE_0 以外 */
#define WDGIF_E_PARAM_POINTER  0x03U  /**< NULL ポインタ                */

/** ApiId（各関数の Doxygen \ServiceID タグと一致させること。
 *  [SWS_WdgIf_00042]/[00044]/[00046] の Service ID[hex] と一致させた。
 *  実 AUTOSAR の WdgIf には Init が存在しない（[SWS_WdgIf_00018] により
 *  下位ドライバが 1 個の構成では省略可、Wdg_Init は WdgM が直接呼ぶ設計。
 *  詳細は WdgIf.c 冒頭のコメント参照）ため、対応する ApiId も定義しない。 */
#define WDGIF_API_ID_SET_MODE              0x01U
#define WDGIF_API_ID_SET_TRIGGER_CONDITION 0x02U
#define WDGIF_API_ID_GET_VERSION_INFO      0x03U

/** バージョン情報（Com/E2EXf/PduR 等の既存モジュールと同じ命名規則） */
#define WDGIF_VENDOR_ID          0U
#define WDGIF_SW_MAJOR_VERSION   1U
#define WDGIF_SW_MINOR_VERSION   0U
#define WDGIF_SW_PATCH_VERSION   0U

#endif /* WDGIF_CFG_H */
