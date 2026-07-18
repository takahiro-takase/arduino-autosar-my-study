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
