/**
 * \file    EcuM.h
 * \brief   ECU ステートマネージャ 公開インタフェース (AUTOSAR SWS_EcuStateManager 準拠)
 * \details ECU 全体の起動シーケンスと周期処理をカプセル化する。
 *          実際の AUTOSAR EcuM は STARTUP / RUN / POST_RUN / SLEEP / SHUTDOWN
 *          の各フェーズを管理するが、本実装では Arduino 向けに
 *          EcuM_Init() と EcuM_MainFunction() の 2 関数に簡略化している。
 *          呼び出し側（main.cpp）は EcuM.h だけをインクルードすれば
 *          BSW の詳細を知らずにシステムを起動・運転できる。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef ECUM_H
#define ECUM_H

#include "Std_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   BSW スタック全体を起動フェーズ順に初期化する。
 *
 * \details AUTOSAR EcuM の StartupTwo フェーズに相当し、
 *          CAN ドライバ → CAN インタフェース → PDU ルータ →
 *          COM → RTE (SW-C 初期化) の順で各モジュールの _Init を呼び出す。
 *          Serial.begin() のような Arduino 固有の初期化は
 *          呼び出し側 setup() で事前に完了しておくこと。
 *
 * \pre        Arduino ランタイムが初期化済みであること（setup() の先頭で呼ぶ想定）。
 * \note       AUTOSAR EcuM では StartupOne (OS 起動前) と
 *             StartupTwo (OS 起動後) に分かれるが、本実装では一本化している。
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void EcuM_Init(void);

/**
 * \brief   BSW スタックの周期処理を実行する。
 *
 * \details CAN ISR ポーリング（Can_Isr）と RTE Runnable スケジューリング
 *          （Rte_ScheduleRunnables）を呼び出す。
 *          AUTOSAR OS 環境では OsTask として周期起動されるが、
 *          本実装では Arduino の loop() から毎ループ呼び出す。
 *
 * \pre        EcuM_Init() が正常完了していること。
 * \note       AUTOSAR 標準の EcuM_MainFunction は主に状態遷移管理を行うが、
 *             本実装では BSW ポーリングと RTE スケジューリングを担う。
 *
 * \ServiceID      {0x18}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void EcuM_MainFunction(void);

#ifdef __cplusplus
}
#endif

#endif /* ECUM_H */
