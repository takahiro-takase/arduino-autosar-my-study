/**
 * \file    E2E_Cfg.h
 * \brief   E2E ライブラリ プリコンパイル設定 (AUTOSAR SWS_E2ELibrary 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef E2E_CFG_H
#define E2E_CFG_H

/** AUTOSAR E2E Library の (Module) ID（AUTOSAR_TR_BSWModuleList の
 *  「List of libraries」表参照、固定値 207） */
#define E2E_MODULE_ID  207U

/** ApiId（[SWS_E2E_00032] の Service ID[hex] を実測して確認済み） */
#define E2E_API_ID_GET_VERSION_INFO  0x14U

/** バージョン情報（Com/E2EXf/PduR 等の既存モジュールと同じ命名規則） */
#define E2E_VENDOR_ID          0U
#define E2E_SW_MAJOR_VERSION   1U
#define E2E_SW_MINOR_VERSION   0U
#define E2E_SW_PATCH_VERSION   0U

#endif /* E2E_CFG_H */
