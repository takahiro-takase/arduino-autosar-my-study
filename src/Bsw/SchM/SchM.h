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
 *          本実装:
 *            Can 受信が MCP2515 の INT ピンによる真のハードウェア割り込み
 *            (Can_Isr(), attachInterrupt 経由) になったことで、メインループ
 *            (Os_SchedulerStep が呼ぶ各タスク) と割り込みコンテキストが
 *            実際にプリエンプトし合う関係になった。そのため、いずれかの
 *            排他エリアも「割り込みコンテキストから読み書きされ得る」ものは
 *            もう NOP では済まない。SchM_Hw_EnterExclusiveArea() /
 *            SchM_Hw_ExitExclusiveArea()（src/Hal/SchM_Hw.cpp）が実際に
 *            noInterrupts()/interrupts() を呼び出す。
 *            AVR・Renesas RA いずれも割り込み優先度は単一レベルのため、
 *            グローバル割り込み無効化 1 本で全排他エリアに対応できる
 *            （実車 AUTOSAR OS の SuspendAllInterrupts() と同じ考え方）。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef SCHM_H
#define SCHM_H

#include "SchM_Hw.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * Rte モジュール — RTE 内部ミラー変数の保護
 *
 * 保護対象: Rte_EngineStateMirror (EngineState_t)
 *           Rte_EngineInfoMirror / Rte_AbsInfoMirror
 *           （E2E Transformer 方式の RX ミラー。Rte.c 参照）
 * 書き込み: Rte_Write_EngineStatus_EngineState() — Task 2 (3000 ms)
 *           Rte_COMCbk_EngineInfo() / Rte_COMCbk_AbsInfo()
 *           — Can_MainFunction_Read() 経由（メインループのタスク）
 * 読み出し: Rte_Read_EngineStatus_EngineState()  — Task 3 (500 ms) / DCM
 *           Rte_Read_SpeedSensor_EngineSpeed() 等 — 各タスク
 *
 * いずれもメインループ内のタスクからのみ呼ばれ、割り込みコンテキストからは
 * 触れられない。真に先占され得るタスクはまだ存在しないため、現状は
 * 保護不要だが、実 RTOS 環境移植時に備え Rte.c 側は既に Enter/Exit で
 * くるんである。ここでは Can/Com と同じグローバル割り込み無効化を流用する。
 * ----------------------------------------------------------------------- */
#define SchM_Enter_Rte_MIRROR_EXCLUSIVE_AREA()  SchM_Hw_EnterExclusiveArea()
#define SchM_Exit_Rte_MIRROR_EXCLUSIVE_AREA()   SchM_Hw_ExitExclusiveArea()

/* -----------------------------------------------------------------------
 * Com モジュール — RX/TX シグナルバッファの保護
 *
 * 保護対象: Com 内部の RX/TX I-PDU バッファ
 * 書き込み: Com_ReceiveSignal() / Com_SendSignal() — 各タスク
 * 読み出し: 同上
 *
 * Com_RxIndication() は Can_MainFunction_Read()（メインループのタスク）
 * からのみ呼ばれ、割り込みコンテキストからは呼ばれない設計とした
 * （Can_Isr() 自体は SPI 通信や Serial ログを伴う重い処理をせず、
 * ペンディングフラグを立てるだけに留めている。理由は Can.c ファイル
 * 冒頭のコメントを参照）。そのため Com のバッファは実際には
 * まだ割り込みと競合しないが、Rte 側と同じ理由で Enter/Exit を
 * 用意しておく。
 * ----------------------------------------------------------------------- */
#define SchM_Enter_Com_SIGNAL_EXCLUSIVE_AREA()  SchM_Hw_EnterExclusiveArea()
#define SchM_Exit_Com_SIGNAL_EXCLUSIVE_AREA()   SchM_Hw_ExitExclusiveArea()

/* -----------------------------------------------------------------------
 * Can モジュール — 割り込みペンディングフラグの保護
 *
 * 保護対象: Can_RxIrqPending / Can_WakeupIrqPending (Can.c)
 * 書き込み: Can_Isr()（真のハードウェア割り込みコンテキスト、
 *           attachInterrupt 経由で INT ピンの立ち下がりで起動）
 * 読み出し・クリア: Can_MainFunction_Read() / Can_MainFunction_Wakeup()
 *           （メインループのタスク、1 ms 周期）
 *
 * この 2 つは実際にメインループと割り込みが競合し得る、本プロジェクトで
 * 唯一「NOP では正しく動かない」排他エリア。読み出し(フラグ確認)と
 * クリアを 1 つの排他エリアで囲わないと、その間に割り込みが発生した際に
 * フラグのセットが失われ、受信フレーム・ウェイクアップ通知を取りこぼす
 * （詳細は Can.c の Can_MainFunction_Read()/Can_MainFunction_Wakeup() を参照）。
 * ----------------------------------------------------------------------- */
#define SchM_Enter_Can_IRQFLAG_EXCLUSIVE_AREA()  SchM_Hw_EnterExclusiveArea()
#define SchM_Exit_Can_IRQFLAG_EXCLUSIVE_AREA()   SchM_Hw_ExitExclusiveArea()

#ifdef __cplusplus
}
#endif

#endif /* SCHM_H */
