/**
 * \file    App_WarningIndicator.c
 * \brief   警告灯インジケータ SW-C 実装 (AUTOSAR ASW)
 * \details RTE 経由で EngineState と AbsActive を受信し、警告灯 LED を
 *          優先度順に制御するメータ ECU 用 SW-C。
 *
 *          LED 制御ルール（優先度順）:
 *            優先度 1: ENGINE_STATE_FAULT  → 点滅 (500 ms 周期 = 本 Runnable の呼出周期)
 *            優先度 2: ABS_ACTIVE = 1      → 点灯 (ABS 作動中警告)
 *            優先度 3: ENGINE_STATE_RUNNING → 点灯 (正常稼働)
 *            それ以外 (OFF / STARTING)     → 消灯
 *
 *          エンジン FAULT 中に ABS が同時作動しても点滅を維持する
 *          (エンジン異常を最優先で通知するため)。
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
 * \brief   エンジン状態と ABS 作動状態に応じて LED を優先度制御する Runnable。
 *
 * \details OS の 500 ms タスクから周期的に呼び出される。
 *          Rte_Read_WarningIndicator_EngineState() と
 *          Rte_Read_AbsSensor_AbsActive() で最新の状態を取得し、
 *          優先度順に Rte_Call_Led_SetLevel() で LED レベルを決定する。
 *
 *          優先度ルール:
 *            1. ENGINE_STATE_FAULT  → 呼出ごとに s_blinkState をトグルして 1 Hz 点滅
 *            2. ABS_ACTIVE = 1      → 点灯 (エンジン FAULT でなければ ABS 警告を表示)
 *            3. ENGINE_STATE_RUNNING → 点灯
 *            4. その他              → 消灯
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
    AbsActive_t   absActive = 0U;

    (void)Rte_Read_WarningIndicator_EngineState(&state);
    (void)Rte_Read_AbsSensor_AbsActive(&absActive);

    if (state == ENGINE_STATE_FAULT)
    {
        s_blinkState ^= 1U;
        DET_LOGI(TAG, "LED BLINK(Fault)");
        (void)Rte_Call_Led_SetLevel(s_blinkState);
    }
    else if (absActive != 0U)
    {
        DET_LOGI(TAG, "LED ON(ABS)");
        (void)Rte_Call_Led_SetLevel(1U);
    }
    else if (state == ENGINE_STATE_RUNNING)
    {
        DET_LOGI(TAG, "LED ON");
        (void)Rte_Call_Led_SetLevel(1U);
    }
    else
    {
        DET_LOGI(TAG, "LED OFF");
        (void)Rte_Call_Led_SetLevel(0U);
    }
}
