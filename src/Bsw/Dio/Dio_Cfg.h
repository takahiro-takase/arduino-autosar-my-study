/**
 * \file    Dio_Cfg.h
 * \brief   DIO プリコンパイル設定 (AUTOSAR SWS_Dio 準拠)
 * \details プロジェクトで使用するデジタル I/O チャネル ID を定義する。
 *          チャネル ID は Arduino のピン番号に対応する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef DIO_CFG_H
#define DIO_CFG_H

/* -----------------------------------------------------------------------
 * DET（Default Error Tracer）関連定数
 *
 * ModuleId は SWS 本文には明記されないため、AUTOSAR_TR_BSWModuleList
 * （Release 4.3.1、docs/ 配下）の「List of Basic Software Modules」表で
 * DIO Driver (Dio) に割り当てられた固定値 120 を使う。
 * ----------------------------------------------------------------------- */

/** AUTOSAR DIO Driver の ModuleId（AUTOSAR_TR_BSWModuleList 参照、固定値 120） */
#define DIO_MODULE_ID  120U

/** 開発エラーコード（[SWS_Dio_00188]/[SWS_Dio_00189] より、GetVersionInfo の
 *  NULL チェックのみ使用。0x14 は本来 DIO_E_PARAM_INVALID_PORT_ID の値であり、
 *  DIO_E_PARAM_POINTER の値は 0x20 が正しい） */
#define DIO_E_PARAM_POINTER  0x20U

/** 開発エラーコード追加分（[SWS_Dio_00177]/[SWS_Dio_00178]、SWS_Dio 7.6.1
 *  Development Errors 表より実測確認。DIO_E_PARAM_INVALID_CHANNEL_ID(0x0A)は
 *  本プロジェクトの Dio がチャネルID照合テーブルを持たない設計のため対象外
 *  （docs/modules/Det_Notes.md「Dio / Port」参照） */
#define DIO_E_PARAM_INVALID_PORT_ID     0x14U
#define DIO_E_PARAM_INVALID_GROUP       0x1FU

/** ApiId（各関数の Doxygen \ServiceID タグと一致させること。値は SWS 8.x 章の
 *  「Service ID[hex]」記載を実測して確認済み） */
#define DIO_API_ID_GET_VERSION_INFO      0x12U
#define DIO_API_ID_READ_PORT             0x02U
#define DIO_API_ID_WRITE_PORT            0x03U
#define DIO_API_ID_READ_CHANNEL_GROUP    0x04U
#define DIO_API_ID_WRITE_CHANNEL_GROUP   0x05U

/** バージョン情報（SWS_Dio_GetVersionInfo、Com/E2EXf/PduR 等の既存モジュールと同じ命名規則） */
#define DIO_VENDOR_ID          0U
#define DIO_SW_MAJOR_VERSION   1U
#define DIO_SW_MINOR_VERSION   0U
#define DIO_SW_PATCH_VERSION   0U

/**
 * Arduino UNO RUNNING LED (D6)。
 * エンジン正常稼働中（ENGINE_STATE_RUNNING）に点灯する。
 */
#define DIO_CHANNEL_LED_RUNNING  6U

/**
 * Arduino UNO FAULT LED (D7)。
 * 異常状態（ENGINE_STATE_FAULT）に点滅する。
 */
#define DIO_CHANNEL_LED_FAULT    7U

/**
 * Arduino UNO LED (D8)。
 * 警告灯インジケータとして使用する。
 */
#define DIO_CHANNEL_LED_WARNING  8U

/**
 * Arduino UNO プッシュボタン (D9)。
 * 警告確認ボタン。FAULT 状態でボタン押下 → FAULT→OFF 遷移。
 * Port は INPUT_PULLUP で設定するため、押下時は DIO_LOW となる。
 * IoHwAb_Button_GetLevel() で論理反転し、押下=1 に変換する。
 */
#define DIO_CHANNEL_BUTTON       9U

/* -----------------------------------------------------------------------
 * Dio_PortType / Dio_ChannelGroupType 設定
 *
 * 実 MCU のポートレジスタ（8/16/32bit 一括読み書き）は Arduino API では
 * 直接扱えないため、本プロジェクトの「ポート」は個々のチャネル（Arduino
 * ピン）を複数まとめた論理グループとして Dio.c 内で定義し、
 * Dio_ReadPort/WritePort/ReadChannelGroup/WriteChannelGroup は
 * そのグループ内を1chずつ digitalRead/digitalWrite でループ処理する
 * （非アトミック。Dio_FlipChannel と同じ制約）。
 * 本プロジェクトの Dio はチャネルの入出力方向を追跡していないため、
 * ポート/チャネルグループの構成には出力チャネルのみを含めること
 * （入力チャネルを含めると WritePort 等が INPUT_PULLUP 設定を書き換えて
 * しまう）。
 * ----------------------------------------------------------------------- */

/** LED出力3ch(D6=RUNNING/D7=FAULT/D8=WARNING)をまとめた仮想ポート。
 *  bit0=D6, bit1=D7, bit2=D8（実体は Dio.c の Dio_PortLedGroupChannels[]）。 */
#define DIO_PORT_LED_GROUP  0U

/** Dio_PortType の総数。Dio.c の Dio_PortConfig[] の要素数と一致させること
 *  （ポートを追加する際は両方を同時に更新する。Port.c の PORT_PIN_COUNT と
 *  同じ安全策）。 */
#define DIO_PORT_COUNT  1U

/** ChannelGroup 例: DIO_PORT_LED_GROUP の下位2bit(D6+D7)をまとめたグループ。
 *  Dio.c が事前定義済みインスタンス `Dio_ChannelGroupRunFault`（Dio.h で
 *  extern 宣言）として公開しているので、通常はそちらを使えばよい。
 *  自前で組み立てる場合は `Dio_ChannelGroupType g = {
 *  DIO_CHANNELGROUP_RUN_FAULT_PORT, DIO_CHANNELGROUP_RUN_FAULT_MASK,
 *  DIO_CHANNELGROUP_RUN_FAULT_OFFSET };` のようにする（Dio_ChannelGroupType
 *  は Dio.h で定義されており、本ファイルより後にインクルードされるため
 *  実体を本ファイルには置けない）。 */
#define DIO_CHANNELGROUP_RUN_FAULT_PORT    DIO_PORT_LED_GROUP
#define DIO_CHANNELGROUP_RUN_FAULT_MASK    0x03U
#define DIO_CHANNELGROUP_RUN_FAULT_OFFSET  0U

#endif /* DIO_CFG_H */
