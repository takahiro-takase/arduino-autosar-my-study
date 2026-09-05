/**
 * \file    CanIf_Types.h
 * \brief   CAN インタフェース型定義 (AUTOSAR SWS_CANInterface 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef CANIF_TYPES_H
#define CANIF_TYPES_H

#include "Platform_Types.h"
#include "Std_Types.h"
#include "ComStack_Types.h"
#include "Can_GeneralTypes.h"

/* SWS_CANIF_00012: <User_RxIndication>, upper-layer RX indication callback. */
typedef void (*CanIf_RxIndicationFctType)(PduIdType RxPduId, const PduInfoType* PduInfoPtr);

/* SWS_CANIF_00011: upper-layer TX confirmation callback.
 * result: E_OK = transmitted successfully, E_NOT_OK = failed. */
typedef void (*CanIf_TxConfirmationFctType)(PduIdType TxPduId, Std_ReturnType result);

// -------------------------------------------------------
// TX PDU 設定エントリ（テーブルの 1 行）
//
// CanIf_Transmit(TxPduId, PduInfo) が呼ばれると、
// TxPduId をインデックスにこのテーブルを引き、
// Can_Write(Hth, Can_PduType) を構築して CanDrv を呼ぶ。
// -------------------------------------------------------
typedef struct
{
    PduIdType                   UpperLayerTxPduId; // TxConfirmation で上位層に返す PDU ID
    Can_IdType                  CanId;             // 送出する CAN フレームの ID
    uint8                       Dlc;               // 最大データ長（0〜8）
    Can_HwHandleType            Hth;               // 使用する TX バッファ（HTH）
    CanIf_TxConfirmationFctType TxConfirmFct;      // 送信完了コールバック（不要なら NULL）
} CanIf_TxPduConfigType;

/* RX PDU configuration entry (one row of the routing table).
 * CanIf_RxIndication(Mailbox, PduInfoPtr) searches this table by HOH and
 * CAN ID, then calls RxIndicationFct(UpperLayerRxPduId, PduInfoPtr).
 *
 * Dlc: 期待するデータ長 (SWS_CANIF_00026 の CanIfRxPduDataLength に相当)。
 *      CanIf_RxIndication() が SduLength < Dlc の L-PDU を上位層へ渡さず
 *      棄却する (SWS_CANIF_00168 の CANIF_E_INVALID_DATA_LENGTH 相当)。
 *
 * ReadRxPduDataEnabled: CanIf_ReadRxPduData()（[SWS_CANIF_00194]、
 *      2026-08 追加）用のオプトインフラグ（実 AUTOSAR の
 *      CANIF_READRXPDU_DATA、ECUC_CanIf_00600 に相当）。1 のときのみ
 *      CanIf_RxIndication() がこの RX PDU の受信データを内部バッファへ
 *      複製し、CanIf_ReadRxPduData() でポーリング取得できるようにする
 *      （既定 0＝バッファリングなし。全 RX PDU を無条件にバッファすると
 *      Arduino の RAM 予算を無駄に消費するため、実 AUTOSAR と同じく
 *      per-PDU の明示的な opt-in とした）。末尾に追加したのは、既存の
 *      位置初期化（designated でない集成体初期化）を使うテストファイルで
 *      この 1 行を書き足す必要が無いようにするため（末尾省略はゼロ初期化
 *      される、Com_SignalConfigType 等と同じ規約）。 */
typedef struct
{
    Can_IdType                CanId;              // マッチさせる CAN ID
    Can_HwHandleType          Hrh;               // 受信元の HRH（RX バッファ識別子）
    PduIdType                 UpperLayerRxPduId; // 上位層に渡す受信 PDU ID
    uint8                     Dlc;               // 期待するデータ長（これ未満は棄却）
    CanIf_RxIndicationFctType RxIndicationFct;   // 受信時に呼ぶ上位層コールバック
    uint8                     ReadRxPduDataEnabled; // CanIf_ReadRxPduData() 用の opt-in
} CanIf_RxPduConfigType;

