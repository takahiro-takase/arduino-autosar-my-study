/**
 * \file    App_WarningIndicator.c
 * \brief   警告灯インジケータ SW-C 実装 (AUTOSAR ASW)
 * \details RTE 経由で EngineState を受信し、警告灯 LED を
 *          エンジン状態に応じて制御するメータ ECU 用 SW-C。
 *
 *          LED 制御ルール:
 *            - ENGINE_STATE_RUNNING : 点灯 (定常)
 *            - ENGINE_STATE_FAULT   : 点滅 (500 ms 周期 = 本 Runnable の呼出周期)
 *            - それ以外 (OFF / STARTING) : 消灯
 *
 *          本 SW-C は Dio / IoHwAb を直接参照しない。
 *          LED 操作はすべて RTE の Client/Server ポート
 *          (Rte_Call_Led_SetLevel) 経由で行い、ピン番号などの
 *          ハードウェア詳細から完全に分離されている。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "App_WarningIndicator.h"
#include "Rte.h"
#include "Det.h"

#define TAG "WarnInd"

static uint8 s_blinkState = 0U;

/**
 * \brief   警告灯インジケータ SW-C を初期化する。
 *
 * \details LED を消灯状態にし、点滅用内部状態を初期化する。
 *          LED チャネルの方向設定は IoHwAb_Init() が担うため、
 *          本関数では行わない。
 *
 * \pre        IoHwAb_Init() が正常完了していること。
 *
 * \ServiceID      {0xD0}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void App_WarningIndicator_Init(void)
{
    (void)Rte_Call_Led_SetLevel(0U);
    s_blinkState = 0U;
    DET_LOGI(TAG, "Init");
}

/**
 * \brief   エンジン状態に応じて LED を制御する Runnable。
 *
 * \details OS の 500 ms タスクから周期的に呼び出される。
 *          Rte_Read_WarningIndicator_EngineState() で最新の EngineState を取得し、
 *          Rte_Call_Led_SetLevel() で LED レベルを設定する。
 *          FAULT 時は呼出ごとに s_blinkState をトグルし 1 Hz 点滅を実現する。
 *
 * \pre        App_WarningIndicator_Init() が正常完了していること。
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
            (void)Rte_Call_Led_SetLevel(1U);
            break;

        case ENGINE_STATE_FAULT:
            s_blinkState ^= 1U;
            DET_LOGI(TAG, "LED BLINK");
            (void)Rte_Call_Led_SetLevel(s_blinkState);
            break;

        default:
            DET_LOGI(TAG, "LED OFF");
            (void)Rte_Call_Led_SetLevel(0U);
            break;
    }
}
