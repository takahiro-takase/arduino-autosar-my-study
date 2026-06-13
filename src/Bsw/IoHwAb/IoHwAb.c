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

#define TAG "IoHwAb"

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
    Dio_WriteChannel(DIO_CHANNEL_LED_WARNING, DIO_LOW);  /* 消灯状態で起動 */
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
 * \brief   エンジン起動ボタンの押下状態を取得する。
 *
 * \details DIO_CHANNEL_BUTTON は PORT_PIN_IN_PULLUP で設定されているため、
 *          ボタン未押下時は VCC に引き上げられ DIO_HIGH、
 *          押下時は GND に接続され DIO_LOW となる。
 *          本関数でその論理を反転して呼び出し元に渡す（押下=1）。
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
    /* INPUT_PULLUP: LOW = 押下（GND接続）、HIGH = 解放（プルアップ電位）*/
    *level = (Dio_ReadChannel(DIO_CHANNEL_BUTTON) == DIO_LOW) ? 1U : 0U;
    return E_OK;
}
