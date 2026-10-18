/**
 * \file    CanTSyn_Cfg.h
 * \brief   CAN 時刻同期 (CanTSyn) プリコンパイル設定 (AUTOSAR SWS_CanTSyn 準拠)
 * \details 本 ECU が時間マスターとして配信する SYNC/FUP メッセージのパラメータを定義する。
 *
 *          本プロジェクトでの位置づけ:
 *            CAN ツール（PC）が ECU のシリアルログの `[<ms>ms]` と PC の時刻を
 *            対応づけるための時刻基準を、CAN バス上へ配信する。ECU のログ出力
 *            （`DET_LOG_LEVEL`）に依存せず、CAN の受信だけで ECU 時刻が分かる。
 *
 *          対応範囲（簡略版）:
 *            - 時間マスターのみ（SYNC/FUP の送信）。CRC 付きメッセージ（0x20/0x28）に
 *              対応（CANTSYN_TX_CRC_SECURED で切り替え）。時間スレーブ（受信側）、
 *              OFS/OFNS（オフセット時間ベース）、CAN FD の拡張フォーマットは対象外。
 *            - 時刻源は StbM ではなく ECU の稼働時間（Os カウンタ [ms]）。
 *              時間領域は 0 の1つ。時間ベースの状態（同期/ゲートウェイ）は持たない。
 *            将来 StbM を実装する場合は、`CanTSyn_GetNowMs()`（CanTSyn.c）を
 *            StbM_GetCurrentTime() に置き換え、本ファイルの設定値を ECUC の
 *            パラメータ（CanTSynGlobalTimeDomainId 等）へ対応づける。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様（docs/autosar/4.3.1/AUTOSAR_SWS_TimeSyncOverCAN.pdf）
 *          を参考にした学習用実装です。AUTOSAR 認証済み実装ではなく、製品への
 *          適用は想定していません。
 */
#ifndef CANTSYN_CFG_H
#define CANTSYN_CFG_H

/* ======================================================================
 * Includes
 * ====================================================================== */

#include "Std_Types.h"
#include "CanIf_Cfg.h"

/* ======================================================================
 * Definitions
 * ====================================================================== */

/* -----------------------------------------------------------------------
 * DET（Default Error Tracer）関連定数
 *
 * SWS_CanTSyn 7.6.1 Development Errors 表（[SWS_CanTSyn_00089]）に基づく開発エラーコード。
 * ModuleId は AUTOSAR_TR_BSWModuleList（Release 4.3.1、docs/ 配下）の
 * 「List of Basic Software Modules」表で Time Sync Over CAN (CanTSyn) に
 * 割り当てられた固定値 161。
 * ----------------------------------------------------------------------- */
/** AUTOSAR CanTSyn の ModuleId（AUTOSAR_TR_BSWModuleList 参照、固定値 161） */
#define CANTSYN_MODULE_ID  161U

#define CANTSYN_E_INVALID_PDUID  0x01U  /* [SWS_CanTSyn_00089]: 不正な PDU ID */
#define CANTSYN_E_UNINIT         0x02U  /* [SWS_CanTSyn_00005/00089]: 未初期化時の API 呼び出し */
#define CANTSYN_E_NULL_POINTER   0x03U  /* [SWS_CanTSyn_00089]: NULL ポインタ */
#define CANTSYN_E_PARAM          0x05U  /* [SWS_CanTSyn_00089]: 不正なパラメータ */
#define CANTSYN_E_INV_CTRL_IDX   0x06U  /* [SWS_CanTSyn_00089]: 不正な CAN チャネル番号 */

/** ApiId（各関数の Doxygen \ServiceID タグと一致させること。SWS_CanTSyn 8.1.3 の Service ID） */
#define CANTSYN_API_ID_INIT                  0x01U
#define CANTSYN_API_ID_GET_VERSION_INFO      0x02U
#define CANTSYN_API_ID_SET_TRANSMISSION_MODE 0x03U
#define CANTSYN_API_ID_TX_CONFIRMATION       0x40U
#define CANTSYN_API_ID_MAIN_FUNCTION         0x06U

/** バージョン情報（Com/E2EXf/PduR 等の既存モジュールと同じ命名規則） */
#define CANTSYN_VENDOR_ID          0U
#define CANTSYN_SW_MAJOR_VERSION   1U
#define CANTSYN_SW_MINOR_VERSION   0U
#define CANTSYN_SW_PATCH_VERSION   0U

/* -----------------------------------------------------------------------
 * 時刻同期メッセージ（SYNC/FUP）のパラメータ
 * ----------------------------------------------------------------------- */

