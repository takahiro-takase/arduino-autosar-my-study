/**
 * \file    Os_Cfg.h
 * \brief   OS プリコンパイル設定 (AUTOSAR SWS_Os 準拠)
 * \details OS が管理するタスク数を定義する。
 *          実際の AUTOSAR 環境ではコンフィギュレーションツールが生成する
 *          ファイルに相当する。
 *
 * \copyright  Copyright (c) 2025 T_T
 * \license    MIT License - 詳細は LICENSE ファイルを参照。
 *
 * \note    本ファイルは AUTOSAR 4.3.1 仕様を参考にした学習用実装です。
 *          AUTOSAR 認証済み実装ではなく、製品への適用は想定していません。
 */
#ifndef OS_CFG_H
#define OS_CFG_H

/* ======================================================================
 * Includes
 * ====================================================================== */

/* ======================================================================
 * Definitions
 * ====================================================================== */

/* @@GEN-BEGIN os-tasks  生成元: config/data/Os.json  （自動生成: 手編集禁止） */
/** 管理タスク総数（Os.json の tasks の数。Os_PBCfg.c の Os_TaskTable の要素数と一致する） */
#define OS_TASK_COUNT  25U

/* シンボリック名: OsConf_OsTask_<function>（値はタスク表の添字 = タスク ID） */
/* cppcheck-suppress misra-c2012-2.5 */
#define OsConf_OsTask_Can_MainFunction_Read          0U
/* cppcheck-suppress misra-c2012-2.5 */
#define OsConf_OsTask_CanTp_MainFunction             1U
/* cppcheck-suppress misra-c2012-2.5 */
#define OsConf_OsTask_Rte_ScheduleRunnables          2U
/* cppcheck-suppress misra-c2012-2.5 */
#define OsConf_OsTask_Rte_ScheduleWarningIndicator   3U
/* cppcheck-suppress misra-c2012-2.5 */
#define OsConf_OsTask_CanSM_MainFunction             4U
/* cppcheck-suppress misra-c2012-2.5 */
#define OsConf_OsTask_Com_MainFunctionRx             5U
/* cppcheck-suppress misra-c2012-2.5 */
#define OsConf_OsTask_Com_MainFunctionRouteSignals   6U
/* cppcheck-suppress misra-c2012-2.5 */
#define OsConf_OsTask_Com_MainFunctionTx             7U
/* cppcheck-suppress misra-c2012-2.5 */
#define OsConf_OsTask_IoHwAb_MainFunction            8U
/* cppcheck-suppress misra-c2012-2.5 */
#define OsConf_OsTask_WdgM_MainFunction              9U
/* cppcheck-suppress misra-c2012-2.5 */
#define OsConf_OsTask_Dcm_MainFunction              10U
/* cppcheck-suppress misra-c2012-2.5 */
#define OsConf_OsTask_FiM_MainFunction              11U
/* cppcheck-suppress misra-c2012-2.5 */
#define OsConf_OsTask_CanNm_MainFunction            12U
/* cppcheck-suppress misra-c2012-2.5 */
#define OsConf_OsTask_NvM_MainFunction              13U
/* cppcheck-suppress misra-c2012-2.5 */
#define OsConf_OsTask_Can_MainFunction_Write        14U
/* cppcheck-suppress misra-c2012-2.5 */
#define OsConf_OsTask_Can_MainFunction_BusOff       15U
/* cppcheck-suppress misra-c2012-2.5 */
#define OsConf_OsTask_Can_MainFunction_Wakeup       16U
/* cppcheck-suppress misra-c2012-2.5 */
#define OsConf_OsTask_SecOC_MainFunctionTx          17U
/* cppcheck-suppress misra-c2012-2.5 */
#define OsConf_OsTask_MemIf_MainFunction            18U
/* cppcheck-suppress misra-c2012-2.5 */
#define OsConf_OsTask_App_GptDemo_Run               19U
/* cppcheck-suppress misra-c2012-2.5 */
#define OsConf_OsTask_ComM_MainFunction             20U
/* cppcheck-suppress misra-c2012-2.5 */
#define OsConf_OsTask_SecOC_MainFunctionRx          21U
/* cppcheck-suppress misra-c2012-2.5 */
#define OsConf_OsTask_App_Test_Run                  22U
/* cppcheck-suppress misra-c2012-2.5 */
#define OsConf_OsTask_CanTSyn_MainFunction          23U
/* cppcheck-suppress misra-c2012-2.5 */
#define OsConf_OsTask_StbM_MainFunction             24U
/* @@GEN-END os-tasks */

/** OsCounter の ID。本プロジェクトのカウンタは 1 つ（SYSTEM_COUNTER）で、
 *  1 tick = 1 ms、起動時の millis() と同じ座標系（Os.c 参照）。 */
#define SYSTEM_COUNTER  0U

#endif /* OS_CFG_H */
