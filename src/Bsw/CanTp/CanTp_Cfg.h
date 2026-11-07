/**
 * \file    CanTp_Cfg.h
 * \brief   CanTp プリコンパイル設定 (AUTOSAR SWS_CanTp 準拠)
 * \details ISO 15765-2 トランスポートプロトコルモジュールのコンパイル時定数を定義する。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツール
 *          (EB tresos / Vector DaVinci 等) が生成するファイルに相当する。
 *
 *          本プロジェクトの設定:
 *            - 診断チャネル 1 系統 (CAN 0x7E0 RX / CAN 0x7E8 TX)
 *            - BS=0 (ブロックサイズなし: FC 待ちなしで全 CF 送信)
 *            - STmin=0ms (CF 間の最小分離時間なし)
 *            - RX/TX バッファ 32 バイト (最大 UDS ペイロード対応)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef CANTP_CFG_H
#define CANTP_CFG_H

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Platform_Types.h"
#include "CanIf_Cfg.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

/* -----------------------------------------------------------------------
 * DET（Default Error Tracer）関連定数
 *
 * SWS_CanTp 7.4.1 Development Errors 表（-table 抽出で確定した値）に基づく
 * 開発エラーコード。ModuleId は SWS 本文には明記されないため、
 * AUTOSAR_TR_BSWModuleList（Release 4.3.1、docs/ 配下）の
 * 「List of Basic Software Modules」表で CAN Transport Layer (CanTp) に
 * 割り当てられた固定値 35 を使う。
 *
 * [SWS_CanTp_00031]: CanTp_Init/GetVersionInfo を除く全 API は、未初期化
 * 状態で呼ばれた場合に CANTP_E_UNINIT を報告しなければならない。本実装は
 * CanTp_ConfigType を保持しない簡略設計のため、初期化済みかどうかを示す
 * 専用フラグ（CanTp_Initialized）を別途持つ。
 * ----------------------------------------------------------------------- */
/** AUTOSAR CAN Transport Layer の ModuleId（AUTOSAR_TR_BSWModuleList 参照、固定値 35） */
#define CANTP_MODULE_ID  35U

/** 開発エラーコード（7.4.1 表より、実際に使用する分のみ） */
#define CANTP_E_PARAM_POINTER   0x03U  /* [SWS_CanTp_00321 等]: NULL ポインタチェック */
#define CANTP_E_UNINIT          0x20U  /* [SWS_CanTp_00031]: 未初期化時の API 呼び出し */
#define CANTP_E_INVALID_TX_ID   0x30U  /* TxSduId（CanTp_Transmit）または TxPduId（CanTp_TxConfirmation）が、本実装の固定チャネル ID と不一致 */
#define CANTP_E_INVALID_RX_ID   0x40U  /* RxPduId（CanTp_RxIndication）が、本実装の固定チャネル ID と不一致 */

/** クラシック CAN のフレーム（N-PDU）の長さ [バイト]。クラシックの送受信では、常に 8 バイトへパディングする
 *  （受信パディング ON、[SWS_CanTp_00344] 他）。 */
#define CANTP_FRAME_LEN         8U

/** CAN FD のフレーム（N-PDU）の最大の長さ [バイト]（ISO 15765-2 の TX_DL に相当。[SWS_CanTp_00354]）。
 *  要求が FD のフレームで届いたときだけ、応答も FD のフレーム（最大でこの長さ）で返す。
 *  CanIf の、診断応答の送信 PDU の設定 DLC（CanIf_PBCfg.c）以下にすること。 */
#define CANTP_FD_FRAME_LEN      64U

/** ランタイムエラーコード（7.4.2 [SWS_CanTp_00352] 表）。Det_ReportRuntimeError() で報告する。 */
#define CANTP_E_PADDING         0x70U  /* 長さが 8 バイト未満の PDU を受信した（パディング ON の前提に反する） */
#define CANTP_E_COM             0xB0U  /* [SWS_CanTp_00229]: タイムアウト以外の理由で、受信・送信を中断した */
#define CANTP_E_RX_COM          0xC0U  /* [SWS_CanTp_00229]: 受信がタイムアウト（N_Cr など）で中断した */
#define CANTP_E_TX_COM          0xD0U  /* [SWS_CanTp_00229]: 送信がタイムアウト（N_Bs、N_As、N_Cs など）で中断した */

/** ApiId（各関数の Doxygen \ServiceID タグと一致させること。値は SWS 8.x 章の
 *  「Service ID[hex]」記載を実測して確認済み） */
/* cppcheck-suppress misra-c2012-2.5 */
#define CANTP_API_ID_INIT              0x01U
#define CANTP_API_ID_TRANSMIT          0x49U
#define CANTP_API_ID_RX_INDICATION     0x42U
#define CANTP_API_ID_TX_CONFIRMATION   0x40U
#define CANTP_API_ID_MAIN_FUNCTION     0x06U
#define CANTP_API_ID_GET_VERSION_INFO  0x07U