// -------------------------------------------------------
// CanIf 全体設定（CanIf_Init に渡す）
//
// TX/RX テーブルへのポインタとエントリ数をまとめたもの。
// AUTOSAR では ARXML から自動生成されるが、
// ここでは main.cpp で静的に定義して渡す。
// -------------------------------------------------------
typedef struct
{
    const CanIf_TxPduConfigType* TxPduConfig; // TX PDU テーブルの先頭
    uint8                        TxPduCount;  // TX エントリ数
    const CanIf_RxPduConfigType* RxPduConfig; // RX PDU テーブルの先頭
    uint8                        RxPduCount;  // RX エントリ数
} CanIf_ConfigType;

/* [SWS_CANIF_00137]: PDU チャネル（本プロジェクトは単一コントローラのため
 * 実質コントローラ単位）の送受信有効/無効状態。CanIf_SetPduMode()/
 * CanIf_GetPduMode() で操作する。
 *
 * CANIF_TX_OFFLINE_ACTIVE(0x02、[SWS_CANIF_00072]、7.19.2.3節)は未実装
 * （欠番）。実仕様は「CanIf_Transmit() が Can_Write() まで到達させず、
 * その場で即座に TX 確認コールバックを呼んで送信成功を偽装する」モード
 * （診断パッシブモード等の用途）であり、過去の本コメントは「一定間隔で
 * ウェイクアップ用フレームを送信するモード」と誤って説明していたため訂正
 * した（2026-09-05、シグネチャ準拠サーベイで発覚）。
 *
 * 訂正後、Arduino 上でも実装できないか検討した（CanIf_Transmit() へ
 * Can_Write() を経由しない即時 CanIf_TxConfirmation() 呼び出し分岐を
 * 一度実装した）が、/code-review で重大な設計上の非互換を発見: 本プロジェクトの
 * `Com_DoTransmit()`（Com.c）は「PduR_ComTransmit() が返ってきた後に
 * `Com_TxConfPending[]` を 1 にセットする」という順序を前提にしており、
 * これは `Can.c` が実ハードウェア送信の TX 確認を意図的に別タスク（
 * `Can_MainFunction_Write`）へ遅延させている設計（Can.c 冒頭コメント、
 * 過去の Bus-Off + HW ウォッチドッグリセット不具合の再発防止）と対になる
 * 前提である。CANIF_TX_OFFLINE_ACTIVE の「即座に確認コールバックを呼ぶ」
 * という仕様どおりの同期動作にすると、`PduR_ComTransmit()` が返る前に
 * `Com_TxConfPending[]` が 0 へクリアされてしまい、直後に `Com_DoTransmit()`
 * が無条件で 1 に上書きしてしまう。結果、当該 I-PDU は「送信済み・未確認」の
 * まま二度と確認が届かない状態に固着し、TX 送信デッドライン監視が誤って
 * 発火する（実害のあるリグレッション）。根本修正には `Com_DoTransmit()` の
 * 保留フラグ管理の順序自体を見直す必要があり、実機検証済みの COM 送信経路
 * 中枢に手を入れることになる一方、本モード自体は現状どこからも呼ばれない
 * （動機のない）機能であるため、リスクに見合わないと判断し実装を見送った。 */
typedef enum
{
    CANIF_OFFLINE    = 0x00,  /* TX/RX 双方禁止（初期化直後の既定） */
    CANIF_TX_OFFLINE = 0x01,  /* TX 禁止・RX は継続（SILENT_COMMUNICATION） */
    CANIF_ONLINE     = 0x03   /* 通常動作（TX/RX 双方有効） */
} CanIf_PduModeType;

/* [SWS_CANIF_00201]: CanIf_ReadTxNotifStatus()/CanIf_ReadRxNotifStatus() の
 * 戻り値型。送受信イベントの発生有無のみを表す 2 値。 */
typedef enum
{
    CANIF_NO_NOTIFICATION    = 0x00,  /* 対象 L-PDU について通知イベント無し */
    CANIF_TX_RX_NOTIFICATION = 0x01   /* 対象 L-PDU の送信完了/受信を検出済み */
} CanIf_NotifStatusType;

#endif
