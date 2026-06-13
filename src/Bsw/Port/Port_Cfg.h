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

/** 管理ピン総数 */
#define PORT_PIN_COUNT          2U

/** 警告灯 LED (D8) — 出力ピン */
#define PORT_PIN_LED_WARNING    8U

/** エンジン起動ボタン (D9) — 入力ピン (INPUT_PULLUP)
 *  ボタン押下時は GND と接続され DIO_LOW が読まれる。
 *  IoHwAb_Button_GetLevel() で論理反転して 押下=1 に変換する。 */
#define PORT_PIN_BUTTON         9U

#endif /* PORT_CFG_H */
