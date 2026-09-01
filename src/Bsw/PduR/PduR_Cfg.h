/**
 * \file    PduR_Cfg.h
 * \brief   PDU ルータ プリコンパイル設定 (AUTOSAR SWS_PDURouter 準拠)
 * \details PDU ルータモジュールのプリコンパイル設定を定義する。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツール
 *          (EB tresos / Vector DaVinci 等) が生成するファイルに相当する。
 *          RX / TX ルーティングパス数をコンパイル時定数として提供し、
 *          PduR_PBCfg.c での配列サイズ指定に使用する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef PDUR_CFG_H
#define PDUR_CFG_H

#include "Platform_Types.h"

/* -----------------------------------------------------------------------
 * DET（Default Error Tracer）関連定数
 *
 * SWS_PduR_00100 の Development Errors 表（7.8.1 章）に基づく開発エラー
 * コード。ModuleId は SWS 本文には明記されないため、AUTOSAR_TR_BSWModuleList
 * （Release 4.3.1、docs/ 配下）の「List of Basic Software Modules」表で
 * PDU Router (PduR) に割り当てられた固定値 51 を使う。
 *
 * [SWS_PduR_00119]: PduR_Init/PduR_GetVersionInfo を除く全 API は、未初期化
 * 状態で呼ばれた場合に PDUR_E_UNINIT を報告しなければならない（CanIf の
 * 「黙って戻る」方式とは異なる点に注意）。
 * ----------------------------------------------------------------------- */

/** AUTOSAR PDU Router の ModuleId（AUTOSAR_TR_BSWModuleList 参照、固定値 51） */
#define PDUR_MODULE_ID  51U

/** 開発エラーコード（SWS_PduR_00100 表より、実際に使用する分のみ） */
#define PDUR_E_INIT_FAILED  0x00U  /* 不正な設定ポインタ／設定内容 */
#define PDUR_E_UNINIT       0x01U  /* [SWS_PduR_00119]: 未初期化時の API 呼び出し */
#define PDUR_E_PDU_ID_INVALID 0x02U  /* [SWS_PduR_00221]: 一致するルーティングパスなし */
#define PDUR_E_PARAM_POINTER  0x09U  /* NULL ポインタチェック */

/** ApiId（各関数の Doxygen \ServiceID タグと一致させること。値は SWS 8.x 章の
 *  「Service ID[hex]」記載を実測して確認済み） */
#define PDUR_API_ID_INIT                0xF0U
#define PDUR_API_ID_TRANSMIT            0x49U
#define PDUR_API_ID_RX_INDICATION       0x42U
#define PDUR_API_ID_TX_CONFIRMATION     0x40U
/** SecOC 専用 TX エントリポイント（PduR_SecOCTransmit）。PduR 自身の SWS には
 *  この名前の独立した API エントリは存在せず、generic な
 *  PduR_<User:Up>Transmit テンプレート（Service ID 0x49）が SecOC 向けに
 *  実体化されたインスタンスに相当するため、PDUR_API_ID_TRANSMIT と同じ値を
 *  そのまま使う（0x4A は PduR_<User:Up>CancelTransmit の Service ID であり
 *  「未使用の隣接値」ではないため、以前の割り当ては誤りだった）。 */
#define PDUR_API_ID_SECOC_TRANSMIT      PDUR_API_ID_TRANSMIT
/** SecOC 向け TX 確認エントリポイント（PduR_SecOCTxConfirmation）。CanIf 向け
 *  の PduR_CanIfTxConfirmation と同じく generic な PduR_<Lo>TxConfirmation
 *  テンプレート（Service ID 0x40）の実体化のため、PDUR_API_ID_TX_CONFIRMATION
 *  と同じ値を使う。 */
#define PDUR_API_ID_SECOC_TX_CONFIRMATION PDUR_API_ID_TX_CONFIRMATION
#define PDUR_API_ID_GET_VERSION_INFO    0xF1U

/** バージョン情報（SWS_PduR_00338、Com/E2EXf 等の既存モジュールと同じ命名規則） */
#define PDUR_VENDOR_ID          0U
#define PDUR_SW_MAJOR_VERSION   1U
#define PDUR_SW_MINOR_VERSION   0U
#define PDUR_SW_PATCH_VERSION   0U

/* -----------------------------------------------------------------------
 * プリコンパイル設定定数
 * ----------------------------------------------------------------------- */

/** RX ルーティングパス数
 *  DaVinci: /ActiveEcuC/PduR/PduRConfig/PduRRoutingTable 内の
 *           RECEIVE 方向 PduRRoutingPath ノード数
 *  パス 0: CAN 0x100 → COM   (EngineInfo,  エンジン ECU)
 *  パス 1: CAN 0x7E0 → CanTp (UDS 診断要求, 診断ツール)
 *  パス 2: CAN 0x110 → COM   (AbsInfo,     ABS ECU)
 *  パス 3: CAN 0x120 → SecOC (ImmobilizerCmd, KeyFobEcu 想定。検証成功後
 *           SecOC 自身が Com_RxIndication() を直接呼ぶため、COM への
 *           配信先はここには現れない） */
#define PDUR_RX_PATH_COUNT   4U

/** RX パス 0 の配信先数（COM のみ） */
#define PDUR_RX_DEST_COUNT_PATH0  1U

/** RX パス 1 の配信先数（CanTp のみ） */
#define PDUR_RX_DEST_COUNT_PATH1  1U

/** RX パス 2 の配信先数（COM のみ: AbsInfo は ABS ECU からの受信専用） */
#define PDUR_RX_DEST_COUNT_PATH2  1U

/** RX パス 3 の配信先数（SecOC のみ） */
#define PDUR_RX_DEST_COUNT_PATH3  1U

/** TX ルーティングパス数
 *  パス 0: COM   → CanIf TxPduId=0 (CAN 0x200, EngineState)
 *  パス 1: CanTp → CanIf TxPduId=1 (CAN 0x7E8, UDS 診断応答)
 *  パス 2: COM   → CanIf TxPduId=3 (CAN 0x210, WarningStatus Signal Group)
 *  パス 3: COM   → CanIf TxPduId=4 (CAN 0x220, E2EHealthStatus PERIODIC。
 *          E2E Profile05 保護のみ。以前は SecOC を経由していたが撤去済み)
 *  パス 4: COM   → CanIf TxPduId=5 (CAN 0x230, ImmobilizerStatus。
 *          Signal Gateway の転送先。DIRECT)
 *  SrcPduId は COM と CanTp が共通の名前空間として PduR_Transmit() へ渡すため、
 *  各パスで重複しない値を割り当てること。 */
#define PDUR_TX_PATH_COUNT   5U

#endif /* PDUR_CFG_H */
