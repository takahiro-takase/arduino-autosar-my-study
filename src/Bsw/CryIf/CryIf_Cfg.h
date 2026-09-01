/**
 * \file    CryIf_Cfg.h
 * \brief   Crypto Interface プリコンパイル設定 (AUTOSAR SWS_CryptoInterface 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef CRYIF_CFG_H
#define CRYIF_CFG_H

/* -----------------------------------------------------------------------
 * DET（Default Error Tracer）関連定数
 *
 * ModuleId は SWS 本文には明記されないため、AUTOSAR_TR_BSWModuleList
 * （Release 4.3.1、docs/ 配下）の「List of Basic Software Modules」表で
 * Crypto Interface (CryIf) に割り当てられた固定値 112 を使う。
 * ----------------------------------------------------------------------- */

/** AUTOSAR Crypto Interface の ModuleId（AUTOSAR_TR_BSWModuleList 参照、固定値 112） */
#define CRYIF_MODULE_ID  112U

/** 開発エラーコード（docs/AUTOSAR_SWS_CryptoInterface.pdf を実測して確認済み） */
#define CRYIF_E_UNINIT        0x00U  /* [SWS_CryIf_00027 等]: 未初期化時の API 呼び出し */
#define CRYIF_E_INIT_FAILED   0x01U
#define CRYIF_E_PARAM_POINTER 0x02U  /* [SWS_CryIf_00029 等]: NULL ポインタチェック */
#define CRYIF_E_PARAM_HANDLE  0x03U  /* [SWS_CryIf_00028 等]: channelId が範囲外 */
#define CRYIF_E_PARAM_VALUE   0x04U  /* [SWS_CryIf_00053]: keyLength=0 等の不正値 */

/** ApiId（値は docs/AUTOSAR_SWS_CryptoInterface.pdf の「Service ID[hex]」記載を
 *  実測して確認済み） */
#define CRYIF_API_ID_INIT              0x00U
#define CRYIF_API_ID_GET_VERSION_INFO  0x01U
#define CRYIF_API_ID_KEY_ELEMENT_SET   0x04U
#define CRYIF_API_ID_KEY_SET_VALID     0x05U
#define CRYIF_API_ID_KEY_ELEMENT_GET   0x06U
#define CRYIF_API_ID_PROCESS_JOB       0x03U

/** バージョン情報（Com/E2EXf/PduR 等の既存モジュールと同じ命名規則） */
#define CRYIF_VENDOR_ID          0U
#define CRYIF_SW_MAJOR_VERSION   1U
#define CRYIF_SW_MINOR_VERSION   0U
#define CRYIF_SW_PATCH_VERSION   0U

/** 本プロジェクトの CryIf は単一の Crypto チャネル（= 単一 Crypto Driver
 *  Object へのルーティング）のみを持つ（CanIf が単一 CAN コントローラに
 *  固定しているのと同じ簡略化）。 */
#define CRYIF_CHANNEL_ID  0U

#endif /* CRYIF_CFG_H */
