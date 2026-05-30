/**
 * \file    App_WarningIndicator.h
 * \brief   警告灯インジケータ SW-C 公開インタフェース (AUTOSAR ASW)
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef APP_WARNING_INDICATOR_H
#define APP_WARNING_INDICATOR_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   警告灯インジケータ SW-C を初期化する。
 *
 * \details LED チャネルを出力モードに設定し、内部状態を初期化する。
 *          EcuM_Init() から一度だけ呼び出すこと。
 */
void App_WarningIndicator_Init(void);

/**
 * \brief   エンジン状態に応じて LED を制御する Runnable。
 *
 * \details RTE 経由で EngineState を読み取り、
 *          RUNNING=点灯、FAULT=点滅 (500 ms 周期)、その他=消灯 を実現する。
 *          OS の 500 ms タスクから呼び出すこと。
 */
void App_WarningIndicator_Run(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_WARNING_INDICATOR_H */
