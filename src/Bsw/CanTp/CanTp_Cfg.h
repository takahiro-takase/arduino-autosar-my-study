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
 * バッファサイズ
 * ----------------------------------------------------------------------- */

/** RX N-SDU バッファサイズ (FF/CF 組立用): 32 バイト
 *  対応可能な最大 UDS ペイロード。4 DTC 応答 = 19 バイト + 余裕 */
#define CANTP_RX_BUFFER_SIZE      32U

/** TX N-SDU バッファサイズ (FF/CF 分割用): 32 バイト */
#define CANTP_TX_BUFFER_SIZE      32U

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

/** N_Cr タイムアウト: 次の CF を受信するまでの最大待機時間 (ms) */
#define CANTP_N_CR_TIMEOUT_MS     150UL

/** N_Bs タイムアウト: FF 送信後に FC を受信するまでの最大待機時間 (ms)
 *  手動テスト (Cangaroo で FC を手動送信) に対応するため 5000ms に設定。
 *  実運用では 150ms が標準値。 */
#define CANTP_N_BS_TIMEOUT_MS     5000UL

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
