/**
 * \file    EcuM.c
 * \brief   ECU ステートマネージャ (AUTOSAR SWS_EcuStateManager 準拠)
 * \details BSW スタックの起動シーケンスと周期処理をカプセル化する。
 *          main.cpp が個々の BSW モジュールを直接制御することなく、
 *          EcuM_Init() と EcuM_MainFunction() の 2 つの API だけで
 *          ECU を起動・運転できるようにする。
 *
 *          起動シーケンス (EcuM_Init):
 *            1. NvM_Init       — EEPROM → RAM ミラー一括ロード (最初期)
 *            2. Can_Init       — CAN コントローラ初期化
 *            3. Can_SetControllerMode(START) — CAN バス通信開始
 *            4. CanIf_Init     — CAN インタフェース初期化
 *            5. PduR_Init      — PDU ルータ初期化
 *            6. Com_Init       — COM モジュール初期化
 *            7. CanTp_Init     — CAN トランスポートプロトコル初期化
 *            8. Dcm_Init       — 診断通信モジュール初期化
 *            9. Dem_Init       — NvM 経由で DTC ステータスを復元
 *           10. App_EngineManager_Init — SW-C 初期化
 *           11. Os_Init        — タスクスケジューラ初期化 (全モジュール初期化後)
 *
 *          周期処理 (EcuM_MainFunction):
 *            Os_SchedulerStep() — タスクテーブルに従い周期到来タスクを実行
 *              Task 0: Can_Isr            (1 ms)    — CAN 受信ポーリング
 *              Task 1: CanTp_MainFunction (1 ms)    — タイムアウト監視・CF 送信
 *              Task 2: Rte_ScheduleRunnables (3000 ms) — エンジン Runnable 起動
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */

#include "EcuM.h"
#include "NvM.h"
#include "NvM_PBCfg.h"
#include "Os.h"
#include "Os_PBCfg.h"
#include "Can.h"
#include "Can_PBCfg.h"
#include "CanIf.h"
#include "CanIf_PBCfg.h"
#include "PduR.h"
#include "PduR_PBCfg.h"
#include "Com.h"
#include "Com_PBCfg.h"
#include "CanTp.h"
#include "Dcm.h"
#include "Dem.h"
#include "Rte.h"
#include "App_EngineManager.h"

/**
 * \brief   BSW スタック全体を起動フェーズ順に初期化する。
 *
 * \details AUTOSAR の依存関係順（下位層から上位層）に各モジュールの
 *          _Init 関数を呼び出す。
 *          - NvM_Init: 全 NvM ブロックを EEPROM から RAM ミラーへ一括ロードする。
 *            他モジュールより先に呼ぶことで、Dem_Init 時に NvM が使用可能になる。
 *          - Can_Init / Can_SetControllerMode: CAN コントローラを初期化し
 *            CAN バスへの送受信を開始する (CAN_T_START)。
 *          - CanIf_Init: CAN インタフェース層を初期化し、ルーティングテーブルを設定。
 *          - PduR_Init: PDU ルータを初期化し、RX/TX ルーティングパスを設定。
 *          - Com_Init: COM モジュールを初期化し、シグナル/I-PDU テーブルを設定。
 *          - CanTp_Init: トランスポートプロトコルを初期化し、RX/TX チャネルを IDLE に。
 *          - Dcm_Init: 診断セッションをデフォルトに初期化する。
 *          - Dem_Init: NvM 経由で前回の DTC ステータスを復元する。
 *          - App_EngineManager_Init: SW-C を初期化する。
 *
 * \pre        Arduino ランタイムが初期化済みであること（setup() の先頭で呼ぶ想定）。
 * \note       AUTOSAR EcuM では StartupOne (OS 起動前) と
 *             StartupTwo (OS 起動後) に分かれるが、本実装では一本化している。
 *
 * \ServiceID      {0x00}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void EcuM_Init(void)
{
    NvM_Init(&NvM_Config);
    Can_Init(&Can_Config);
    Can_SetControllerMode(0U, CAN_T_START);
    CanIf_Init(&CanIf_Config);
    PduR_Init(&PduR_Config);
    Com_Init(&Com_Config);
    CanTp_Init();
    Dcm_Init();
    Dem_Init();
    App_EngineManager_Init();
    Os_Init(&Os_Config);
}

/**
 * \brief   BSW スタックの周期処理を実行する。
 *
 * \details Arduino の loop() から毎ループ呼び出される。
 *          - Can_Isr: MCP2515 の INT ピンを確認し、受信フレームがあれば
 *            CanIf_RxIndication を通じて上位層へ配信する。
 *          - CanTp_MainFunction: タイムアウト監視と CF 送信タイミング管理を行う。
 *          - Rte_ScheduleRunnables: 経過時間を確認し、周期が来た
 *            SW-C Runnable (App_EngineManager_Run) を呼び出す。
 *
 * \pre        EcuM_Init() が正常完了していること。
 * \note       AUTOSAR 標準の EcuM_MainFunction は主に状態遷移管理を行うが、
 *             本実装では BSW ポーリングと RTE スケジューリングを担う。
 *
 * \ServiceID      {0x18}
 * \Reentrancy     {Non Reentrant}
 * \Synchronicity  {Synchronous}
 */
void EcuM_MainFunction(void)
{
    Os_SchedulerStep();
}
