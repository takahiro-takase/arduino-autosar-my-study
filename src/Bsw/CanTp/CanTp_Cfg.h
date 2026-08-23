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

#include "Platform_Types.h"

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
#define CANTP_E_INVALID_TX_ID   0x30U  /* TxSduId が本実装の固定チャネル ID と不一致 */
#define CANTP_E_INVALID_RX_ID   0x40U  /* RxPduId が本実装の固定チャネル ID と不一致 */

/** ApiId（各関数の Doxygen \ServiceID タグと一致させること。値は SWS 8.x 章の
 *  「Service ID[hex]」記載を実測して確認済み） */
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

/* -----------------------------------------------------------------------
 * バッファサイズ
 * ----------------------------------------------------------------------- */

/** RX N-SDU バッファサイズ (FF/CF 組立用): 32 バイト
 *  対応可能な最大 UDS 要求ペイロード。現状の最大要求は WriteDataByIdentifier
 *  CryptoKeyUpdate (0x2E, DID 0x0108) の 20 バイト（4ヘッダ+16文字キー）。 */
#define CANTP_RX_BUFFER_SIZE      32U

/** TX N-SDU バッファサイズ (FF/CF 分割用): 48 バイト
 *  Dcm_Cbk.c の DCM_TX_BUF_SIZE（3 + DEM_EVENT_COUNT*4、subFunc 0x02/0x0A の
 *  DTC一覧応答の最大長）以上でなければならない。以前は固定値 32 のままで、
 *  DEM_EVENT_COUNT=10 なら DCM_TX_BUF_SIZE=43 バイトとなり 32 バイトの
 *  バッファに収まらず、subFunc 0x0A（全DTC無条件応答、常に最大長になる）の
 *  応答が CanTp_Transmit() で "invalid len" として毎回拒否されるバグが
 *  実機ログで発覚した（Dcm_Cbk.c 側は DEM_EVENT_COUNT 変化に自動追従する
 *  よう既に修正済みだったが、このバッファは連動していなかった）。
 *  48 = FF(6バイト) + CF×6(7バイト×6) というISO-TPのフレーム境界に
 *  ちょうど一致する値で、最後のCFにパディングの無駄が出ない。43バイトへの
 *  最小限の対応（44等）ではなく、DEM_EVENT_COUNT が今後 11 に増えても
 *  （3+11*4=47 バイト）このバッファを再度触らずに済む余裕を持たせている
 *  （このバグは Dcm_TxBuf 自体の教訓の後に CanTp 側で再発したものであり、
 *  同種の再発を避けるため）。RAM使用率に対して数バイトの余裕は無視できる
 *  コストである一方、DEM_EVENT_COUNT を大きく変更する場合はこの値も
 *  再確認すること。 */
#define CANTP_TX_BUFFER_SIZE      48U

/* -----------------------------------------------------------------------
 * フロー制御パラメータ (AUTOSAR CanTp N_USData.req の N_Ar, N_Bs, N_Cr)
 * ----------------------------------------------------------------------- */

/** ブロックサイズ (BS): 0 = ブロック間 FC なし (すべての CF を連続送信) */
#define CANTP_BLOCK_SIZE          0U

/** 最小分離時間 (STmin): 0 = CF 間の最小待ち時間なし */
#define CANTP_ST_MIN              0U

/* -----------------------------------------------------------------------
 * タイムアウト値 (ms)
 * ISO 15765-2 Table 8: N_Ar / N_Br / N_Cr / N_Bs / N_Cs
 * ----------------------------------------------------------------------- */

/** N_Cr タイムアウト: 次の CF を受信するまでの最大待機時間 (ms)
 *  ISO 15765-2 Table 8 の標準値である 1000ms を採用する。 */
#define CANTP_N_CR_TIMEOUT_MS     1000UL

/** N_Bs タイムアウト: FF 送信後に FC を受信するまでの最大待機時間 (ms)
 *  ISO 15765-2 Table 8 の標準値は 1000ms だが、手動テスト (Cangaroo で FC を
 *  手動送信) に対応するため、本実装ではあえて長い 5000ms に設定している。 */
#define CANTP_N_BS_TIMEOUT_MS     5000UL

/** N_As タイムアウト: CF の物理送信が同期的に失敗し続けた場合に、そのまま
 *  無期限リトライさせずセッションを中断するまでの最大許容時間 (ms)。
 *  本実装は非同期の TxConfirmation 経路が常に E_OK 固定（Can.c/CanIf.c 参照）
 *  で真の N_As（送信確認待ち）を表現できないため、代わりに
 *  Can_Write() の同期的な送信失敗が一定時間継続した場合の保護として用いる。 */
#define CANTP_N_AS_TIMEOUT_MS     1000UL

/* -----------------------------------------------------------------------
 * PDU / N-SDU ID
 * ----------------------------------------------------------------------- */

/** CanTp が診断応答フレームを送信する際に使用する PduR TX SrcPduId。
 *  PduR_PBCfg.c TX パス 1 (CAN 0x7E8) に対応する。 */
#define CANTP_PDUR_TX_SDU_ID      1U

/** Dcm が CanTp_Transmit() に渡す TX N-SDU ID (診断応答チャネル) */
#define CANTP_TX_SDU_ID           0U

/** CanTp_RxIndication() が受け取る RX N-SDU ID (診断要求チャネル) */
#define CANTP_RX_SDU_ID           0U

#endif /* CANTP_CFG_H */
