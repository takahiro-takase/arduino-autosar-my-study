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
// 実 AUTOSAR では ComFilterAlgorithm は 2 つの異なる目的で使われる:
//   (1) ComTransferProperty=TRIGGERED_ON_CHANGE の判定材料
//       （signal(group) の送信要求時、値が変化していれば送信を開始する。
//       本実装では COM_FILTER_MASKED_NEW_DIFFERS_MASKED_OLD が Com_SendSignal()
//       内でこの役割を担い、Com_RequestTxOnChange() を呼ぶかどうかを決める）
//   (2) TMS（Transmission Mode Selector）の評価材料
//       （SWS_Com_00676-00679: この I-PDU が今 ComTxModeTrue/False の
//       どちらを使うべきかを、TmsContributor=1 のシグナルの条件評価
//       （OR 合成）で決める。本実装では COM_FILTER_MASKED_NEW_DIFFERS_X が
//       この役割を担う。詳細は Com_IPduConfigType の TxModeModeTrue 参照）
// この 2 つは実 AUTOSAR でも独立した仕組みであり、同じシグナルが両方に
// 使われるとは限らない（本実装でも、変化時送信の判定用シグナルと TMS 用
// シグナルは別々に設定できる）。
//
//   COM_FILTER_ALWAYS                     : 常に更新とみなす（フィルタなし、既定）
//   COM_FILTER_MASKED_NEW_DIFFERS_MASKED_OLD
//     : (新値 & Mask) != (前回値 & Mask) のときだけ更新とみなす（用途 (1)）
//   COM_FILTER_MASKED_NEW_DIFFERS_X
//     : (新値 & Mask) != FilterX のときだけ TMC（Transmission Mode Condition）
//       を true とみなす（用途 (2)。SWS_COM_00813 の MASKED_NEW_DIFFERS_X 相当。
//       実 AUTOSAR は他に MASKED_NEW_EQUALS_X/NEW_IS_WITHIN/NEW_IS_OUTSIDE も
//       持つが、本実装では TMS 判定に最小限必要なこの 1 種類のみ実装する）
// -------------------------------------------------------
typedef enum
{
    COM_FILTER_ALWAYS = 0,
    COM_FILTER_MASKED_NEW_DIFFERS_MASKED_OLD = 1,
    COM_FILTER_MASKED_NEW_DIFFERS_X = 2
} Com_FilterAlgorithmType;

