/**
 * \file    SecOC_Cfg.h
 * \brief   SecOC プリコンパイル設定 (AUTOSAR SWS_SecureOnboardCommunication 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef SECOC_CFG_H
#define SECOC_CFG_H

#include "Platform_Types.h"

/* -----------------------------------------------------------------------
 * DET（Default Error Tracer）関連定数
 *
 * SWS_SecOC 7.7.1 Development Errors 表（SWS_SecOC_00101）に基づく開発
 * エラーコード。ModuleId は SWS 本文には明記されないため、
 * AUTOSAR_TR_BSWModuleList（Release 4.3.1、docs/ 配下）の
 * 「List of Basic Software Modules」表で Secure Onboard Communication
 * (SecOC) に割り当てられた固定値 150 を使う。
 *
 * 本モジュールは AUTOSAR 標準の Doxygen \ServiceID タグを元々持たなかった
 * ため、SWS 8.x 章の「Service ID[hex]」記載を実測して新規に付与する。
 * かつて本プロジェクトは SecOC_IfTransmit との対称性を理由に受信側にも
 * 独自に "If" を付けて SecOC_IfRxIndication としていたが、実際の SWS は
 * SecOC_RxIndication（"If" なし。Tp 層側は別途 SecOC_TpRxIndication）と
 * 非対称な命名のため、2026-08 のシグネチャ準拠サーベイで実仕様の名前に
 * 修正した。
 * ----------------------------------------------------------------------- */

/** AUTOSAR Secure Onboard Communication の ModuleId
 *  （AUTOSAR_TR_BSWModuleList 参照、固定値 150） */
#define SECOC_MODULE_ID  150U

/** 開発エラーコード（SWS_SecOC 7.7.1 表より、実際に使用する分のみ） */
#define SECOC_E_PARAM_POINTER      0x01U
#define SECOC_E_UNINIT             0x02U
#define SECOC_E_INVALID_PDU_SDU_ID 0x03U

/** ApiId（値は SWS 8.x 章の「Service ID[hex]」記載を実測して確認済み） */
#define SECOC_API_ID_INIT              0x01U
#define SECOC_API_ID_RX_INDICATION     0x42U
#define SECOC_API_ID_IF_TRANSMIT       0x49U
#define SECOC_API_ID_TX_CONFIRMATION   0x40U
#define SECOC_API_ID_MAIN_FUNCTION_TX  0x03U
#define SECOC_API_ID_MAIN_FUNCTION_RX  0x06U
#define SECOC_API_ID_DEINIT            0x05U
#define SECOC_API_ID_GET_VERSION_INFO  0x02U
#define SECOC_API_ID_VERIFY_STATUS_OVERRIDE  0x0BU

/** バージョン情報（SWS_SecOC、Com/E2EXf/PduR 等の既存モジュールと同じ命名規則） */
#define SECOC_VENDOR_ID          0U
#define SECOC_SW_MAJOR_VERSION   1U
#define SECOC_SW_MINOR_VERSION   0U
#define SECOC_SW_PATCH_VERSION   0U

/** RX Secured I-PDU テーブルのエントリ数
 *  [0]=ImmobilizerCmd (CAN 0x120, KeyFobEcu からの想定) */
#define SECOC_RX_PDU_COUNT  1U

/** TX Secured I-PDU テーブルのエントリ数。現在 TX 方向で SecOC を使う PDU は
 *  無い（以前は E2EHealthStatus (CAN 0x220) を保護していたが、E2E Profile05
 *  単体保護へ切り替えて撤去した）。RX 方向 (ImmobilizerCmd) は引き続き使用中。 */
#define SECOC_TX_PDU_COUNT  0U

#endif /* SECOC_CFG_H */
