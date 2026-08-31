/**
 * \file    Port_Cfg.h
 * \brief   Port プリコンパイル設定 (AUTOSAR SWS_Port 準拠)
 * \details プロジェクトで使用するポートピンの方向設定を定義する。
 *          AUTOSAR 環境ではコンフィギュレーションツールが生成するファイルに相当する。
 *
 *          ピン番号は Arduino のデジタルピン番号に対応する。
 *          Dio_Cfg.h の DIO_CHANNEL_* と同じ物理ピンを参照するが、
 *          Port（方向設定）と Dio（値読み書き）は独立したモジュールである。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef PORT_CFG_H
#define PORT_CFG_H

/* -----------------------------------------------------------------------
 * DET（Default Error Tracer）関連定数
 *
 * ModuleId は SWS 本文には明記されないため、AUTOSAR_TR_BSWModuleList
 * （Release 4.3.1、docs/ 配下）の「List of Basic Software Modules」表で
 * Port Driver (Port) に割り当てられた固定値 124 を使う。
 * ----------------------------------------------------------------------- */

/** AUTOSAR Port Driver の ModuleId（AUTOSAR_TR_BSWModuleList 参照、固定値 124） */
#define PORT_MODULE_ID  124U

/** 開発エラーコード（SWS_Port 7.x Development Error 表より、GetVersionInfo の
 *  NULL チェックと Port_SetPinMode() のみ使用） */
#define PORT_E_PARAM_POINTER      0x10U
#define PORT_E_MODE_UNCHANGEABLE  0x0EU

/** ApiId（各関数の Doxygen \ServiceID タグと一致させること。値は SWS 8.x 章の
 *  「Service ID[hex]」記載を実測して確認済み） */
#define PORT_API_ID_GET_VERSION_INFO  0x03U
#define PORT_API_ID_SET_PIN_MODE      0x04U

/** バージョン情報（SWS_Port_GetVersionInfo、Com/E2EXf/PduR 等の既存モジュールと同じ命名規則） */
#define PORT_VENDOR_ID          0U
#define PORT_SW_MAJOR_VERSION   1U
#define PORT_SW_MINOR_VERSION   0U
#define PORT_SW_PATCH_VERSION   0U

/** 管理ピン総数 */
#define PORT_PIN_COUNT          4U

/** RUNNING LED (D6) — 出力ピン（エンジン正常稼働中に点灯） */
#define PORT_PIN_LED_RUNNING    6U

/** FAULT LED (D7) — 出力ピン（異常状態に点滅） */
#define PORT_PIN_LED_FAULT      7U

/** ABS / 警告灯 LED (D8) — 出力ピン（ABS 作動中に点灯） */
#define PORT_PIN_LED_WARNING    8U

/** 警告確認ボタン (D9) — 入力ピン (INPUT_PULLUP)
 *  FAULT 状態でボタン押下 → FAULT→OFF 遷移。
 *  ボタン押下時は GND と接続され DIO_LOW が読まれる。
 *  IoHwAb_Button_GetLevel() で論理反転して 押下=1 に変換する。 */
#define PORT_PIN_BUTTON         9U

#endif /* PORT_CFG_H */
