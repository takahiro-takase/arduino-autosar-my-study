/**
 * \file    SchM.h
 * \brief   スケジュールマネージャ 排他エリアマクロ (AUTOSAR SWS_SchM 準拠)
 * \details AUTOSAR の各 BSW モジュールが共有リソースへのアクセスを保護するために
 *          使用する排他エリア（Exclusive Area）マクロを定義する。
 *
 *          命名規則:
 *            SchM_Enter_<Module>_<ExclusiveAreaName>()  — 排他エリア開始
 *            SchM_Exit_<Module>_<ExclusiveAreaName>()   — 排他エリア終了
 *
 *          AUTOSAR 実環境での実装:
 *            - 割り込みレベル保護 : SuspendAllInterrupts() / ResumeAllInterrupts()
 *            - タスクレベル保護   : GetResource() / ReleaseResource() (AUTOSAR OS)
 *          本実装（Arduino 協調スケジューリング）:
 *            - タスク間に先占が発生しないため NOP で十分。
 *              実際の RTOS 移植時はここを置き換えるだけでよい。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef SCHM_H
#define SCHM_H

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * Rte モジュール — Rte_EngineStateMirror 変数の保護
 *
 * 保護対象: Rte_EngineStateMirror (EngineState_t)
 * 書き込み: Rte_Write_EngineStatus_EngineState() — Task 2 (3000 ms)
 * 読み出し: Rte_Read_EngineStatus_EngineState()  — Task 3 (500 ms) / DCM
 *
 * Arduino 協調スケジューリングではタスク間の先占がないため NOP。
 * RTOS 環境では GetResource(RES_MIRROR) / ReleaseResource(RES_MIRROR) に相当。
 * ----------------------------------------------------------------------- */
#define SchM_Enter_Rte_MIRROR_EXCLUSIVE_AREA()  ((void)0)
#define SchM_Exit_Rte_MIRROR_EXCLUSIVE_AREA()   ((void)0)

/* -----------------------------------------------------------------------
 * Com モジュール — RX/TX シグナルバッファの保護
 *
 * 保護対象: Com 内部の RX/TX I-PDU バッファ
 * 書き込み: Com_ReceiveSignal() / Com_SendSignal() — 各タスク
 * 読み出し: 同上
 *
 * 同上の理由により NOP。
 * ----------------------------------------------------------------------- */
#define SchM_Enter_Com_SIGNAL_EXCLUSIVE_AREA()  ((void)0)
#define SchM_Exit_Com_SIGNAL_EXCLUSIVE_AREA()   ((void)0)

#ifdef __cplusplus
}
#endif

#endif /* SCHM_H */
