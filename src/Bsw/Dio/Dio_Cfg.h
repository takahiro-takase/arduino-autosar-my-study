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

/** ApiId（各関数の Doxygen \ServiceID タグと一致させること。値は SWS 8.x 章の
 *  「Service ID[hex]」記載を実測して確認済み） */
#define DIO_API_ID_GET_VERSION_INFO  0x12U

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

#endif /* DIO_CFG_H */
