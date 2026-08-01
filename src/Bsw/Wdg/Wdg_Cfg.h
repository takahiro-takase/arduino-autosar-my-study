/**
 * \file    Wdg_Cfg.h
 * \brief   Watchdog Driver プリコンパイル設定 (AUTOSAR SWS_Wdg 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef WDG_CFG_H
#define WDG_CFG_H

/* -----------------------------------------------------------------------
 * DET（Default Error Tracer）関連定数
 *
 * ModuleId は SWS 本文には明記されないため、AUTOSAR_TR_BSWModuleList
 * （Release 4.3.1、docs/ 配下）の「List of Basic Software Modules」表で
 * Watchdog Driver (Wdg) に割り当てられた固定値 102 を使う。
 *
 * 開発エラーコード・ApiId は docs/4.3.1/AUTOSAR_SWS_WatchdogDriver.pdf
 * ([SWS_Wdg_00010] 7.2.1 Development Errors 表、8.3 章 Service ID[hex])
 * を実測して確認済み。WDG_E_PARAM_CONFIG（Value 0x12、コンフィグ内容の
 * 妥当性検証用）・WDG_E_INIT_FAILED（Value 0x15、コンフィグセット選択
 * 不正用）は、本プロジェクトが単一コンフィグしか持たず内容検証も NULL
 * チェックのみのため未使用。WDG_E_MODE_FAILED/WDG_E_DISABLE_REJECTED は
 * SWS 上 Extended Production Error（開発エラーとは別区分）で、本プロジェクト
 * は他モジュール同様 Production Error の仕組み自体を持たないため未実装
 * （Wdg.c 冒頭のコメント参照。無効化要求は DET_LOGW のみで E_NOT_OK を返す）。
 * ----------------------------------------------------------------------- */

/** AUTOSAR Watchdog Driver の ModuleId（AUTOSAR_TR_BSWModuleList 参照、固定値 102） */
#define WDG_MODULE_ID  102U

/** 開発エラーコード（[SWS_Wdg_00010] 7.2.1 表より、実際に使用する分のみ） */
#define WDG_E_DRIVER_STATE   0x10U  /**< 未初期化時の API 呼び出し（AUTOSAR 実仕様では
                                     *   WDG_IDLE 以外の状態での SetMode 呼び出し全般を指すが、
                                     *   本実装では未初期化チェックにのみ使用） */
#define WDG_E_PARAM_MODE     0x11U  /**< Mode が範囲外                    */
#define WDG_E_PARAM_TIMEOUT  0x13U  /**< timeout が最大許容値超過          */
#define WDG_E_PARAM_POINTER  0x14U  /**< NULL ポインタ（ConfigPtr/versioninfo） */

/** ApiId（各関数の Doxygen \ServiceID タグと一致させること。
 *  [SWS_Wdg_00106]/[00107]/[00155]/[00109] の Service ID[hex] と一致させた。
 *  0x02（Wdg_DeInit 相当）は実 AUTOSAR の Wdg にも存在しないため欠番のまま。 */
#define WDG_API_ID_INIT                  0x00U
#define WDG_API_ID_SET_MODE              0x01U
#define WDG_API_ID_SET_TRIGGER_CONDITION 0x03U
#define WDG_API_ID_GET_VERSION_INFO      0x04U

/** バージョン情報（Com/E2EXf/PduR 等の既存モジュールと同じ命名規則） */
#define WDG_VENDOR_ID          0U
#define WDG_SW_MAJOR_VERSION   1U
#define WDG_SW_MINOR_VERSION   0U
#define WDG_SW_PATCH_VERSION   0U

#endif /* WDG_CFG_H */
