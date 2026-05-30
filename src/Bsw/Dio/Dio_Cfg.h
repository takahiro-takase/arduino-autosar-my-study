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
 * Arduino UNO LED (D8)。
 * 警告灯インジケータとして使用する。
 */
#define DIO_CHANNEL_LED_WARNING  8U

#endif /* DIO_CFG_H */
