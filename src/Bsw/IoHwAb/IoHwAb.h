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
 * \brief   エンジン起動ボタンの押下状態を取得する。
 *
 * \details MCAL Dio_ReadChannel() で DIO_CHANNEL_BUTTON の物理レベルを読み取り、
 *          INPUT_PULLUP による論理反転（LOW=押下）を吸収して上位層に渡す。
 *          呼び出し元は物理的なプルアップ配線を意識しない。
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

#ifdef __cplusplus
}
#endif

#endif /* IOHWAB_H */
