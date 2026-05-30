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

/** 管理タスク総数 (Os_PBCfg.c の Os_TaskTable 要素数と一致させること) */
#define OS_TASK_COUNT  5U

#endif /* OS_CFG_H */
