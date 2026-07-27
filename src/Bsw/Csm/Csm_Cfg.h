/**
 * \file    Csm_Cfg.h
 * \brief   Crypto Service Manager プリコンパイル設定 (AUTOSAR SWS_CryptoServiceManager 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef CSM_CFG_H
#define CSM_CFG_H

/* -----------------------------------------------------------------------
 * DET（Default Error Tracer）関連定数
 *
 * ModuleId は SWS 本文には明記されないため、AUTOSAR_TR_BSWModuleList
 * （Release 4.3.1、docs/ 配下）の「List of Basic Software Modules」表で
 * Crypto Service Manager (Csm) に割り当てられた固定値 110 を使う。
 *
 * [SWS_Csm_91008]: Csm_Init を除く全 API は、未初期化状態で呼ばれた場合に
 * CSM_E_UNINIT を報告しなければならない。Csm_GetVersionInfo もこの例外の
 * 対象外（＝チェックが必要）である点に注意。他の多くのモジュールで
 * GetVersionInfo が UNINIT 例外になっているのとは異なる（実測で確認済み）。
 * ----------------------------------------------------------------------- */

/** AUTOSAR Crypto Service Manager の ModuleId（AUTOSAR_TR_BSWModuleList 参照、固定値 110） */
#define CSM_MODULE_ID  110U

/** 開発エラーコード（docs/AUTOSAR_SWS_CryptoServiceManager.pdf を実測して確認済み） */
#define CSM_E_PARAM_POINTER      0x01U  /* [SWS_Csm_91009]: NULL ポインタチェック */
#define CSM_E_SMALL_BUFFER       0x03U  /* [SWS_Csm_91012]: 出力バッファ不足 */
#define CSM_E_PARAM_HANDLE       0x04U  /* [SWS_Csm_91011]: jobId が範囲外 */
#define CSM_E_UNINIT             0x05U  /* [SWS_Csm_91008]: 未初期化時の API 呼び出し */
#define CSM_E_INIT_FAILED        0x07U
#define CSM_E_SERVICE_NOT_STARTED 0x09U /* [SWS_Csm_91010]: CryIf が未初期化 */

/** ApiId（値は docs/AUTOSAR_SWS_CryptoServiceManager.pdf の「Service ID[hex]」
 *  記載を実測して確認済み） */
#define CSM_API_ID_INIT               0x00U
#define CSM_API_ID_GET_VERSION_INFO   0x3BU
#define CSM_API_ID_MAC_GENERATE       0x60U
#define CSM_API_ID_MAC_VERIFY         0x61U
#define CSM_API_ID_KEY_SET_VALID      0x67U
#define CSM_API_ID_KEY_ELEMENT_SET    0x78U

/** バージョン情報（Com/E2EXf/PduR 等の既存モジュールと同じ命名規則） */
#define CSM_VENDOR_ID          0U
#define CSM_SW_MAJOR_VERSION   1U
#define CSM_SW_MINOR_VERSION   0U
#define CSM_SW_PATCH_VERSION   0U

/* -----------------------------------------------------------------------
 * ジョブ ID（Csm_MacGenerate/Csm_MacVerify の jobId 引数に渡す）
 * 各ジョブがどのプリミティブ（MACGENERATE/MACVERIFY）でどの鍵
 * （CRYPTO_KEY_*）を使うかは Csm_PBCfg.c の Csm_JobConfigData で決める
 * （SecOC は jobId しか知らず、鍵そのものへは一切アクセスしない）。
 * ----------------------------------------------------------------------- */

/** RX Secured I-PDU「ImmobilizerCmd」の MAC 検証ジョブ */
#define CSM_JOB_ID_IMMOBILIZER_CMD_VERIFY     0U

/** TX Secured I-PDU「E2EHealthStatus」の MAC 生成ジョブ */
#define CSM_JOB_ID_E2E_HEALTH_STATUS_GENERATE 1U

/** ジョブテーブルの総数 */
#define CSM_JOB_COUNT  2U

#endif /* CSM_CFG_H */