/** 時間領域 ID（0〜15。CanTSynGlobalTimeDomainId 相当）。SYNC/FUP の Byte2 上位4bit。 */
#define CANTSYN_TIME_DOMAIN_ID  0U

/** SYNC の送信周期 [ms]（CanTSynGlobalTimeTxPeriod 相当）。 */
#define CANTSYN_TX_PERIOD_MS  1000UL

/** SYNC/FUP の送信完了通知の待ち時間 [ms]（CanTSynMasterConfirmationTimeout 相当、
 *  [SWS_CanTSyn_00033]）。超過すると送信を取り消し、次の周期で SYNC からやり直す。 */
#define CANTSYN_MASTER_CONFIRMATION_TIMEOUT_MS  100UL

/** CanTSyn_MainFunction() の呼び出し周期 [ms]（CanTSynMainFunctionPeriod 相当、
 *  Os.json のタスク周期と一致させること）。 */
#define CANTSYN_MAIN_FUNCTION_PERIOD_MS  100UL

/** 時刻同期メッセージの CanIf の TX PDU ID（CanIf.json の CanTSyn_Tx。SYNC と FUP は
 *  同じ CAN ID・同じ PDU を使い、先頭バイトの Type で区別する [SWS_CanTSyn_00008]）。 */
#define CANTSYN_CANIF_TX_PDU_ID  CanIfConf_CanIfTxPduCfg_CanTSyn_Tx

/** CanIf が TxConfirmation で CanTSyn へ返す TX PDU ID（CanIf.json の upperLayerTxPduId）。 */
#define CANTSYN_TX_PDU_ID  0U

/** 時刻同期メッセージの DLC（クラシック CAN では 8、[SWS_CanTSyn_00010]）。 */
#define CANTSYN_DLC  8U

/** 時刻同期メッセージの CAN ID（信号表 TimeSync の canId と一致させること。configurator の
 *  検査で確認する）。CRC の DataID の元にも使う。 */
#define CANTSYN_CAN_ID  0x240U

/* SYNC/FUP の Type 値（[SWS_CanTSyn_00031]）。CRC なしは 0x10/0x18、CRC 付きは 0x20/0x28 */
#define CANTSYN_TYPE_SYNC      0x10U
#define CANTSYN_TYPE_FUP       0x18U
#define CANTSYN_TYPE_SYNC_CRC  0x20U
#define CANTSYN_TYPE_FUP_CRC   0x28U

/** 送信する SYNC/FUP を CRC 付きにするか（1=CRC 付き 0x20/0x28、0=CRC なし 0x10/0x18。
 *  CanTSynGlobalTimeTxCrcSecured 相当、[SWS_CanTSyn_00031]）。 */
#define CANTSYN_TX_CRC_SECURED  1U

/** 送信する SYNC/FUP の Type（CANTSYN_TX_CRC_SECURED に従う）。 */
#define CANTSYN_TX_TYPE_SYNC  ((CANTSYN_TX_CRC_SECURED != 0U) ? CANTSYN_TYPE_SYNC_CRC : CANTSYN_TYPE_SYNC)
#define CANTSYN_TX_TYPE_FUP   ((CANTSYN_TX_CRC_SECURED != 0U) ? CANTSYN_TYPE_FUP_CRC : CANTSYN_TYPE_FUP)

/* -----------------------------------------------------------------------
 * CRC の DataID（[SWS_CanTSyn_00054]: DataID = DataIDList[SC]、メッセージ種別ごとの表）
 *
 * DataID は 8bit。CAN ID を元に、SC（0〜15）ごとに別の値、SYNC と FUP で別の値にする
 * （E2E で DataID を CAN ID と一致させている慣習に倣い、表を CAN ID から導く）:
 *   SYNC の DataID = (CAN ID + SC) の下位 8bit            （CAN ID 0x240 なら 0x40〜0x4F）
 *   FUP  の DataID = (CAN ID + 0x10 + SC) の下位 8bit     （CAN ID 0x240 なら 0x50〜0x5F）
 * SC ごとに値が違うため、別の SC のフレームを取り違えると CRC が合わなくなり、
 * SYNC と FUP を取り違えても CRC が合わない。受信側（CAN ツール）も同じ式で検証する。
 * ----------------------------------------------------------------------- */
#define CANTSYN_DATA_ID_SYNC(sc)  ((uint8)((CANTSYN_CAN_ID + (uint32)(sc)) & 0xFFU))
#define CANTSYN_DATA_ID_FUP(sc)   ((uint8)((CANTSYN_CAN_ID + 0x10UL + (uint32)(sc)) & 0xFFU))

#endif /* CANTSYN_CFG_H */