/** バージョン情報（SWS_CanTp、Com/E2EXf/PduR 等の既存モジュールと同じ命名規則） */
#define CANTP_VENDOR_ID          0U
#define CANTP_SW_MAJOR_VERSION   1U
#define CANTP_SW_MINOR_VERSION   0U
#define CANTP_SW_PATCH_VERSION   0U

/* @@GEN-BEGIN cantp-params  生成元: config/data/CanTp.json  （自動生成: 手編集禁止） */
/* -----------------------------------------------------------------------
 * フロー制御パラメータ (AUTOSAR CanTp N_USData.req の N_Ar, N_Bs, N_Cr)
 * ----------------------------------------------------------------------- */

/** ブロックサイズ (BS): 0 = ブロック間 FC なし (すべての CF を連続送信) */
#define CANTP_BLOCK_SIZE  0U

/** 最小分離時間 (STmin): 0 = CF 間の最小待ち時間なし */
#define CANTP_ST_MIN  0U

/* -----------------------------------------------------------------------
 * タイムアウト値 (ms)
 * ISO 15765-2 Table 8: N_Ar / N_Br / N_Cr / N_Bs / N_Cs
 * ----------------------------------------------------------------------- */

/** N_Cr タイムアウト: 次の CF を受信するまでの最大待機時間 (ms)
 *  ISO 15765-2 Table 8 の標準値である 1000ms を採用する。 */
#define CANTP_N_CR_TIMEOUT_MS  1000UL

/** N_Bs タイムアウト: FF 送信後に FC を受信するまでの最大待機時間 (ms)
 *  ISO 15765-2 Table 8 の標準値は 1000ms だが、手動テスト (Cangaroo で FC を
 *  手動送信) に対応するため、本実装ではあえて長い 5000ms に設定している。 */
#define CANTP_N_BS_TIMEOUT_MS  5000UL

/** N_As タイムアウト: CF の物理送信が同期的に失敗し続けた場合に、そのまま
 *  無期限リトライさせずセッションを中断するまでの最大許容時間 (ms)。
 *  本実装は非同期の TxConfirmation 経路が常に E_OK 固定（Can.c/CanIf.c 参照）
 *  で真の N_As（送信確認待ち）を表現できないため、代わりに
 *  Can_Write() の同期的な送信失敗が一定時間継続した場合の保護として用いる。 */
#define CANTP_N_AS_TIMEOUT_MS  1000UL

/* -----------------------------------------------------------------------
 * N-PDU のパディング（[SWS_CanTp_00059]、ECUC_CanTp_00298）
 * ----------------------------------------------------------------------- */

/** パディングバイト（CanTpPaddingByte）: SF・最後の CF・FC の、使わないバイトに入れる値（0xAA）。
 *  ISO 15765-2 は値を定めない。0x00 や 0xFF とは違う値にして、バスの上でデータとの区別をつける。 */
#define CANTP_PADDING_BYTE  0xAAU

/* -----------------------------------------------------------------------
 * PDU / N-SDU ID
 * ----------------------------------------------------------------------- */

/** CanTp が診断応答のフレーム（N-PDU）を送信する際に、CanIf_Transmit() へ渡す CanIf の TX PDU
 *  （CanIf.json の txPdus の name から求めたシンボリック名。CAN 0x7E8 の PDU。txConfirm は CANTP）。 */
#define CANTP_CANIF_TX_PDU_ID  CanIfConf_CanIfTxPduCfg_DiagResp_Tx

/** CanIf が CanTp_TxConfirmation() に渡す、TX N-PDU の ID（CanIf.json の DiagResp_Tx の上位層へ返す ID）。 */
#define CANTP_TX_NPDU_ID  2U

/** CanIf が CanTp_RxIndication() に渡す、RX N-PDU の ID（CanIf.json の DiagReq_Rx の上位層へ渡す ID）。 */
#define CANTP_RX_NPDU_ID  1U

/** PduR の TP のルーティングパス（PduR.json の tpTxPaths）が CanTp_Transmit() に渡す TX N-SDU ID (診断応答チャネル)。N-PDU の ID（txNPduId）とは別の名前空間 */
#define CANTP_TX_SDU_ID  0U

/** PduR の TP のルーティングパス（PduR.json の tpRxPaths）が、PduR_CanTpStartOfReception() などで受け取る RX N-SDU ID (診断要求チャネル)。N-PDU の ID（rxNPduId）とは別の名前空間 */
#define CANTP_RX_SDU_ID  0U

/** Dcm_StartOfReception() / Dcm_CopyRxData() / Dcm_TpRxIndication() に渡す、Dcm の RX I-PDU の ID。 */
#define CANTP_DCM_RX_PDU_ID  0U

/** Dcm_CopyTxData() / Dcm_TpTxConfirmation() に渡す、Dcm の TX I-PDU の ID。 */
#define CANTP_DCM_TX_PDU_ID  0U
/* @@GEN-END cantp-params */

#endif /* CANTP_CFG_H */
