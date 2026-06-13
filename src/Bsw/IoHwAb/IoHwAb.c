/**
 * \file    IoHwAb.c
 * \brief   I/O ハードウェア抽象化層 実装 (AUTOSAR IoHwAb 準拠)
 * \details MCAL Dio モジュールをラップし、上位層（RTE 経由 SW-C）に
 *          ハードウェア非依存の I/O インタフェースを提供する。
 *          本プロジェクトでは警告灯 LED (DIO_CHANNEL_LED_WARNING) のみを管理する。
 *
 *          本ファイルが Dio.h / Dio_Cfg.h を参照する唯一の非 MCAL ファイルである。
 *          SW-C は本層を直接インクルードせず、RTE C/S ポートを通じてのみアクセスする。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "IoHwAb.h"
#include "Dio.h"
#include "Dio_Cfg.h"
#include "Det.h"
#include "Dem.h"

#define TAG "IoHwAb"

/** ボタン確定に必要な連続一致サンプル数 (4 × 10ms = 40ms) */
#define IOHWAB_BUTTON_DEBOUNCE_COUNT  4U

/** ボタン固着と判定するまでの確定押下継続サンプル数 (500 × 10ms = 5000ms) */
#define IOHWAB_BUTTON_STUCK_COUNT     500U

static uint8  s_confirmedLevel  = 0U;  /* デバウンス確定値 (0=解放, 1=押下) */
static uint8  s_debounceCounter = 0U;  /* 未確定サンプル積算カウンタ */
static uint16 s_stuckCounter    = 0U;  /* 確定押下継続サンプル数 */

/**
 * \brief   IoHwAb モジュールを初期化する。
 *
 * \details 警告灯 LED を消灯状態で起動する。
 *          ピン方向設定は Port_Init() が担うため、本関数では行わない。
 *          チャネル番号 (DIO_CHANNEL_LED_WARNING) は Dio_Cfg.h で一元管理し、
 *          上位層には公開しない。
 *
 * \pre        Port_Init() が正常完了していること。
 *
 * \ServiceID      {0xC0}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void IoHwAb_Init(void)
{
    Dio_WriteChannel(DIO_CHANNEL_LED_RUNNING, DIO_LOW);  /* 消灯状態で起動 */
    Dio_WriteChannel(DIO_CHANNEL_LED_FAULT,   DIO_LOW);  /* 消灯状態で起動 */
    Dio_WriteChannel(DIO_CHANNEL_LED_WARNING,  DIO_LOW);  /* 消灯状態で起動 */
    s_confirmedLevel  = 0U;
    s_debounceCounter = 0U;
    s_stuckCounter    = 0U;
    DET_LOGI(TAG, "Init");
}

/**
 * \brief   警告灯 LED の出力レベルを設定する。
 *
 * \details Dio_WriteChannel() へ委譲する。チャネル ID は本関数内で解決し、
 *          呼び出し元（RTE / SW-C）にピン番号を公開しない。
 *
 * \param[in]  level  出力レベル。0 = 消灯 (LOW)、1 = 点灯 (HIGH)。
 *
 * \retval  E_OK  常に成功。
 *
 * \ServiceID      {0xC1}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType IoHwAb_Led_SetLevel(uint8 level)
{
    Dio_WriteChannel(DIO_CHANNEL_LED_WARNING, (Dio_LevelType)level);
    return E_OK;
}

/**
 * \brief   RUNNING LED (D6) の出力レベルを設定する。
 *
 * \param[in]  level  出力レベル。0 = 消灯、1 = 点灯。
 *
 * \retval  E_OK  常に成功。
 *
 * \ServiceID      {0xC3}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType IoHwAb_LedRunning_SetLevel(uint8 level)
{
    Dio_WriteChannel(DIO_CHANNEL_LED_RUNNING, (Dio_LevelType)level);
    return E_OK;
}

/**
 * \brief   FAULT LED (D7) の出力レベルを設定する。
 *
 * \param[in]  level  出力レベル。0 = 消灯、1 = 点灯。
 *
 * \retval  E_OK  常に成功。
 *
 * \ServiceID      {0xC4}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType IoHwAb_LedFault_SetLevel(uint8 level)
{
    Dio_WriteChannel(DIO_CHANNEL_LED_FAULT, (Dio_LevelType)level);
    return E_OK;
}

/**
 * \brief   ボタンのデバウンス処理を実行する周期関数。
 *
 * \details 積分カウンタ方式:
 *          生レベルが確定値と異なれば s_debounceCounter をインクリメントし、
 *          IOHWAB_BUTTON_DEBOUNCE_COUNT に達した時点で確定値を更新する。
 *          生レベルが確定値と一致すればカウンタをリセットする。
 *          Dio_ReadChannel の呼び出しは本関数に集約し、
 *          IoHwAb_Button_GetLevel は確定値を返すだけにする。
 *
 * \ServiceID      {0xC5}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void IoHwAb_MainFunction(void)
{
    /* INPUT_PULLUP: LOW = 押下（GND接続）、HIGH = 解放（プルアップ電位）*/
    const uint8 rawLevel = (Dio_ReadChannel(DIO_CHANNEL_BUTTON) == DIO_LOW) ? 1U : 0U;

    if (rawLevel == s_confirmedLevel)
    {
        s_debounceCounter = 0U;  /* 安定、カウンタリセット */
    }
    else
    {
        s_debounceCounter++;
        if (s_debounceCounter >= IOHWAB_BUTTON_DEBOUNCE_COUNT)
        {
            s_confirmedLevel  = rawLevel;
            s_debounceCounter = 0U;
            DET_LOGI(TAG, "Button confirmed level=%u", (unsigned)s_confirmedLevel);
        }
    }

    /* ボタン固着検出 (デバウンス確定値を使用) */
    if (s_confirmedLevel == 1U)
    {
        if (s_stuckCounter < IOHWAB_BUTTON_STUCK_COUNT)
        {
            s_stuckCounter++;
            if (s_stuckCounter == IOHWAB_BUTTON_STUCK_COUNT)
            {
                Dem_ReportErrorStatus(DEM_EVENT_BUTTON_STUCK, DEM_EVENT_STATUS_FAILED);
                DET_LOGW(TAG, "Button stuck dtc=0x%06lX", (unsigned long)DEM_DTC_BUTTON_STUCK);
            }
        }
    }
    else
    {
        if (s_stuckCounter >= IOHWAB_BUTTON_STUCK_COUNT)
        {
            /* 固着から解放 → PASSED 報告 */
            Dem_ReportErrorStatus(DEM_EVENT_BUTTON_STUCK, DEM_EVENT_STATUS_PASSED);
            DET_LOGI(TAG, "Button stuck cleared");
        }
        s_stuckCounter = 0U;
    }
}

/**
 * \brief   警告確認ボタンの押下状態を取得する。
 *
 * \details IoHwAb_MainFunction が確定したデバウンス済み状態を返す。
 *          Dio_ReadChannel は呼び出さず、静的変数 s_confirmedLevel を参照するだけ。
 *
 * \param[out] level  押下状態 (0=解放, 1=押下)。
 *
 * \retval  E_OK  常に成功。
 *
 * \ServiceID      {0xC2}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType IoHwAb_Button_GetLevel(uint8* level)
{
    *level = s_confirmedLevel;
    return E_OK;
}
