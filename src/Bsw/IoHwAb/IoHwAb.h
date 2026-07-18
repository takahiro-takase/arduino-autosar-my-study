/**
 * \file    IoHwAb.h
 * \brief   I/O ハードウェア抽象化層 公開インタフェース (AUTOSAR IoHwAb 準拠)
 * \details MCAL Dio モジュールと ASW SW-C の間に位置し、
 *          アプリケーションをピン番号などのハードウェア詳細から分離する。
 *          SW-C は RTE の Client/Server ポート経由でのみ本層を呼び出す。
 *          Dio.h / Dio_Cfg.h を直接参照するのは本ファイルの実装（IoHwAb.c）のみ。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef IOHWAB_H
#define IOHWAB_H

#include "Std_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   IoHwAb モジュールを初期化する。
 *
 * \details 管理するすべての I/O チャネルを出力モードに設定し、
 *          初期レベルを LOW（消灯）にする。
 *          EcuM_Init() から ASW 初期化より前に呼び出すこと。
 *
 * \ServiceID      {0xC0}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void IoHwAb_Init(void);

/**
 * \brief   警告灯 LED の出力レベルを設定する。
 *
 * \details MCAL Dio_WriteChannel() へ委譲し、LED を点灯または消灯する。
 *          RTE の Client/Server ポート (Rte_Call_Led_SetLevel) から呼び出される。
 *
 * \param[in]  level  出力レベル。0 = 消灯、1 = 点灯。
 *
 * \retval  E_OK  常に成功。
 *
 * \ServiceID      {0xC1}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType IoHwAb_Led_SetLevel(uint8 level);

/**
 * \brief   RUNNING LED (D6) の出力レベルを設定する。
 *
 * \details ENGINE_STATE_RUNNING 中に点灯する専用 LED を制御する。
 *          RTE の Client/Server ポート (Rte_Call_LedRunning_SetLevel) から呼び出される。
 *
 * \param[in]  level  出力レベル。0 = 消灯、1 = 点灯。
 *
 * \retval  E_OK  常に成功。
 *
 * \ServiceID      {0xC3}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType IoHwAb_LedRunning_SetLevel(uint8 level);

/**
 * \brief   FAULT LED (D7) の出力レベルを設定する。
 *
 * \details ENGINE_STATE_FAULT 中に点滅する専用 LED を制御する。
 *          RTE の Client/Server ポート (Rte_Call_LedFault_SetLevel) から呼び出される。
 *
 * \param[in]  level  出力レベル。0 = 消灯、1 = 点灯。
 *
 * \retval  E_OK  常に成功。
 *
 * \ServiceID      {0xC4}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType IoHwAb_LedFault_SetLevel(uint8 level);

/**
 * \brief   ボタンのデバウンス処理を実行する周期関数。
 *
 * \details 10 ms 周期で呼び出し、生レベルが IOHWAB_BUTTON_DEBOUNCE_COUNT 回
 *          連続して確定値と異なった場合に確定値を更新する（積分カウンタ方式）。
 *          本関数が唯一 Dio_ReadChannel を呼び出す。
 *          IoHwAb_Button_GetLevel は本関数が確定した値を返すだけになる。
 *          Os_PBCfg.c のタスクテーブルに 10 ms 周期で登録すること。
 *
 * \ServiceID      {0xC5}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void IoHwAb_MainFunction(void);

/**
 * \brief   警告確認ボタンの押下状態を取得する。
 *
 * \details IoHwAb_MainFunction が確定したデバウンス済み状態を返す。
 *          INPUT_PULLUP による論理反転（LOW=押下）は IoHwAb_MainFunction 内で吸収済み。
 *          RTE の Client/Server ポート (Rte_Call_Button_GetLevel) から呼び出される。
 *
 * \param[out] level  押下状態を受け取る変数へのポインタ。
 *                    0 = ボタン解放、1 = ボタン押下。NULL 禁止。
 *
 * \retval  E_OK  常に成功。
 *
 * \ServiceID      {0xC2}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType IoHwAb_Button_GetLevel(uint8* level);

/**
 * \brief   ADC センサ電圧値を取得する。
 *
 * \details IoHwAb_MainFunction が計算した最新の mV 値を返す。
 *          Adc_ReadChannel は呼び出さず、静的変数 s_adcMv を参照するだけ。
 *          RTE の Client/Server ポート (Rte_Call_Adc_GetValue_mV) から呼び出される。
 *
 * \param[out] mv  変換済み電圧値 [mV] の格納先。NULL 禁止。
 *
 * \retval  E_OK  常に成功。
 *
 * \ServiceID      {0xC6}
 * \Reentrancy     {Reentrant}
 * \Synchronicity  {Synchronous}
 */
Std_ReturnType IoHwAb_Adc_GetValue_mV(uint16* mv);

#ifdef __cplusplus
}
#endif

#endif /* IOHWAB_H */
