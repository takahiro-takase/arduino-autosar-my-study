/**
 * \file    Com_Types.h
 * \brief   通信マネージャ型定義 (AUTOSAR SWS_COM 準拠)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef COM_TYPES_H
#define COM_TYPES_H

#include "Platform_Types.h"
#include "Std_Types.h"
#include "ComStack_Types.h"

// -------------------------------------------------------
// 基本ID型
// -------------------------------------------------------
typedef uint8 Com_SignalIdType;
typedef uint8 Com_IPduIdType;

// -------------------------------------------------------
// シグナルのエンディアン
//
//   BIG_ENDIAN (Motorola)
//     BitPosition = シグナルの MSB 位置
//     ビットは BitPosition から BitPosition+BitSize-1 へ連続して格納
//     例: EngineSpeed 16bit, BitPosition=0
//         → bit0(MSB)〜bit15(LSB) が byte[0]上位〜byte[1]下位
//
//   LITTLE_ENDIAN (Intel)
//     BitPosition = シグナルの LSB 位置
//     ビットは BitPosition から BitPosition+BitSize-1 へ連続して格納
//     例: 16bit, BitPosition=0
//         → bit0(LSB)〜bit15(MSB) が byte[0]下位〜byte[1]上位
//
//   ※ ビット番号の定義（この実装全体で統一）
//      bit 0 = byte[0] の MSB、bit 7 = byte[0] の LSB
//      bit 8 = byte[1] の MSB、...（ネットワークビット順）
// -------------------------------------------------------
typedef enum
{
    COM_BIG_ENDIAN    = 0,  // Motorola byte order（CAN標準）
    COM_LITTLE_ENDIAN = 1   // Intel byte order
} Com_SignalEndianType;

// -------------------------------------------------------
// シグナルフィルタアルゴリズム（ComFilterAlgorithm 相当）
//
// TX シグナルについて、Com_SendSignal() が受け取った新しい値を
// 実際に「送信すべき更新」とみなすかどうかを Com 自身が判定する。
// ASW は値をセットするだけで、送信要否の判断は Com の責務とする
// （AUTOSAR の責務分離：ASW は「何を送るか」、Com は「いつ送るか」を決める）。
//
//   COM_FILTER_ALWAYS                     : 常に更新とみなす（フィルタなし、既定）
//   COM_FILTER_MASKED_NEW_DIFFERS_MASKED_OLD
//     : (新値 & Mask) != (前回値 & Mask) のときだけ更新とみなす
// -------------------------------------------------------
typedef enum
{
    COM_FILTER_ALWAYS = 0,
    COM_FILTER_MASKED_NEW_DIFFERS_MASKED_OLD = 1
} Com_FilterAlgorithmType;

// -------------------------------------------------------
// TX 送信モード（ComTxModeMode 相当、簡略版）
//
//   実 AUTOSAR の ComTxModeMode は DIRECT/PERIODIC/MIXED/NONE を持つ。
//   本実装は以下の 2 値のみをサポートする:
//
//   COM_TX_MODE_MIXED    : 送信要否の判断は「呼び出し元が Com_TriggerIPDUSend()
//     を呼んだとき」に限られる（MeterStatus/WarningStatus が使用）。
//     ComFilterAlgorithm を通過した変化があれば送信、無くても
//     COM_TX_PERIODIC_FLOOR_CYCLES 回の呼び出しで周期フロアとして送信する
//     （実 MIXED モードの簡略版。時刻ではなく呼び出し回数基準）。
//
//   COM_TX_MODE_PERIODIC : Com が自分自身の周期タスク（Com_MainFunction）で
//     実時間ベースに送信タイミングを判断する。ASW/CDD は Com_SendSignal() で
//     値を更新するだけでよく、Com_TriggerIPDUSend() を呼ぶ必要が一切ない
//     （実車の Com の PERIODIC 送信モードと同じ「値の生成」と「送信タイミング」
//     の責務分離）。
// -------------------------------------------------------
typedef enum
{
    COM_TX_MODE_MIXED    = 0,
    COM_TX_MODE_PERIODIC = 1
} Com_TxModeModeType;

// -------------------------------------------------------
// I-PDU 設定（1エントリ = 1つの I-PDU）
//
//   IPduId    : COM 内の I-PDU インデックス（0始まり）
//   DLC       : PDU のバイト長 = 内部バッファサイズ
//   PduRId    : PduR 空間での ID
//               RX → Com_RxIndication に渡される DestPduId と一致させる
//               TX → PduR_Transmit に渡す SrcPduId と一致させる
//   TimeoutMs : RX 受信デッドライン [ms]（DaVinci: ComRxDeadlineMonitoringPeriod）
//               0 = 監視無効。TX I-PDU では 0 を設定すること。
//   IsSignalGroup : TX I-PDU のみ使用。1 = Signal Group（Com_SendSignal は
//               シャドウバッファへ書き込むのみとし、Com_SendSignalGroup() で
//               まとめて実バッファへ確定コミットする）。0 = 通常の直接送信
//               （Com_SendSignal がその場で実バッファへ書き込む、既存の挙動）。
//   TxModeMode / TxPeriodMs : TX I-PDU のみ使用（DaVinci: ComTxModeMode /
//               ComTxModeTimePeriodFactor）。COM_TX_MODE_MIXED では未使用。
//               COM_TX_MODE_PERIODIC では TxPeriodMs [ms] 周期で
//               Com_MainFunction() が自動送信する。
//   RxIndicationCbk : RX I-PDU のみ使用。非NULL なら Com_RxIndication() が
//               バッファ更新後に呼ぶ（実 AUTOSAR の ComNotification /
//               RTE 生成コールバックに相当）。E2E Transformer 等、
//               「フレーム受信の都度」処理が必要な上位層向けの汎用フックで
//               あり、Com はここで何が実行されるか一切関知しない
//               （IPduId のハードコード比較を Com.c 本体に埋め込まないため）。
//   TxTransformCbk  : TX I-PDU のみ使用。非NULL なら実際の送信直前（MIXED は
//               Com_TriggerIPDUSend()、PERIODIC は Com_MainFunction() 内部）に、
//               実 TX バッファへのポインタと長さを渡して呼ぶ。E2E Transformer
//               が Counter/CRC をバッファへ書き込む等の「送信直前の最終変換」
//               に使う汎用フック。
// -------------------------------------------------------
typedef struct
{
    Com_IPduIdType     IPduId;
    uint8              DLC;
    PduIdType          PduRId;
    uint16             TimeoutMs;
    uint8              IsSignalGroup;
    Com_TxModeModeType TxModeMode;
    uint16             TxPeriodMs;
    void (*RxIndicationCbk)(void);
    void (*TxTransformCbk)(uint8* Data, uint8 Length);
} Com_IPduConfigType;

// -------------------------------------------------------
// Signal 設定（1エントリ = 1つのシグナル）
//
//   SignalId    : アプリ層が使うシグナル識別子（Com_ReceiveSignal / Com_SendSignal のキー）
//   IPduId      : このシグナルが属する I-PDU の ID
//   BitPosition : I-PDU 内の先頭ビット位置
//                 big-endian  → MSB のビット番号
//                 little-endian → LSB のビット番号
//   BitSize     : シグナルのビット長（1〜32）
//   Endian      : ビットの詰め方向
//   FilterAlgorithm / Mask : TX シグナルの送信要否フィルタ（RX シグナルでは未使用）
// -------------------------------------------------------
typedef struct
{
    Com_SignalIdType        SignalId;
    Com_IPduIdType          IPduId;
    uint8                   BitPosition;
    uint8                   BitSize;
    Com_SignalEndianType    Endian;
    Com_FilterAlgorithmType FilterAlgorithm;
    uint32                  Mask;
} Com_SignalConfigType;

// -------------------------------------------------------
// COM 全体設定（Com_Init に渡す）
//
//   RX / TX それぞれに IPdu テーブルを持つ。
//   Signal テーブルは RX / TX 共通（IPduId で区別する）。
// -------------------------------------------------------
typedef struct
{
    const Com_IPduConfigType*   RxIPdus;       // RX I-PDU テーブル
    uint8                       RxIPduCount;
    const Com_IPduConfigType*   TxIPdus;       // TX I-PDU テーブル
    uint8                       TxIPduCount;
    const Com_SignalConfigType* Signals;        // シグナルテーブル
    uint8                       SignalCount;
} Com_ConfigType;

#endif
