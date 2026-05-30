/**
 * \file    App_WarningIndicator.c
 * \brief   警告灯インジケータ SW-C 実装 (AUTOSAR ASW)
 * \details RTE 経由で EngineState を受信し、Arduino 内蔵 LED (D13) を
 *          エンジン状態に応じて制御するメータ ECU 用 SW-C。
 *
 *          LED 制御ルール:
 *            - ENGINE_STATE_RUNNING : 点灯 (定常)
 *            - ENGINE_STATE_FAULT   : 点滅 (500 ms 周期 = 本 Runnable の呼出周期)
 *            - それ以外 (OFF / STARTING) : 消灯
 *
 *          点滅は s_blinkState を毎回トグルすることで実現する。
 *          Runnable 周期 (500 ms) が点滅半周期に相当するため、
 *          500 ms ON → 500 ms OFF → ... の 1 Hz 点滅となる。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "App_WarningIndicator.h"
#include "Rte.h"
#include "Dio.h"
#include "Dio_Cfg.h"
#include "Det.h"

#define TAG "WarnInd"

static Dio_LevelType s_blinkState = DIO_LOW;

/**
 * \brief   警告灯インジケータ SW-C を初期化する。
 *
 * \details LED チャネルを OUTPUT に設定し、消灯状態で起動する。
 *
 * \pre        Arduino ランタイムが初期化済みであること。
 *
 * \ServiceID      {0xD0}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void App_WarningIndicator_Init(void)
{
    Dio_InitChannel(DIO_CHANNEL_LED_WARNING);
    Dio_WriteChannel(DIO_CHANNEL_LED_WARNING, DIO_LOW);
    s_blinkState = DIO_LOW;
    DET_LOGI(TAG, "Init");
}

/**
 * \brief   エンジン状態に応じて LED を制御する Runnable。
 *
 * \details OS の 500 ms タスクから周期的に呼び出される。
 *          Rte_Read_WarningIndicator_EngineState() で最新の EngineState を取得し、
 *          状態に応じた LED 制御を行う。
 *          FAULT 時は呼出ごとに s_blinkState をトグルし 1 Hz 点滅を実現する。
 *
 * \pre        App_WarningIndicator_Init() が正常完了していること。
 * \pre        Rte_Write_EngineStatus_EngineState() で EngineState が更新されていること。
 *
 * \ServiceID      {0xD1}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void App_WarningIndicator_Run(void)
{
    EngineState_t state;
    (void)Rte_Read_WarningIndicator_EngineState(&state);

    switch (state)
    {
        case ENGINE_STATE_RUNNING:
            DET_LOGI(TAG, "LED ON");
            Dio_WriteChannel(DIO_CHANNEL_LED_WARNING, DIO_HIGH);
            break;

        case ENGINE_STATE_FAULT:
            s_blinkState = (s_blinkState == DIO_HIGH) ? DIO_LOW : DIO_HIGH;
            DET_LOGI(TAG, "LED BLINK");
            Dio_WriteChannel(DIO_CHANNEL_LED_WARNING, s_blinkState);
            break;

        default:
            DET_LOGI(TAG, "LED OFF");
            Dio_WriteChannel(DIO_CHANNEL_LED_WARNING, DIO_LOW);
            break;
    }
}