// -------------------------------------------------------
// TX 送信モード（ComTxModeMode 相当、簡略版）
//
//   実 AUTOSAR の ComTxModeMode は DIRECT/PERIODIC/MIXED/NONE を持つ。
//   本実装は以下の 3 値をサポートする。いずれも ASW/CDD は
//   Com_SendSignal() / Com_SendSignalGroup() で値を更新するだけでよく、
//   送信要否・タイミングの判断は一切 Com 自身が行う
//   （実車の Com と同じ「値の生成」と「送信タイミング」の責務分離。
//   SWS_Com_00734/00742/00743: DIRECT/MIXED は変化検知した signal(group)の
//   send request が「次回メイン関数までに」送信を開始させる。本実装では
//   実際の PduR_Transmit() 呼び出しを必ず Com_MainFunction() 側で行う
//   ことで、この「次回メイン関数まで」の猶予をそのまま「ASW Runnable の
//   スタックフレームで SPI 送信までブロッキングさせない」設計に利用している）。
//
//   COM_TX_MODE_DIRECT   : ComFilterAlgorithm を通過した変化があると
//     次回 Com_MainFunction() で送信する。周期フロアは持たない
//     （WarningStatus が使用。他 ECU の制御判断に使われないダッシュボード
//     表示用ミラー情報のため、取りこぼしても実害が小さいと判断した）。
//
//   COM_TX_MODE_MIXED    : DIRECT と同じ変化時送信に加えて、
//     一定時間（TxPeriodMs）変化がなくても周期フロアとして再送する
//     （MeterStatus が使用。他 ECU がエンジン状態を判断材料にするため、
//     1 回きりのイベント送信だけでは起動直後の受信側や瞬断からの復帰後に
//     いつまでも古い値のままになりうる。実 MIXED モードの簡略版で、
//     フロアタイマーは直近の送信［変化時送信/フロアいずれも］でリセットする
//     簡略化のため、実車の ComMinimumDelayTime による厳密な位相制御は行わない）。
//
//   COM_TX_MODE_PERIODIC : 値の変化には反応せず、Com_MainFunction()
//     が TxPeriodMs 周期で常に送信する（E2EHealthStatus が使用）。
//
//   1 つの I-PDU に上記モードを 1 つだけ固定で持たせるのではなく、TMS
//   （Transmission Mode Selector）により状況に応じて 2 つのモードを
//   自動切り替えすることもできる（WarningStatus が使用。通常は DIRECT、
//   FAULT/ABS 警告灯が点灯中は MIXED へ切り替わる。詳細は
//   Com_IPduConfigType の TxModeModeTrue の説明を参照）。
// -------------------------------------------------------
typedef enum
{
    COM_TX_MODE_MIXED    = 0,
    COM_TX_MODE_PERIODIC = 1,
    COM_TX_MODE_DIRECT   = 2
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
//   TxModeMode / TxPeriodMs : TX I-PDU のみ使用（DaVinci: ComTxModeFalse /
//               ComTxModeTimePeriodFactor）。TMS（下記）が false と評価された
//               ときに使うモード。TMS を持たない（TmsContributor なシグナルが
//               存在しない）I-PDU では常にこちらだけを使う。COM_TX_MODE_DIRECT
//               では TxPeriodMs は未使用。MIXED では周期フロア間隔として、
//               PERIODIC では厳密な送信周期として TxPeriodMs [ms] を使う。
//   TxModeModeTrue / TxPeriodMsTrue : TX I-PDU のみ使用（DaVinci:
//               ComTxModeTrue / ComTxModeTimePeriodFactor）。TMS（Transmission
//               Mode Selector、SWS_Com_00032/00799/00676-00679 相当）が true と
//               評価されたときに使うモード。TMS は、この I-PDU に属する
//               シグナルのうち TmsContributor=1 のものを対象に
//               ComFilterAlgorithm=COM_FILTER_MASKED_NEW_DIFFERS_X を評価し、
//               いずれか 1 つでも真なら true（SWS_Com_00678、OR 合成）、
//               1 つも無ければ false（SWS_Com_00679）とする。Com_SendSignal()/
//               Com_SendSignalGroup() のたびに再評価する（SWS_Com_00245）。
//               ASW/CDD は TMS の存在を一切意識せず、値をセットするだけでよい。
//   RxIndicationCbk : RX I-PDU のみ使用。非NULL なら Com_RxIndication() が
//               バッファ更新後に呼ぶ（実 AUTOSAR の ComNotification /
//               RTE 生成コールバックに相当）。E2E Transformer 等、
//               「フレーム受信の都度」処理が必要な上位層向けの汎用フックで
//               あり、Com はここで何が実行されるか一切関知しない
//               （IPduId のハードコード比較を Com.c 本体に埋め込まないため）。
//   TxTransformCbk  : TX I-PDU のみ使用。非NULL なら実際の送信直前（DIRECT/
//               MIXED はイベント駆動の送信直前、PERIODIC は Com_MainFunction()
//               内部の周期送信直前）に、実 TX バッファへのポインタと長さを
//               渡して呼ぶ。E2E Transformer が Counter/CRC をバッファへ
//               書き込む等の「送信直前の最終変換」に使う汎用フック。
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
    Com_TxModeModeType TxModeModeTrue;
    uint16             TxPeriodMsTrue;
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
//   FilterAlgorithm / Mask / FilterX : TX シグナルの送信要否フィルタ・TMS 評価
//               フィルタ（RX シグナルでは未使用）。FilterX は
//               COM_FILTER_MASKED_NEW_DIFFERS_X 専用の比較値
//               （MASKED_NEW_DIFFERS_MASKED_OLD では未使用）。
//   TmsContributor : TX シグナルのみ使用。1 = このシグナルの
//               ComFilterAlgorithm 評価結果が、所属 I-PDU の TMS
//               （Com_IPduConfigType.TxModeModeTrue 参照）へ寄与する
//               （SWS_Com_00676）。0 = TMS 計算に一切関与しない（既定）。
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
    uint32                  FilterX;
    uint8                   TmsContributor;
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
