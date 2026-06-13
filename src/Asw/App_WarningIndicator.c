/**
 * \file    App_WarningIndicator.c
 * \brief   警告灯インジケータ SW-C 実装 (AUTOSAR ASW)
 * \details RTE 経由で EngineState と AbsActive を受信し、3 本の LED を
 *          それぞれ独立した役割で制御するメータ ECU 用 SW-C。
 *
 *          LED 制御ルール（各 LED が独立して動作）:
 *            D6 RUNNING LED: ENGINE_STATE_RUNNING のとき点灯、それ以外消灯
 *            D7 FAULT LED  : ENGINE_STATE_FAULT のとき点滅 (500 ms 周期)、それ以外消灯
 *            D8 ABS LED    : ABS_ACTIVE = 1 のとき点灯、それ以外消灯
 *
 *          1 本の LED で優先度制御していた旧方式と異なり、各 LED が独立した
 *          表示責務を持つ。FAULT と ABS を同時に表示できる。
 *
 *          本 SW-C は Dio / IoHwAb を直接参照しない。
 *          LED 操作はすべて RTE の Client/Server ポート経由で行い、
 *          ピン番号などのハードウェア詳細から完全に分離されている。
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
    (void)Rte_Call_LedRunning_SetLevel(0U);
    (void)Rte_Call_LedFault_SetLevel(0U);
    (void)Rte_Call_Led_SetLevel(0U);
    s_blinkState = 0U;
    DET_LOGI(TAG, "Init");
}

/**
 * \brief   エンジン状態と ABS 作動状態に応じて 3 本の LED を独立制御する Runnable。
 *
 * \details OS の 500 ms タスクから周期的に呼び出される。
 *          Rte_Read_WarningIndicator_EngineState() と
 *          Rte_Read_AbsSensor_AbsActive() で最新の状態を取得し、
 *          各 LED を独立した役割で制御する。
 *
 *          各 LED の制御ルール:
 *            D6 RUNNING LED: ENGINE_STATE_RUNNING → 点灯、それ以外 → 消灯
 *            D7 FAULT LED  : ENGINE_STATE_FAULT   → 500 ms ごとにトグル（点滅）、それ以外 → 消灯
 *            D8 ABS LED    : ABS_ACTIVE = 1       → 点灯、それ以外 → 消灯
 *
 *          ログ形式 [RUN:<0|1> FAULT:<0|1> ABS:<0|1>] で 3 本の状態を一覧表示する。
 *          FAULT LED が 1→0→1 と交互に変化することで点滅を確認できる。
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

    /* D6: RUNNING LED — ENGINE_STATE_RUNNING のとき点灯 */
    const uint8 runLevel = (state == ENGINE_STATE_RUNNING) ? 1U : 0U;
    (void)Rte_Call_LedRunning_SetLevel(runLevel);

    /* D7: FAULT LED — ENGINE_STATE_FAULT のとき 500 ms ごとにトグル */
    uint8 faultLevel = 0U;
    if (state == ENGINE_STATE_FAULT)
    {
        s_blinkState ^= 1U;
        faultLevel = s_blinkState;
    }
    else
    {
        s_blinkState = 0U;
    }
    (void)Rte_Call_LedFault_SetLevel(faultLevel);

    /* D8: ABS LED — ABS 作動中に点灯 */
    const uint8 absLevel = (absActive != 0U) ? 1U : 0U;
    (void)Rte_Call_Led_SetLevel(absLevel);

    DET_LOGI(TAG, "[RUN:%u FAULT:%u ABS:%u]", (unsigned)runLevel, (unsigned)faultLevel, (unsigned)absLevel);
}
