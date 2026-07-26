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
 * SecOC_IfRxIndication は実際の SWS では SecOC_RxIndication（引数構成は
 * 同一）という名前であり、本プロジェクトは SecOC_IfTransmit との対称性の
 * ため独自に "If" を付けている。
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
#define SECOC_API_ID_MAIN_FUNCTION_TX  0x03U
#define SECOC_API_ID_DEINIT            0x05U
#define SECOC_API_ID_GET_VERSION_INFO  0x02U

/** バージョン情報（SWS_SecOC、Com/E2EXf/PduR 等の既存モジュールと同じ命名規則） */
#define SECOC_VENDOR_ID          0U
#define SECOC_SW_MAJOR_VERSION   1U
#define SECOC_SW_MINOR_VERSION   0U
#define SECOC_SW_PATCH_VERSION   0U

/** RX Secured I-PDU テーブルのエントリ数
 *  [0]=ImmobilizerCmd (CAN 0x120, KeyFobEcu からの想定) */
#define SECOC_RX_PDU_COUNT  1U

/** TX Secured I-PDU テーブルのエントリ数
 *  [0]=E2EHealthStatus (CAN 0x220、E2E 保護済みペイロードをさらに認証) */
#define SECOC_TX_PDU_COUNT  1U

#endif /* SECOC_CFG_H */
