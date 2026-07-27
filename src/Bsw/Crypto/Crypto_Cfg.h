/**
 * \file    Crypto_Cfg.h
 * \brief   Crypto Driver プリコンパイル設定 (AUTOSAR SWS_CryptoDriver 準拠)
 * \details Crypto Driver モジュールの DET 定数・鍵テーブル定数を定義する。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツールが生成する
 *          ファイルに相当する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef CRYPTO_CFG_H
#define CRYPTO_CFG_H

/* -----------------------------------------------------------------------
 * DET（Default Error Tracer）関連定数
 *
 * SWS_Crypto 7.x Development Errors 表に基づく開発エラーコード。
 * ModuleId は SWS 本文には明記されないため、AUTOSAR_TR_BSWModuleList
 * （Release 4.3.1、docs/ 配下）の「List of Basic Software Modules」表で
 * Crypto Driver (Crypto) に割り当てられた固定値 114 を使う。
 * ----------------------------------------------------------------------- */

/** AUTOSAR Crypto Driver の ModuleId（AUTOSAR_TR_BSWModuleList 参照、固定値 114） */
#define CRYPTO_MODULE_ID  114U

/** 開発エラーコード（docs/AUTOSAR_SWS_CryptoDriver.pdf を実測して確認済み） */
#define CRYPTO_E_UNINIT        0x00U  /* [SWS_Crypto_00057]: 未初期化時の API 呼び出し */
#define CRYPTO_E_INIT_FAILED   0x01U
#define CRYPTO_E_PARAM_POINTER 0x02U  /* [SWS_Crypto_00047 等]: NULL ポインタチェック */
#define CRYPTO_E_PARAM_HANDLE  0x04U  /* [SWS_Crypto_00058 等]: objectId/cryptoKeyId が範囲外 */
#define CRYPTO_E_PARAM_VALUE   0x05U  /* [SWS_Crypto_00079 等]: keyLength=0 等の不正値 */

/** ランタイムエラー（[SWS_Crypto_00194] 実測。Development Error とは別チャネルで、
 *  本プロジェクトは Dem 未接続のため Det_ReportError は呼ばず DET_LOGW のみ行う）。
 *  実車の値は 9（Runtime Error Types 表内の通し番号）だが、DET ログ上の
 *  区別用としてこのファイル内でのみ意味を持つ値として定義する。 */
#define CRYPTO_E_KEY_NOT_VALID  9U

/** ApiId（値は docs/AUTOSAR_SWS_CryptoDriver.pdf の「Service ID[hex]」記載を
 *  実測して確認済み） */
#define CRYPTO_API_ID_INIT              0x00U
#define CRYPTO_API_ID_GET_VERSION_INFO  0x01U
#define CRYPTO_API_ID_KEY_ELEMENT_SET   0x04U
#define CRYPTO_API_ID_KEY_SET_VALID     0x05U
#define CRYPTO_API_ID_PROCESS_JOB       0x03U

/** 鍵要素 ID。実 AUTOSAR は鍵ごとに複数の要素（暗号鍵本体・アルゴリズム情報等）
 *  を持てるが、本プロジェクトは AES-128 鍵本体の1要素のみを扱う。KeyM 仕様書
 *  [SWS_KeyM_00016] が "key element id `1'" と明記している値に合わせる。 */
#define CRYPTO_KEY_ELEMENT_ID_CIPHER_KEY  1U

/** バージョン情報（Com/E2EXf/PduR 等の既存モジュールと同じ命名規則） */
#define CRYPTO_VENDOR_ID          0U
#define CRYPTO_SW_MAJOR_VERSION   1U
#define CRYPTO_SW_MINOR_VERSION   0U
#define CRYPTO_SW_PATCH_VERSION   0U

/** 本プロジェクトの Crypto Driver は単一の Crypto Driver Object のみを持つ
 *  （CanIf が単一 CAN コントローラに固定しているのと同じ簡略化）。
 *  CryIf_ProcessJob() から渡される channelId は無視し、常にこの ID で
 *  扱う。 */
#define CRYPTO_OBJECT_ID  0U

/* -----------------------------------------------------------------------
 * 鍵テーブル（実車は KeyM 等による鍵のプロビジョニング・保護が必須だが、
 * 本実装は学習のためコンパイル時の固定鍵テーブルに簡略化する）
 * ----------------------------------------------------------------------- */

/** RX Secured I-PDU「ImmobilizerCmd」検証用の鍵（KeyFobEcu と共有） */
#define CRYPTO_KEY_IMMOBILIZER_CMD    0U

/** TX Secured I-PDU「E2EHealthStatus」生成用の鍵 */
#define CRYPTO_KEY_E2E_HEALTH_STATUS  1U

/** 鍵テーブルの総数 */
#define CRYPTO_KEY_COUNT  2U

#endif /* CRYPTO_CFG_H */
